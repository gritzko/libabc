//  abc/BOXx.h — BOX-26A26EbjLj: an in-memory sorted container of fixed-size
//  records, an LSM in one house (PUPS-26B045Ffpb, BOX.md "Layout"): the
//  memtable X(,b) holds the sorted runs in PAST, the unsorted segment in DATA;
//  the house DATA cells slice those runs, oldest first, fed to HITx as is.
//  Past ABC_BOX_SEG bytes, X(BOX,Sort) seals the segment: (1) stable-sort and
//  fold it, (2) pick the tail by the 1/8 ladder (HITLadderOverRuns), (3) one
//  in-place pass folds equal keys (X(,Merge), else the youngest wins), (4)
//  QSORTx the range, (5) trim the X(,Max) tombstones.  Codes, knobs: BOX.md.

#include "B.h"
#include "BOX.h"
#include "HIT.h"
#include "OK.h"
#include "S.h"

#if defined(BOXx_C) || defined(BOXx_MERGE) || defined(ABC_BOX_MERGE)
#error "BOXx_C, BOXx_MERGE, ABC_BOX_MERGE are retired: ABC_X_MERGE (HIT.h)"
#endif

//  PUPS-26B045Ffpb: BOX instantiates its HITx; the outermost header of the
//  instance drops the shared ABC_X_MERGE knob (HIT.h).
#ifndef ABC_X_NEST
#define ABC_X_NEST
#define BOX_X_OUTER
#endif
#include "HITx.h"

#define BOX_T X(, )

#ifndef ABC_BOX_SEG
#define ABC_BOX_SEG 512  //  c: the unsorted segment's length in BYTES
#endif
static_assert(ABC_BOX_SEG >= sizeof(BOX_T), "ABC_BOX_SEG: at least one record");

//  The tombstone is a X(,Max) row: the max key, tested by key via Z.
fun b8 X(BOX, _isMax)(BOX_T const *r) { return !X(, Z)(r, &X(, Max)); }

fun void X(BOX, _tomb)(BOX_T *r) { *r = X(, Max); }

//  Fold adjacent equal keys of a sorted [lo,hi) into one; returns the end.
fun BOX_T *X(BOX, _dedup)(BOX_T *lo, BOX_T *hi) {
    if (hi - lo < 2) return hi;
    BOX_T *w = lo;
    for (BOX_T *r = lo + 1; r < hi; r++) {
        if (X(, KeyEq)(w, r)) {
#ifdef ABC_X_MERGE
            X(, Merge)(w, r);
#else
            *w = *r;
#endif
        } else {
            *++w = *r;
        }
    }
    return w + 1;
}

//  (1) stable-sort the segment, fold equal keys in arrival order; returns the
//  folded end.
fun BOX_T *X(BOX, _fold)(BOX_T *lo, BOX_T *hi) {
    if (hi - lo < 2) return hi;
    X(QSORT, InSort)(lo, hi);
    return X(BOX, _dedup)(lo, hi);
}

//  (3) the parallel in-place pass over the selected runs, each run's head its
//  cursor (the runs are consumed): equal keys combined into the youngest equal
//  slot, the others tombstoned.
fun void X(BOX, _pass)(X(, css) sel) {
    for (;;) {
        BOX_T const *min = NULL;
        $for(X(, cs), r, sel)
            if (!$empty(*r) && (!min || X(, Z)((*r)[0], min))) min = (*r)[0];
        if (!min) break;
        BOX_T key = *min;
        BOX_T *last = NULL;
        $for(X(, cs), r, sel) {
            if ($empty(*r) || !X(, KeyEq)((*r)[0], &key)) continue;
            BOX_T *at = (BOX_T *)(*r)[0]++;
            if (last) {
#ifdef ABC_X_MERGE
                X(, Merge)(last, at);
                *at = *last;
#endif
                X(BOX, _tomb)(last);
            }
            last = at;
        }
    }
}

//  The memtable: the house's last two cells read as a X(,b), owner-mapped.
fun X(, ) **X(BOX, Mem)(X(, csb) h) { return (X(, ) **)h[3]; }

//  Seal the segment into a run, then collapse the tailing runs the 1/8 ladder
//  selects — or, with `all`, every run into one.  FULL: no free house cell;
//  checked first, nothing changes then.
fun ok64 X(BOX, Sort)(X(, csb) h, b8 all) {
    BOX_T **rb = X(BOX, Mem)(h);
    X(, cs) **sb = (X(, cs) **)h;
    b8 seg = rb[2] > rb[1];
    if (seg && sb[2] == sb[3]) return FULL;
    if (seg) {
        BOX_T *end = X(BOX, _fold)(rb[1], rb[2]);
        (*sb[2])[0] = rb[1];
        (*sb[2])[1] = end;
        sb[2]++;
        rb[1] = rb[2] = end;
    }
    size_t n = (size_t)(sb[2] - sb[1]);
    size_t m = n;
    if (!all) {
        a_pad(u64, lens, ABC_HIT_MAX_RUNS);
        for (X(, cs) *r = sb[1]; r < sb[2]; r++)
            u64bFeed1(lens, (u64)$len(*r));
        m = HITLadderOverRuns(u64bDataC(lens));
    }
    if (m < 2) return OK;
    X(, cs) *sel = sb[2] - m;
    BOX_T *lo = (BOX_T *)sel[0][0];
    BOX_T *hi = (BOX_T *)sel[m - 1][1];
    //  runs already in order (ascending keys): adjacent, no equal keys, done
    b8 ordered = YES;
    for (size_t i = 1; i < m && ordered; i++)
        ordered = X(, Z)(sel[i - 1][1] - 1, sel[i][0]);
    if (!ordered) {
        X(, css) sub = {sel, sb[2]};
        X(BOX, _pass)(sub);
        X(, s) range = {lo, hi};
        X(, sSort)(range);
        while (hi > lo && X(BOX, _isMax)(hi - 1)) hi--;
    }
    sel[0][0] = lo;
    sel[0][1] = hi;
    sb[2] = sel + 1;
    rb[1] = rb[2] = hi;
    return OK;
}

//  Append one record; past c bytes the segment becomes a run (X(BOX,Sort)).
//  BADARG: a Max-key row; NOROOM: the memtable has no IDLE; FULL: the seal
//  finds no house cell.  On an error nothing is stored.
fun ok64 X(BOX, Put)(X(, csb) h, BOX_T const *rec) {
    BOX_T **rb = X(BOX, Mem)(h);
    if (X(BOX, _isMax)(rec)) return BADARG;
    if (rb[2] == rb[3]) return NOROOM;
    *rb[2]++ = *rec;
    if ((size_t)(rb[2] - rb[1]) * sizeof(BOX_T) <= ABC_BOX_SEG) return OK;
    ok64 o = X(BOX, Sort)(h, NO);
    if (o != OK) rb[2]--;  //  Sort fails before it changes anything
    return o;
}

//  Append a batch as ONE run: seal the pending segment, qsort the batch in
//  place (NOT stable: equal keys inside a batch fold in no order, the merge op
//  must not care), fold it, then run the ladder once.  Codes as Put.
fun ok64 X(BOX, PutBatch)(X(, csb) h, X(, csc) rows) {
    BOX_T **rb = X(BOX, Mem)(h);
    X(, cs) **sb = (X(, cs) **)h;
    size_t n = (size_t)$len(rows);
    if (n == 0) return OK;
    $for(BOX_T const, r, rows) if (X(BOX, _isMax)(r)) return BADARG;
    if (rb[2] > rb[1]) {
        ok64 o = X(BOX, Sort)(h, NO);
        if (o != OK) return o;
    }
    if ((size_t)(rb[3] - rb[2]) < n) return NOROOM;
    if (sb[2] == sb[3]) return FULL;
    BOX_T *lo = rb[2];
    memcpy(lo, rows[0], n * sizeof(BOX_T));
    X(, s) batch = {lo, lo + n};
    X(, sSort)(batch);
    BOX_T *end = X(BOX, _dedup)(lo, lo + n);
    (*sb[2])[0] = lo;
    (*sb[2])[1] = end;
    sb[2]++;
    rb[1] = rb[2] = end;
    return X(BOX, Sort)(h, NO);
}

#ifdef ABC_X_MERGE
//  Fold one more (younger) version of the key into `out`.
fun void X(BOX, _add)(BOX_T *out, BOX_T const *p, b8 *hit) {
    if (*hit) X(, Merge)(out, p);
    else *out = *p;
    *hit = YES;
}

//  The X(,Merge) fold of the DATA runs and the segment onto `out`, oldest
//  first; PUP Get passes the PAST fold in.  YES when any row was folded.
fun b8 X(BOX, _getx)(BOX_T *out, X(, csb) h, BOX_T const *key, b8 hit) {
    BOX_T **rb = X(BOX, Mem)(h);
    for (X(, cs) const *r = h[1]; r < h[2]; r++) {
        X(, sc) rr = {(BOX_T *)(*r)[0], (BOX_T *)(*r)[1]};
        BOX_T const *p = X(, sBinSearch)(key, rr);
        if (p) X(BOX, _add)(out, p, &hit);
    }
    for (BOX_T const *p = rb[1]; p < rb[2]; p++)
        if (X(, KeyEq)(p, key)) X(BOX, _add)(out, p, &hit);
    return hit;
}
#endif

//  Point lookup: the house DATA runs plus the segment, newest wins (or the
//  X(,Merge) fold in age order); PAST is not BOX's.  NONE: no such key.
fun ok64 X(BOX, Get)(BOX_T *out, X(, csb) h, BOX_T const *key) {
#ifdef ABC_X_MERGE
    return X(BOX, _getx)(out, h, key, NO) ? OK : NONE;
#else
    BOX_T **rb = X(BOX, Mem)(h);
    for (BOX_T const *p = rb[2]; p > rb[1]; p--)
        if (X(, KeyEq)(p - 1, key)) {
            *out = *(p - 1);
            return OK;
        }
    for (X(, cs) const *r = h[2]; r > h[1]; r--) {
        X(, sc) rr = {(BOX_T *)r[-1][0], (BOX_T *)r[-1][1]};
        BOX_T const *p = X(, sBinSearch)(key, rr);
        if (p) {
            *out = *p;
            return OK;
        }
    }
    return NONE;
#endif
}

//  Range view: seal the segment into a run, then fill `into` (the caller's
//  slice array, at least the run count long) with every DATA run trimmed to
//  [lo, hi), oldest first, empty ones dropped — a HITx input as it is.
fun ok64 X(BOX, Range)(X(, css) into, X(, csb) h, BOX_T const *lo,
                       BOX_T const *hi) {
    ok64 o = X(BOX, Sort)(h, NO);
    if (o != OK) return o;
    X(, cs) *w = into[0];
    for (X(, cs) const *r = h[1]; r < h[2]; r++, w++) {
        if (w == into[1]) return NOROOM;
        X(, csMv)(*w, *r);
    }
    into[1] = w;
    o = X(HIT, SeekRange)(into, lo, hi);
    return o == NONE ? OK : o;
}

#ifdef BOX_X_OUTER
#undef BOX_X_OUTER
#undef ABC_X_NEST
#undef ABC_X_MERGE
#endif
#undef ABC_BOX_SEG
#undef BOX_T
