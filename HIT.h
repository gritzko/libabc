// HIT.h — the LSM ladder policy, comparator-free.
// DOG-027: the 1/8 rule reads run LENGTHS only, so it has no element type
// and no heap: `lens` are run lengths, oldest-first, in whatever unit the
// caller measures (elements for a HIT stack, bytes for dog's Pup files).
// HITx.h's templated Compact builds on this; a caller that only needs the
// ladder must NOT instantiate a bogus element-typed HIT to reach it.
#ifndef ABC_HIT_H
#define ABC_HIT_H

#include "01.h"
#include "INT.h"
#include "OK.h"

// DOG-027: ONE run cap and ONE ladder ratio for every entry point and both
// JS leaves.  Past the cap the stack is damaged: FULL, the caller re-derives.
#define ABC_HIT_MAX_RUNS 64
// PUPS-26B05AeY1d: a pup house refuses new runs at this many housed, so the
// memtable's 2 cells and 2 spare ones leave room to seal and to compact.
#define ABC_PUP_MAX_RUNS (ABC_HIT_MAX_RUNS - 4)
#define ABC_HIT_LADDER_DIV 8  //  1/8 size-tiered: run i+1 < run i / DIV

// PUPS-26B045Ffpb: ABC_X_MERGE, the one per-instance knob HITx, BOXx and PUPx
// read: equal keys fold by X(,Merge), oldest first; the outermost undefs it.

// PUPS-26B05Ei2nT: run i+1 is out of proportion with run i when it is not
// under 1/8 of it, or empty: an empty run only costs a cell, merging is free.
fun b8 HITLadderOut(u64 older, u64 newer) {
    return newer == 0 || newer * ABC_HIT_LADDER_DIV > older;
}

// YES when every newer run is under 1/8 of the one before it.
fun b8 HITLadderOK(u64csc lens) {
    i64 n = u64csLen(lens);
    for (i64 i = 0; i + 1 < n; i++)
        if (HITLadderOut($at(lens, i), $at(lens, i + 1))) return NO;
    return YES;
}

// How many youngest runs the ladder must collapse; 0 when compact already.
// PUPS-26B05Ei2nT: the fewest whose merge leaves the whole ladder compact.
fun size_t HITLadderOverRuns(u64csc lens) {
    i64 n = u64csLen(lens), v = n;  //  v: the oldest out-of-proportion pair
    for (i64 i = 0; i + 1 < n && v == n; i++)
        if (HITLadderOut($at(lens, i), $at(lens, i + 1))) v = i;
    if (v == n) return 0;
    u64 total = $at(lens, n - 1);
    for (i64 k = n - 2; k > 0; k--) {
        total += $at(lens, k);
        if (k <= v + 1 && !HITLadderOut($at(lens, k - 1), total))
            return (size_t)(n - k);
    }
    return (size_t)n;
}

#endif
