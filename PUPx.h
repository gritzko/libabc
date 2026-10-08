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
//  Needs, instantiated BEFORE this header (abc already does kv64 and u64):
//    T, T##Z, the T##s / T##cs / T##css slices and T##b buffer (Bx),
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

#ifndef PUPx_WHOLE_ROW
#define PUPx_WHOLE_ROW 0
#endif

//  The memtable's typed sort/dedup/collapse: sort + keep-last dedup on DATA;
//  on `collapse`, fold PAST+DATA with the ties to DATA.
fun ok64 X(PUP, Sync)(u8bp mem, b8 collapse) {
    sane(u8bOK(mem));
    X(, ) *dh = (X(, ) *)mem[1];
    X(, ) *de = (X(, ) *)mem[2];
    b8 clean = YES;
    for (X(, ) *p = dh + 1; p < de; p++)
        if (!X(, Z)(p - 1, p)) {
            clean = NO;
            break;
        }
    if (!clean) {
        X(, s) d = {dh, de};
        //  One page of DATA keeps the stable insertion sort; a big memtable of
        //  a whole-row type takes the introsort.
        if (!PUPx_WHOLE_ROW ||
            (size_t)(de - dh) * sizeof(X(, )) <= FILESysPage())
            X(QSORT, InSort)(dh, de);
        else
            X(, sSort)(d);
        X(, sDedup)(d);
        call(u8bShed, mem, (size_t)(de - d[1]) * sizeof(X(, )));
    }
    if (!collapse) done;
    if (mem[0] == mem[1]) {
        call(u8bUsedAll, mem);
        done;
    }
    //  The scratch is the LIVE mapping's size, not a constant — the memtable is
    //  whatever the open sized it to.
    //  (a_carve, spelled out: it pastes its type argument, X() cannot be one)
    X(B, ) scr = {};
    must(ABC_BASS[1] == ABC_BASS[2], "PUPx sync in open gauge");
    call(X(, bAcquire), ABC_BASS, scr,
         (size_t)(u8bTerm(mem) - (u8 *)mem[0]) / sizeof(X(, )));
    X(, cs) runs[2] = {{(X(, ) const *)mem[0], (X(, ) const *)mem[1]},
                       {(X(, ) const *)mem[1], (X(, ) const *)mem[2]}};
    X(, css) hp = {runs, runs + 2};
    X(, ) *base = X(, bIdleHead)(scr);
    X(, s) into = {base, X(, bTerm)(scr)};
    call(X(HIT, Merge), hp, into);
    u8cs merged = {(u8c *)base, (u8c *)into[0]};
    u8bReset(mem);
    call(u8bFeed, mem, merged);
    call(u8bUsedAll, mem);
    done;
}

//  The ladder's typed merge over oldest-first run byte slices; equal rows go to
//  the youngest run.
fun ok64 X(PUP, Merge)(u8s into, u8css srcs) {
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

fun ok64 X(PUP, AllRunsToc)(u8csb out, u8csb tocs, kv64b pups) {
    return PUPAllRunsToc(out, tocs, pups, X(PUP, Sync), (u32)sizeof(X(, )));
}

#undef PUPx_WHOLE_ROW
