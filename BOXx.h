//  abc/BOXx.h — BOX-26A26EbjLj: an in-memory sorted container of fixed-size
//  records, an LSM in one house (PUPS-26B045Ffpb, BOX.md "Layout"): the
//  memtable X(,b) holds the sorted runs in PAST, the sorted segment in DATA;
//  the house DATA cells slice those runs, oldest first, fed to HITx as is.
//  Puts insert into the segment, equal keys folded; past ABC_BOX_SEG bytes
//  X(BOX,Sort) seals it: (1) the segment becomes a run, (2) pick the tail by
//  the 1/8 ladder (HITLadderOverRuns), (3) HIT merge it into IDLE and back; no
//  room: an in-place pass folds equal keys, QSORTx sorts the range, the
//  X(,Max) tombstones trimmed.  Knobs: BOX.md.

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
#define ABC_BOX_SEG 512  //  c: the sorted segment's length in BYTES
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
        (*sb[2])[0] = rb[1];
        (*sb[2])[1] = rb[2];
        sb[2]++;
        rb[1] = rb[2];
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
    X(, css) sub = {sel, sb[2]};
    if (!ordered && (size_t)(rb[3] - rb[2]) >= (size_t)(hi - lo)) {
        //  PERF-26B057RaB9: room in IDLE, so a HIT merge there and back
        X(, s) out = {rb[2], rb[3]};
        X(HIT, Merge)(out, sub);
        size_t len = (size_t)(out[0] - rb[2]);
        memcpy(lo, rb[2], len * sizeof(BOX_T));
        hi = lo + len;
    } else if (!ordered) {
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

//  Insert one record into the sorted segment, an equal key folded in place
//  (PERF-26B057V5O9); past c bytes the segment becomes a run (X(BOX,Sort)).
//  BADARG: a Max-key row; NOROOM: the memtable has no IDLE; FULL: the seal
//  finds no house cell.  On an error nothing is stored.
fun ok64 X(BOX, Put)(X(, csb) h, BOX_T const *rec) {
    BOX_T **rb = X(BOX, Mem)(h);
    if (X(BOX, _isMax)(rec)) return BADARG;
    if (rb[2] == rb[3]) return NOROOM;
    X(, cs) **sb = (X(, cs) **)h;
    b8 seal = (size_t)(rb[2] + 1 - rb[1]) * sizeof(BOX_T) > ABC_BOX_SEG;
    if (seal && sb[2] == sb[3]) return FULL;
    //  an insertion-sort step (a backward memmove is slow on musl); an equal
    //  key, rare, folds and shifts the tail back
    BOX_T *p = rb[2];
    for (; p > rb[1] && X(, Z)(rec, p - 1); p--) *p = p[-1];
    if (p > rb[1] && X(, KeyEq)(p - 1, rec)) {
#ifdef ABC_X_MERGE
        X(, Merge)(p - 1, rec);
#else
        p[-1] = *rec;
#endif
        memmove(p, p + 1, (size_t)(rb[2] - p) * sizeof(BOX_T));
        return OK;
    }
    *p = *rec;
    rb[2]++;
    return seal ? X(BOX, Sort)(h, NO) : OK;
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

//  PERF-26B057ejg4: a sorted run's [first, last] cannot hold the key; two
//  compares instead of a binary search (clustered keys, near-disjoint runs).
fun b8 X(BOX, _out)(X(, cs) const r, BOX_T const *key) {
    return X(, Z)(key, r[0]) || X(, Z)(r[1] - 1, key);
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
        if (X(BOX, _out)(*r, key)) continue;
        X(, sc) rr = {(BOX_T *)(*r)[0], (BOX_T *)(*r)[1]};
        BOX_T const *p = X(, sBinSearch)(key, rr);
        if (p) X(BOX, _add)(out, p, &hit);
    }
    X(, sc) seg = {rb[1], rb[2]};
    BOX_T const *p = X(, sBinSearch)(key, seg);
    if (p) X(BOX, _add)(out, p, &hit);
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
    X(, sc) seg = {rb[1], rb[2]};
    BOX_T const *p = X(, sBinSearch)(key, seg);
    for (X(, cs) const *r = h[2]; !p && r > h[1]; r--) {
        if (X(BOX, _out)(r[-1], key)) continue;
        X(, sc) rr = {(BOX_T *)r[-1][0], (BOX_T *)r[-1][1]};
        p = X(, sBinSearch)(key, rr);
    }
    if (!p) return NONE;
    *out = *p;
    return OK;
#endif
}

//  Range view: seal the segment into a run, then fill `into` (the caller's
//  slice array, at least the run count long) with every DATA run trimmed to
//  [lo, hi), oldest first, empty ones dropped (disjoint ones unsearched,
//  PERF-26B057ejg4) — a HITx input as it is.
fun ok64 X(BOX, Range)(X(, css) into, X(, csb) h, BOX_T const *lo,
                       BOX_T const *hi) {
    BOX_T **rb = X(BOX, Mem)(h);
    //  PERF-26B0577XRz: no segment, no seal (the ladder held at the last one)
    ok64 o = rb[2] > rb[1] ? X(BOX, Sort)(h, NO) : OK;
    if (o != OK) return o;
    if (h[2] - h[1] > into[1] - into[0]) return NOROOM;
    X(, cs) *w = into[0];
    for (X(, cs) const *r = h[1]; r < h[2]; r++) {
        if (!X(, Z)((*r)[0], hi) || X(, Z)((*r)[1] - 1, lo)) continue;
        X(, s) sub;
        X(, sFindRange)(sub, *r, lo, hi);
        if (sub[0] == sub[1]) continue;
        (*w)[0] = sub[0];
        (*w)[1] = sub[1];
        w++;
    }
    into[1] = w;
    return OK;
}

#ifdef BOX_X_OUTER
#undef BOX_X_OUTER
#undef ABC_X_NEST
#undef ABC_X_MERGE
#endif
#undef ABC_BOX_SEG
#undef BOX_T
