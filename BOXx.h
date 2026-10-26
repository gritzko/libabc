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
//  Puts append to the segment until it is longer than c (BOXx_C bytes, 512).  Then
//  X(BOX,Sort): (1) stable-sort the segment and fold its own equal keys, the
//  segment becomes the youngest run; (2) select the tailing runs by the 1/8
//  ladder (HITLadderOverRuns); (3) one parallel pass over the selected runs,
//  in place: equal keys (one per run) are combined by BOXx_MERGE in run order
//  (else the youngest wins), the result goes into the last equal slot, the
//  others become the tombstone; (4) QSORTx the selected range ascending, the
//  tombstones sink to the tail; (5) trim them: the range is one run.
//
//  Tombstone: X(,max), the type's max record (u64max, kv64max...), a reserved
//  value no real record takes; X(,Z) must sort it last.  Trimmed by value.
//
//  Instantiate (Bx, QSORTx, HITx for T first; X(,Z) and X(,max) defined):
//      #define X(M, name) M##u64##name
//      #define BOXx_MERGE(acc, nxt) ...   // optional: fold younger *nxt into *acc
//      #define BOXx_C 512                 // optional: c, segment BYTES
//      #include "abc/BOXx.h"
//      #undef X

#include "B.h"
#include "BOX.h"
#include "HIT.h"
#include "OK.h"
#include "S.h"

#define BOX_T X(, )

#ifndef BOXx_C
#define BOXx_C 512  //  c: the unsorted segment's length in BYTES
#endif

//  The tombstone is the type's max record X(,max), compared by value.
fun b8 X(BOX, IsTomb)(BOX_T const *r) {
    BOX_T mx = X(, max);
    return memcmp((void const *)r, (void const *)&mx, sizeof(BOX_T)) == 0;
}

fun void X(BOX, Tomb)(BOX_T *r) {
    BOX_T mx = X(, max);
    *r = mx;
}

fun b8 X(BOX, Eq)(BOX_T const *a, BOX_T const *b) {
    return !X(, Z)(a, b) && !X(, Z)(b, a);
}

//  Fold adjacent equal keys of a sorted [lo,hi) into one; returns the end.
fun BOX_T *X(BOX, Dedup)(BOX_T *lo, BOX_T *hi) {
    if (hi - lo < 2) return hi;
    BOX_T *w = lo;
    for (BOX_T *r = lo + 1; r < hi; r++) {
        if (X(BOX, Eq)(w, r)) {
#ifdef BOXx_MERGE
            BOXx_MERGE(w, r);
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
fun BOX_T *X(BOX, Fold)(BOX_T *lo, BOX_T *hi) {
    if (hi - lo < 2) return hi;
    X(QSORT, InSort)(lo, hi);
    return X(BOX, Dedup)(lo, hi);
}

//  (3) the parallel in-place pass over runs[from..n): equal keys combined into
//  the youngest equal slot, the others tombstoned.
fun void X(BOX, Pass)(X($, c) *runs, size_t m) {
    BOX_T *cur[HIT_MAX_RUNS];
    for (size_t i = 0; i < m; i++) cur[i] = (BOX_T *)runs[i][0];
    for (;;) {
        BOX_T *min = NULL;
        for (size_t i = 0; i < m; i++)
            if (cur[i] < runs[i][1] && (!min || X(, Z)(cur[i], min)))
                min = cur[i];
        if (!min) break;
        BOX_T key = *min;
        BOX_T *last = NULL;
        for (size_t i = 0; i < m; i++) {
            if (cur[i] == runs[i][1] || !X(BOX, Eq)(cur[i], &key)) continue;
            BOX_T *at = cur[i]++;
            if (last) {
#ifdef BOXx_MERGE
                BOXx_MERGE(last, at);
                *at = *last;
#endif
                X(BOX, Tomb)(last);
            }
            last = at;
        }
    }
}

//  Seal the segment into a run, then collapse the tailing runs the 1/8 ladder
//  selects — or, with `all`, every run into one.  BOXFULL: no run slot left.
fun ok64 X(BOX, Sort)(X(, b) recs, X(, csb) runs, b8 all) {
    BOX_T **rb = (BOX_T **)recs;
    X($, c) **sb = (X($, c) **)runs;
    if (rb[2] > rb[1]) {
        if (sb[2] == sb[3]) return BOXFULL;
        BOX_T *end = X(BOX, Fold)(rb[1], rb[2]);
        (*sb[2])[0] = rb[1];
        (*sb[2])[1] = end;
        sb[2]++;
        rb[1] = rb[2] = end;
    }
    size_t n = (size_t)(sb[2] - sb[1]);
    if (n > HIT_MAX_RUNS) return HITTOOMANY;
    size_t m = n;
    if (!all) {
        a_pad(u64, lens, HIT_MAX_RUNS);
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
        X(BOX, Pass)(sel, m);
        X(, s) range = {lo, hi};
        X(, sSort)(range);
        while (hi > lo && X(BOX, IsTomb)(hi - 1)) hi--;
    }
    sel[0][1] = hi;
    sb[2] = sel + 1;
    rb[1] = rb[2] = hi;
    return OK;
}

//  Append one record; past c bytes the segment becomes a run (X(BOX,Sort)).
//  BOXFULL when the record buffer has no room — the caller seals or drops.
fun ok64 X(BOX, Put)(X(, b) recs, X(, csb) runs, BOX_T const *rec) {
    BOX_T **rb = (BOX_T **)recs;
    if (X(BOX, IsTomb)(rec)) return BOXFAIL;
    if (rb[2] == rb[3]) return BOXFULL;
    *rb[2]++ = *rec;
    if ((size_t)(rb[2] - rb[1]) * sizeof(BOX_T) <= BOXx_C) return OK;
    return X(BOX, Sort)(recs, runs, NO);
}

//  Append a batch as ONE run: seal the pending segment, qsort the batch in
//  place, fold its equal keys, then run the ladder once.  The sort is not
//  stable: equal keys INSIDE one batch fold in no particular order (the
//  merge op must not care, else one of them survives); across batches and
//  puts, younger still wins.  BOXFULL: no room; BOXFAIL: a tombstone row.
fun ok64 X(BOX, PutBatch)(X(, b) recs, X(, csb) runs, BOX_T const *rows,
                          size_t n) {
    BOX_T **rb = (BOX_T **)recs;
    X($, c) **sb = (X($, c) **)runs;
    if (n == 0) return OK;
    for (size_t i = 0; i < n; i++)
        if (X(BOX, IsTomb)(rows + i)) return BOXFAIL;
    if (rb[2] > rb[1]) {
        ok64 o = X(BOX, Sort)(recs, runs, NO);
        if (o != OK) return o;
    }
    if ((size_t)(rb[3] - rb[2]) < n || sb[2] == sb[3]) return BOXFULL;
    BOX_T *lo = rb[2];
    memcpy(lo, rows, n * sizeof(BOX_T));
    X(, s) batch = {lo, lo + n};
    X(, sSort)(batch);
    BOX_T *end = X(BOX, Dedup)(lo, lo + n);
    (*sb[2])[0] = lo;
    (*sb[2])[1] = end;
    sb[2]++;
    rb[1] = rb[2] = end;
    return X(BOX, Sort)(recs, runs, NO);
}

//  Point lookup: runs plus the segment, newest wins (or the BOXx_MERGE fold
//  in age order).  NONE when no record carries the key.
fun ok64 X(BOX, Get)(X(, b) recs, X(, csb) runs, BOX_T const *key,
                     BOX_T *out) {
    X($, c) **sb = (X($, c) **)runs;
    BOX_T const **rb = (BOX_T const **)recs;
#ifdef BOXx_MERGE
    b8 hit = NO;
    for (X($, c) *r = sb[1]; r < sb[2]; r++) {
        X(, sc) rr = {(BOX_T *)(*r)[0], (BOX_T *)(*r)[1]};
        BOX_T *p = X(, sBinSearch)(key, rr);
        if (!p) continue;
        if (hit) {
            BOXx_MERGE(out, p);
        } else {
            *out = *p;
            hit = YES;
        }
    }
    for (BOX_T const *p = rb[1]; p < rb[2]; p++) {
        if (!X(BOX, Eq)(p, key)) continue;
        if (hit) {
            BOXx_MERGE(out, p);
        } else {
            *out = *p;
            hit = YES;
        }
    }
    return hit ? OK : NONE;
#else
    for (BOX_T const *p = rb[2]; p > rb[1]; p--)
        if (X(BOX, Eq)(p - 1, key)) {
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
        if (w == into[1]) return BOXNOROOM;
        (*w)[0] = (*r)[0];
        (*w)[1] = (*r)[1];
        w++;
    }
    into[1] = w;
    o = X(HIT, SeekRange)(into, lo, hi);
    return o == NODATA ? OK : o;
}

#undef BOXx_MERGE
#undef BOXx_C
#undef BOX_T
