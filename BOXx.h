//  abc/BOXx.h — BOX-26A26EbjLj: an in-memory sorted container of fixed-size
//  records: a record buffer plus a buffer of slices, one per sorted run.
//
//  recs  X(,b)    the records (bMap the whole max range once):
//                 PAST = the sorted runs back to back, oldest first;
//                 DATA = the unsorted last segment the puts append to;
//                 IDLE = room.
//  runs  X(,csb)  DATA = one slice per run, oldest first; each run is
//                 ASCENDING by X(,Z) and holds at most one row per key.
//                 Fed to HITx as they are (DATA is an X(,css)).
//
//  Puts append to the segment until it is longer than c (ABC_BOX_SEG bytes,
//  512).  Then X(BOX,Sort): (1) stable-sort the segment and fold its own equal keys, the
//  segment becomes the youngest run; (2) select the tailing runs by the 1/8
//  ladder (HITLadderOverRuns); (3) one parallel pass over the selected runs,
//  in place: equal keys (one per run) are combined by X(,Merge) in run order
//  (else the youngest wins), the result goes into the last equal slot, the
//  others become the tombstone; (4) QSORTx the selected range ascending, the
//  tombstones sink to the tail; (5) trim them: the range is one run.
//
//  Tombstone: a X(,Max) row, the greatest key under X(,Z); no real record
//  carries that key (Put refuses it, BADARG).  Tested by key, trimmed last.
//
//  Instantiate (Bx, QSORTx, HITx for T first; X(,Z), X(,KeyEq), X(,Max)):
//      #define X(M, name) M##u64##name
//      #define ABC_BOX_MERGE      // optional: X(,Merge)(acc, younger) folds
//      #define ABC_BOX_SEG 512    // optional: c, segment BYTES
//      #include "abc/BOXx.h"
//      #undef X
//  Codes: OK, NONE, NOROOM (recs IDLE out), FULL (no run slot or more than
//  ABC_HIT_MAX_RUNS runs), BADARG (a Max-key row).  A failed write stores
//  nothing.  See BOX.md.

#include "B.h"
#include "BOX.h"
#include "HIT.h"
#include "OK.h"
#include "S.h"

#define BOX_T X(, )

#if defined(BOXx_C) || defined(BOXx_MERGE)
#error "BOXx_C, BOXx_MERGE are retired: ABC_BOX_SEG, ABC_BOX_MERGE + X(,Merge)"
#endif

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
#ifdef ABC_BOX_MERGE
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
#ifdef ABC_BOX_MERGE
                X(, Merge)(last, at);
                *at = *last;
#endif
                X(BOX, _tomb)(last);
            }
            last = at;
        }
    }
}

//  Seal the segment into a run, then collapse the tailing runs the 1/8 ladder
//  selects — or, with `all`, every run into one.  FULL: no run slot left or
//  more than ABC_HIT_MAX_RUNS runs; checked first, nothing changes then.
fun ok64 X(BOX, Sort)(X(, b) recs, X(, csb) runs, b8 all) {
    BOX_T **rb = (BOX_T **)recs;
    X($, c) **sb = (X($, c) **)runs;
    b8 seg = rb[2] > rb[1];
    if (seg && sb[2] == sb[3]) return FULL;
    if ((size_t)(sb[2] - sb[1]) + seg > ABC_HIT_MAX_RUNS) return FULL;
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
        for (X($, c) *r = sb[1]; r < sb[2]; r++)
            u64bFeed1(lens, (u64)((*r)[1] - (*r)[0]));
        m = HITLadderOverRuns(u64bDataC(lens));
    }
    if (m < 2) return OK;
    X($, c) *sel = sb[2] - m;
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
//  BADARG: a Max-key row; NOROOM: recs has no IDLE; FULL: the seal finds no
//  run slot.  On an error nothing is stored.
fun ok64 X(BOX, Put)(X(, b) recs, X(, csb) runs, BOX_T const *rec) {
    BOX_T **rb = (BOX_T **)recs;
    if (X(BOX, _isMax)(rec)) return BADARG;
    if (rb[2] == rb[3]) return NOROOM;
    *rb[2]++ = *rec;
    if ((size_t)(rb[2] - rb[1]) * sizeof(BOX_T) <= ABC_BOX_SEG) return OK;
    ok64 o = X(BOX, Sort)(recs, runs, NO);
    if (o != OK) rb[2]--;  //  Sort fails before it changes anything
    return o;
}

//  Append a batch as ONE run: seal the pending segment, qsort the batch in
//  place, fold its equal keys, then run the ladder once.  The sort is not
//  stable: equal keys INSIDE one batch fold in no particular order (the
//  merge op must not care, else one of them survives); across batches and
//  puts, younger still wins.  NOROOM: no recs room; FULL: no run slot;
//  BADARG: a Max-key row.  On an error no row of the batch is stored.
fun ok64 X(BOX, PutBatch)(X(, b) recs, X(, csb) runs, BOX_T const *rows,
                          size_t n) {
    BOX_T **rb = (BOX_T **)recs;
    X($, c) **sb = (X($, c) **)runs;
    if (n == 0) return OK;
    for (size_t i = 0; i < n; i++)
        if (X(BOX, _isMax)(rows + i)) return BADARG;
    if (rb[2] > rb[1]) {
        ok64 o = X(BOX, Sort)(recs, runs, NO);
        if (o != OK) return o;
    }
    if ((size_t)(rb[3] - rb[2]) < n) return NOROOM;
    if (sb[2] == sb[3] || (size_t)(sb[2] - sb[1]) >= ABC_HIT_MAX_RUNS)
        return FULL;
    BOX_T *lo = rb[2];
    memcpy(lo, rows, n * sizeof(BOX_T));
    X(, s) batch = {lo, lo + n};
    X(, sSort)(batch);
    BOX_T *end = X(BOX, _dedup)(lo, lo + n);
    (*sb[2])[0] = lo;
    (*sb[2])[1] = end;
    sb[2]++;
    rb[1] = rb[2] = end;
    return X(BOX, Sort)(recs, runs, NO);
}

//  Point lookup: runs plus the segment, newest wins (or the X(,Merge) fold
//  in age order).  NONE when no record carries the key.
fun ok64 X(BOX, Get)(BOX_T *out, X(, b) recs, X(, csb) runs,
                     BOX_T const *key) {
    X($, c) **sb = (X($, c) **)runs;
    BOX_T const **rb = (BOX_T const **)recs;
#ifdef ABC_BOX_MERGE
    b8 hit = NO;
    for (X($, c) *r = sb[1]; r < sb[2]; r++) {
        X(, sc) rr = {(BOX_T *)(*r)[0], (BOX_T *)(*r)[1]};
        BOX_T *p = X(, sBinSearch)(key, rr);
        if (!p) continue;
        if (hit) {
            X(, Merge)(out, p);
        } else {
            *out = *p;
            hit = YES;
        }
    }
    for (BOX_T const *p = rb[1]; p < rb[2]; p++) {
        if (!X(, KeyEq)(p, key)) continue;
        if (hit) {
            X(, Merge)(out, p);
        } else {
            *out = *p;
            hit = YES;
        }
    }
    return hit ? OK : NONE;
#else
    for (BOX_T const *p = rb[2]; p > rb[1]; p--)
        if (X(, KeyEq)(p - 1, key)) {
            *out = *(p - 1);
            return OK;
        }
    for (X($, c) *r = sb[2]; r > sb[1]; r--) {
        X(, sc) rr = {(BOX_T *)(*(r - 1))[0], (BOX_T *)(*(r - 1))[1]};
        BOX_T *p = X(, sBinSearch)(key, rr);
        if (p) {
            *out = *p;
            return OK;
        }
    }
    return NONE;
#endif
}

//  Range view: seal the segment into a run, then fill `into` (the caller's
//  slice array, at least the run count long) with every run trimmed to
//  [lo, hi), oldest first, empty ones dropped — a HITx input as it is.
fun ok64 X(BOX, Range)(X(, css) into, X(, b) recs, X(, csb) runs,
                       BOX_T const *lo, BOX_T const *hi) {
    ok64 o = X(BOX, Sort)(recs, runs, NO);
    if (o != OK) return o;
    X($, c) **sb = (X($, c) **)runs;
    X(, cs) *w = into[0];
    for (X($, c) *r = sb[1]; r < sb[2]; r++) {
        if (w == into[1]) return NOROOM;
        (*w)[0] = (*r)[0];
        (*w)[1] = (*r)[1];
        w++;
    }
    into[1] = w;
    o = X(HIT, SeekRange)(into, lo, hi);
    return o == NONE ? OK : o;
}

#undef ABC_BOX_MERGE
#undef ABC_BOX_SEG
#undef BOX_T
