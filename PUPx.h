//  PUPx.h — the Pup template for one record type T.  Instantiate as abc does:
//  `#define X(M, name) M##u64##name`, `#include "abc/PUPx.h"`, `#undef X`.
//  It generates PUP<T>Sync and PUP<T>Merge, the whole-range functions the
//  core PUPS.c calls, and the typed entry points (Put, PutMem, Commit,
//  CommitAs, Ladder, AllRuns, AllRunsToc, Read, *Opt per PUPS-26A309qbG9):
//  the core's functions with sync, merge and sizeof(T) filled in.  Needs T,
//  T##Z, T##KeyEq, T##Max and its Bx, QSORTx, HITx instantiated BEFORE it.
//  PUPx_DEAD(row), the optional tombstone test (PUPS-001): PUPS.md "Deletes".

#include "FILE.h"
#include "HIT.h"
#include "PRO.h"
#include "PUPS.h"

//  PUPS-26A275OQK0: c, the memtable segment limit, is PUP_COLLAPSE (PUPS.h).
#define ABC_BOX_SEG PUP_COLLAPSE
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
    test(Bfits(X(, ), mem), PUPBADARG);
    call(X(BOX, Sort), (X(, ) **)mem, (X($, c) **)runs, NO);
    done;
}

//  Feed typed views of the run byte slices `srcs` into `runs`, a X(,cs)
//  buffer; FULL past its room, PUPBADARG on a misaligned slice.
fun ok64 X(PUP, Runs)(X(, csb) runs, u8css srcs) {
    X(, cs) **b = (X(, cs) **)runs;
    $for(u8cs, s, srcs) {
        if (b[2] == b[3]) return FULL;
        if (!$fits(X(, ), *s)) return PUPBADARG;
        (*b[2])[0] = (X(, ) const *)(*s)[0];
        (*b[2])[1] = (X(, ) const *)(*s)[1];
        b[2]++;
    }
    return OK;
}

//  The ladder's typed merge over oldest-first run byte slices; equal rows go to
//  the youngest run.  `bottom`: the slices are the whole stack, dead rows go.
fun ok64 X(PUP, Merge)(u8s into, u8css srcs, b8 bottom) {
    sane($ok(into));
    test((uintptr_t)into[0] % alignof(X(, )) == 0, PUPBADARG);
    a_pad(X(, cs), runs, ABC_HIT_MAX_RUNS);
    call(X(PUP, Runs), runs, srcs);
    X(, ) *base = (X(, ) *)into[0];
    X(, s) dst = {base, base + $len(into) / sizeof(X(, ))};  // whole rows
    call(X(HIT, Merge), dst, runs_data);
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

//  PUPS-26A309qbG9: the same writes keyed and retired by the caller's pupopt.
fun ok64 X(PUP, PutOpt)(pup256b pups, X(, csb) runs, path8s dir, u8cs ext,
                        u8csc rec, u32 mem_pages, pupopt *opt) {
    return PUPPutOpt(pups, (u8cs **)runs, dir, ext, rec, X(PUP, Sync),
                     X(PUP, Merge), (u32)sizeof(X(, )), mem_pages, opt);
}

fun ok64 X(PUP, CommitOpt)(pup256b pups, X(, csb) runs, path8s dir, u8cs ext,
                           b8 durable, pupopt *opt) {
    return PUPCommitOpt(pups, (u8cs **)runs, dir, ext, X(PUP, Sync),
                        X(PUP, Merge), (u32)sizeof(X(, )), durable, opt);
}

fun ok64 X(PUP, LadderOpt)(pup256b pups, path8s dir, u8cs ext, pupopt *opt) {
    return PUPLadderOpt(pups, dir, ext, X(PUP, Merge), (u32)sizeof(X(, )),
                        opt);
}

fun ok64 X(PUP, AllRuns)(u8csb out, pup256b pups, X(, csb) runs) {
    return PUPAllRuns(out, pups, (u8cs **)runs, X(PUP, Sync),
                      (u32)sizeof(X(, )));
}

//  The merged view of the whole stack, newest row per key, dead rows skipped.
//  `runs` may be NULL for a stack that never had a memtable.
fun ok64 X(PUP, Read)(X(, s) into, pup256b pups, X(, csb) runs) {
    sane(into != NULL);
    a_pad(u8cs, srcs, ABC_HIT_MAX_RUNS);
    call(X(PUP, AllRuns), srcs, pups, runs);
    a_pad(X(, cs), srcv, ABC_HIT_MAX_RUNS);
    call(X(PUP, Runs), srcv, srcs_data);
    X(, ) *base = into[0];
    call(X(HIT, Merge), into, srcv_data);
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
