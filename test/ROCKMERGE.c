// MEM-012 repro: ROCKmerge_full's stack/heap selector must not be
// defeated by integer overflow.  These tests exercise the pure sizing
// helpers extracted into ROCKMERGE.h (rocksdb-free, runs without a live
// RocksDB).  The end-to-end overflow demonstration (ROCKtest_overflow)
// reproduces the stack buffer write past stack_recs[64] that the old
// signed `int total = num_operands + 1` allowed.

#include "ROCKMERGE.h"

#include <limits.h>

#include "PRO.h"
#include "TEST.h"

// --- ROCKmergeTotal: count must be size_t, never wrap the selector ---

typedef struct {
    int num_operands;
    b8 has_existing;
    ok64 want_ret;
    size_t want_total;
} TotalCase;

ok64 ROCKtest_total() {
    sane(1);
    TotalCase cases[] = {
        // normal small counts -> stack
        {0, NO, OK, 0},
        {0, YES, OK, 1},
        {3, NO, OK, 3},
        {3, YES, OK, 4},
        {64, NO, OK, 64},
        {63, YES, OK, 64},
        // just over the stack boundary -> heap
        {64, YES, OK, 65},
        {65, NO, OK, 65},
        // negative count is out of range, never trusted
        {-1, NO, ROCKHUGE, 0},
        {INT_MIN, NO, ROCKHUGE, 0},
        // the MEM-012 wrap: INT_MAX + existing wrapped signed `int total`
        // negative, so `total > 64` was false and the 64-slot stack array
        // was (mis)kept while the loop wrote INT_MAX entries.  size_t total
        // stays huge -> heap path (and the alloc guard catches it).
        {INT_MAX, NO, OK, (size_t)INT_MAX},
        {INT_MAX, YES, OK, (size_t)INT_MAX + 1},
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < n; i++) {
        TotalCase *c = &cases[i];
        size_t total = 12345;  // poison
        ok64 r = ROCKmergeTotal(c->num_operands, c->has_existing, &total);
        same(r, c->want_ret);
        if (r == OK) {
            same(total, c->want_total);
            // the selector must be consistent with the count
            same(ROCKmergeOnStack(total), total <= ROCKMERGE_STACK);
        } else {
            same(total, 0);  // on error, out is zeroed
        }
    }
    done;
}

// --- ROCKmergeCap: 2 * sum(lengths), overflow-guarded ---

ok64 ROCKtest_cap() {
    sane(1);
    // empty record set -> floor of ROCKMERGE_STACK, doubled
    u8cs none[1];
    u8css empty = {none, none};
    size_t cap = 1;
    same(ROCKmergeCap(empty, &cap), OK);
    same(cap, ROCKMERGE_STACK * 2);

    // a couple of small records -> 2 * (3 + 5)
    u8 a[3] = {1, 2, 3};
    u8 b[5] = {1, 2, 3, 4, 5};
    u8cs recs[2] = {{a, a + 3}, {b, b + 5}};
    u8css two = {recs, recs + 2};
    cap = 0;
    same(ROCKmergeCap(two, &cap), OK);
    same(cap, (size_t)(3 + 5) * 2);

    // a single record whose length already exceeds SIZE_MAX/2 must be
    // rejected by the doubling guard rather than feeding malloc a small,
    // wrapped capacity the merge callback would then overrun.
    u8 *base = (u8 *)0x1000;
    u8cs huge1[1] = {{base, base + (SIZE_MAX / 2) + 1}};
    u8css onehuge = {huge1, huge1 + 1};
    cap = 7;
    same(ROCKmergeCap(onehuge, &cap), ROCKHUGE);
    same(cap, 0);

    // two records whose lengths sum-overflow size_t must be rejected by
    // the running-sum guard.
    u8cs huge2[2] = {{base, base + (SIZE_MAX - 4)}, {base, base + 16}};
    u8css twohuge = {huge2, huge2 + 2};
    cap = 9;
    same(ROCKmergeCap(twohuge, &cap), ROCKHUGE);
    same(cap, 0);
    done;
}

// --- end-to-end: the selector must never route an oversize count to the
// 64-slot stack array.
//
// This mirrors the structure of ROCKmerge_full: derive the record count,
// pick stack vs heap, then write that many records into the chosen array.
// The trap reproduced here is MEM-012: with the old signed
// `int total = num_operands + (existing?1:0)`, a num_operands near INT_MAX
// wrapped `total` negative, so the `total > 64` guard was *false* and the
// 64-slot stack array stayed selected — yet the fill loop still ran for
// the (huge) real count, writing past stack_recs[64].  ASan aborts on the
// 65th stack write.  To keep the demonstration cheap and deterministic we
// only need the loop to step a few entries past the boundary, so we cap
// the number of actual writes; the *selector* (stack vs heap) is the thing
// under test, and the fix routes any oversize count to the heap.

static ok64 fill_records(int num_operands, b8 has_existing) {
    sane(1);
    size_t total = 0;
    if (ROCKmergeTotal(num_operands, has_existing, &total) != OK)
        return ROCKHUGE;  // negative / wrapped count rejected before any write
    u8cs stack_recs[ROCKMERGE_STACK];
    u8cs *recs = stack_recs;
    size_t cap;        // real capacity of `recs` in elements
    size_t writes;     // how many entries the (capped) fill loop writes
    if (ROCKmergeOnStack(total)) {
        cap = ROCKMERGE_STACK;
        // The production loop writes `total` records.  Cap the iteration a
        // few past the stack array's end so the demo is cheap yet still
        // steps out of bounds when the selector lied: a correct selector
        // guarantees total <= ROCKMERGE_STACK here, so this stays in
        // bounds; a defeated selector keeps the 64-slot array for an
        // oversize `total`, and writes past index 64 trip ASan (the
        // MEM-012 stack buffer overflow).
        writes = total < cap + 8 ? total : cap + 8;
    } else {
        if (total > SIZE_MAX / sizeof(u8cs)) return ROCKHUGE;
        // Cap the heap allocation so a legitimately-large count stays cheap
        // to test; production mallocs the full count (NULL -> *success=0).
        cap = total < 4096 ? total : 4096;
        recs = malloc(cap * sizeof(u8cs));
        if (recs == NULL) return ROCKHUGE;
        writes = cap;
    }
    u8 byte = 42;
    for (size_t i = 0; i < writes; i++) {
        recs[i][0] = &byte;
        recs[i][1] = &byte + 1;
    }
    if (recs != stack_recs) free(recs);
    done;
}

ok64 ROCKtest_overflow() {
    sane(1);
    // small counts: fit on the stack, fill cleanly
    call(fill_records, 0, NO);
    call(fill_records, 10, YES);
    call(fill_records, 64, NO);
    call(fill_records, 63, YES);  // total 64, still stack
    // just over the boundary: heap path, no stack overrun
    call(fill_records, 64, YES);   // total 65
    call(fill_records, 1000, NO);  // heap
    // the MEM-012 wrap: num_operands near INT_MAX.  With the fix this is a
    // huge size_t total -> heap path (no stack overrun).  With the old
    // signed arithmetic the wrap selected the stack array and overran it.
    call(fill_records, INT_MAX, YES);
    call(fill_records, INT_MAX, NO);
    // negative counts are rejected outright
    same(fill_records(-1, NO), ROCKHUGE);
    same(fill_records(INT_MIN, NO), ROCKHUGE);
    done;
}

ok64 maintest() {
    sane(1);
    call(ROCKtest_total);
    call(ROCKtest_cap);
    call(ROCKtest_overflow);
    done;
}

TEST(maintest)
