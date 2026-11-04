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
#define ABC_HIT_LADDER_DIV 8  //  1/8 size-tiered: run i+1 < run i / DIV

// PUPS-26B045Ffpb: ABC_X_MERGE, the one per-instance knob HITx, BOXx and PUPx
// read: equal keys fold by X(,Merge), oldest first; the outermost undefs it.

// YES when every newer run is under 1/8 of the one before it.
fun b8 HITLadderOK(u64csc lens) {
    i64 n = u64csLen(lens);
    for (i64 i = 0; i + 1 < n; i++)
        if ($at(lens, i + 1) * ABC_HIT_LADDER_DIV > $at(lens, i)) return NO;
    return YES;
}

// How many youngest runs the ladder must collapse; 0 when compact already.
fun size_t HITLadderOverRuns(u64csc lens) {
    i64 n = u64csLen(lens);
    if (n < 2) return 0;
    i64 m = 1;
    u64 total = $at(lens, n - 1);
    while (m < n && total * ABC_HIT_LADDER_DIV > $at(lens, n - 1 - m)) {
        total += $at(lens, n - 1 - m);
        m++;
    }
    return m < 2 ? 0 : (size_t)m;
}

#endif
