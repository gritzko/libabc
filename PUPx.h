//  PUPx.h — Pup template for one record type.
//
//  Instantiate with the record type:  #define X(M, name) M##u64##name
//                                     #include "abc/PUPx.h"
//                                     #undef X
//
//  Generates, for type T (names follow abc: X(PUP, Sync) is PUPu64Sync):
//    PUP<T>Sync, PUP<T>Merge    the whole-range functions the core PUPS.c calls
//    PUP<T>Put, PutMem, Commit, CommitAs, Ladder, AllRuns, AllRunsToc
//                               thin typed entry points: the core's functions
//                               with sync, merge and sizeof(T) filled in
//
//  PUPx_DEAD(row): optional tombstone test, a macro over a `T const *`.  Define
//  it before including when a row can say "this key is gone" (PUPS-001).  Reads
//  (PUP<T>Read) never surface a dead row, and a ladder merge that spans the
//  WHOLE stack drops dead rows; a partial merge keeps them, since an older run
//  may still hold the key.  Left undefined it is 0: nothing is ever dead.
//
//  Needs, instantiated BEFORE this header (abc already does kv64 and u64):
//    T, T##Z, T##max (BOX-26A26EbjLj: the tombstone, sorts last), the T##s / T##cs / T##css slices and T##b buffer (Bx),
//    QSORTx for T (QSORT<T>InSort, <T>sSort, <T>sDedup), HITx for T.
//
//  PUPx_WHOLE_ROW: define to 1 before including when <T>Z compares the WHOLE
//  row, so rows that compare equal are bit-identical and any sort will do.
//  Left undefined (the default), <T>Z may look at part of the row (a key), equal
//  rows may differ, and the memtable is sorted STABLY so the newest wins.

#include "FILE.h"
#include "HIT.h"
#include "PRO.h"
#include "PUPS.h"

#include "BOXx.h"

#ifndef PUPx_DEAD
#define PUPx_DEAD(r) 0
#endif

#ifndef PUPx_WHOLE_ROW
#define PUPx_WHOLE_ROW 0
#endif

//  BOX-26A26EbjLj: the memtable is a BOX over the mapping — PAST its sorted
//  runs, DATA the unsorted segment; the run slices are PUPMemRuns(mem).
//  Seal the segment into a run and run the 1/8 ladder; on `collapse`, fold
//  every run into one.  Equal keys: the youngest row wins.
fun ok64 X(PUP, Sync)(u8bp mem, b8 collapse) {
    sane(u8bOK(mem));
    u8cs *const *runs = PUPMemRuns(mem);
    test(runs != NULL, PUPFAIL);
    call(X(BOX, Sort), (X(, ) **)mem, (X($, c) **)runs, collapse);
    done;
}

//  The ladder's typed merge over oldest-first run byte slices; equal rows go to
//  the youngest run.  `bottom`: the slices are the whole stack, dead rows go.
fun ok64 X(PUP, Merge)(u8s into, u8css srcs, b8 bottom) {
    sane($ok(into));
    size_t n = (size_t)$len(srcs);
    if (n > HIT_MAX_RUNS) return HITTOOMANY;
    X(, cs) runs[HIT_MAX_RUNS];
    for (size_t i = 0; i < n; i++) {
        runs[i][0] = (X(, ) const *)srcs[0][i][0];
        runs[i][1] = (X(, ) const *)srcs[0][i][1];
    }
    X(, css) hp = {runs, runs + n};
    X(, ) *base = (X(, ) *)into[0];
    X(, s) dst = {base, (X(, ) *)into[1]};
    call(X(HIT, Merge), hp, dst);
    if (bottom) {
        X(, ) *w = base;
        for (X(, ) *r = base; r < dst[0]; r++)
            if (!PUPx_DEAD(r)) *w++ = *r;
        dst[0] = w;
    }
    into[0] = (u8 *)dst[0];
    done;
}

//  --- typed entry points -----------------------------------------------------

fun ok64 X(PUP, Put)(kv64b pups, path8s dir, u8cs ext, u8csc rec) {
    return PUPPut(pups, dir, ext, rec, X(PUP, Sync), X(PUP, Merge),
                  (u32)sizeof(X(, )));
}

fun ok64 X(PUP, PutMem)(kv64b pups, path8s dir, u8cs ext, u8csc rec,
                        u32 mem_pages) {
    return PUPPutMem(pups, dir, ext, rec, X(PUP, Sync), X(PUP, Merge),
                     (u32)sizeof(X(, )), mem_pages);
}

fun ok64 X(PUP, Commit)(kv64b pups, path8s dir, u8cs ext) {
    return PUPCommit(pups, dir, ext, X(PUP, Sync), X(PUP, Merge),
                     (u32)sizeof(X(, )));
}

fun ok64 X(PUP, CommitAs)(kv64b pups, path8s dir, u8cs ext, b8 durable) {
    return PUPCommitAs(pups, dir, ext, X(PUP, Sync), X(PUP, Merge),
                       (u32)sizeof(X(, )), durable);
}

fun ok64 X(PUP, Ladder)(kv64b pups, path8s dir, u8cs ext) {
    return PUPLadder(pups, dir, ext, X(PUP, Merge), (u32)sizeof(X(, )));
}

fun ok64 X(PUP, AllRuns)(u8csb out, kv64b pups) {
    return PUPAllRuns(out, pups, X(PUP, Sync), (u32)sizeof(X(, )));
}

//  The merged view of the whole stack, newest row per key, dead rows skipped.
fun ok64 X(PUP, Read)(X(, s) into, kv64b pups) {
    sane(into != NULL);
    Bu8cs srcs = {};
    call(u8csbAllocate, srcs, HIT_MAX_RUNS);
    try(X(PUP, AllRuns), srcs, pups);
    nedo { u8csbFree(srcs); fail(__); }
    size_t n = u8csbDataLen(srcs);
    X(, cs) runs[HIT_MAX_RUNS];
    u8cs *sl = u8csbDataHead(srcs);
    for (size_t i = 0; i < n; i++) {
        runs[i][0] = (X(, ) const *)sl[i][0];
        runs[i][1] = (X(, ) const *)sl[i][1];
    }
    X(, css) hp = {runs, runs + n};
    X(, ) *base = into[0];
    try(X(HIT, Merge), hp, into);
    u8csbFree(srcs);
    nedo fail(__);
    X(, ) *w = base;
    for (X(, ) *r = base; r < into[0]; r++)
        if (!PUPx_DEAD(r)) *w++ = *r;
    into[0] = w;
    done;
}

fun ok64 X(PUP, AllRunsToc)(u8csb out, u8csb tocs, kv64b pups) {
    return PUPAllRunsToc(out, tocs, pups, X(PUP, Sync), (u32)sizeof(X(, )));
}

#undef PUPx_WHOLE_ROW
#undef PUPx_DEAD
