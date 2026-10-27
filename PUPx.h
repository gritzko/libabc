//  PUPx.h — Pup template for one record type.
//
//  Instantiate with the record type:  #define X(M, name) M##u64##name
//                                     #include "abc/PUPx.h"
//                                     #undef X
//
//  Generates, for type T (names follow abc: X(PUP, Sync) is PUPu64Sync):
//    PUP<T>Sync, PUP<T>Merge    the whole-range functions the core PUPS.c calls
//    PUP<T>Put, PutMem, Commit, CommitAs, Ladder, AllRuns, AllRunsToc, Read
//                               thin typed entry points: the core's functions
//                               with sync, merge and sizeof(T) filled in, the
//                               memtable runs taken as the caller's X(,csb)
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

#include "FILE.h"
#include "HIT.h"
#include "PRO.h"
#include "PUPS.h"

//  PUPS-26A275OQK0: c, the memtable segment limit, is PUP_COLLAPSE (PUPS.h).
#define BOXx_C PUP_COLLAPSE
#include "BOXx.h"

#ifndef PUPx_DEAD
#define PUPx_DEAD(r) 0
#endif

//  BOX-26A26EbjLj: the memtable is a BOX — PAST its sorted runs, DATA the
//  unsorted segment.  PUPS-26A276Xh2e: the run slices are the caller's X(,csb),
//  which the untyped core passes through as a u8csb; cast once here.  Seal the
//  segment into a run and run the 1/8 ladder.  Equal keys: the youngest wins.
fun ok64 X(PUP, Sync)(u8bp mem, u8csb runs) {
    sane(u8bOK(mem) && runs != NULL);
    call(X(BOX, Sort), (X(, ) **)mem, (X($, c) **)runs, NO);
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
//  PUPS-26A276Xh2e: `runs` is the caller's X(,csb), held next to its pups.

fun ok64 X(PUP, Put)(pup256b pups, X(, csb) runs, path8s dir, u8cs ext,
                     u8csc rec) {
    return PUPPut(pups, (u8cs **)runs, dir, ext, rec, X(PUP, Sync),
                  X(PUP, Merge), (u32)sizeof(X(, )));
}

fun ok64 X(PUP, PutMem)(pup256b pups, X(, csb) runs, path8s dir, u8cs ext,
                        u8csc rec, u32 mem_pages) {
    return PUPPutMem(pups, (u8cs **)runs, dir, ext, rec, X(PUP, Sync),
                     X(PUP, Merge), (u32)sizeof(X(, )), mem_pages);
}

fun ok64 X(PUP, Commit)(pup256b pups, X(, csb) runs, path8s dir, u8cs ext) {
    return PUPCommit(pups, (u8cs **)runs, dir, ext, X(PUP, Sync),
                     X(PUP, Merge), (u32)sizeof(X(, )));
}

fun ok64 X(PUP, CommitAs)(pup256b pups, X(, csb) runs, path8s dir, u8cs ext,
                          b8 durable) {
    return PUPCommitAs(pups, (u8cs **)runs, dir, ext, X(PUP, Sync),
                       X(PUP, Merge), (u32)sizeof(X(, )), durable);
}

fun ok64 X(PUP, Ladder)(pup256b pups, path8s dir, u8cs ext) {
    return PUPLadder(pups, dir, ext, X(PUP, Merge), (u32)sizeof(X(, )));
}

fun ok64 X(PUP, AllRuns)(u8csb out, pup256b pups, X(, csb) runs) {
    return PUPAllRuns(out, pups, (u8cs **)runs, X(PUP, Sync),
                      (u32)sizeof(X(, )));
}

//  The merged view of the whole stack, newest row per key, dead rows skipped.
//  `runs` may be NULL for a stack that never had a memtable.
fun ok64 X(PUP, Read)(X(, s) into, pup256b pups, X(, csb) runs) {
    sane(into != NULL);
    Bu8cs srcs = {};
    call(u8csbAllocate, srcs, HIT_MAX_RUNS);
    try(X(PUP, AllRuns), srcs, pups, runs);
    nedo { u8csbFree(srcs); fail(__); }
    size_t n = u8csbDataLen(srcs);
    X(, cs) srcv[HIT_MAX_RUNS];
    u8cs *sl = u8csbDataHead(srcs);
    for (size_t i = 0; i < n; i++) {
        srcv[i][0] = (X(, ) const *)sl[i][0];
        srcv[i][1] = (X(, ) const *)sl[i][1];
    }
    X(, css) hp = {srcv, srcv + n};
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

fun ok64 X(PUP, AllRunsToc)(u8csb out, u8csb tocs, pup256b pups,
                            X(, csb) runs) {
    return PUPAllRunsToc(out, tocs, pups, (u8cs **)runs, X(PUP, Sync),
                         (u32)sizeof(X(, )));
}

#undef PUPx_DEAD
