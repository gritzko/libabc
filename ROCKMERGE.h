#ifndef ABC_ROCKMERGE_H
#define ABC_ROCKMERGE_H

// Overflow-safe sizing helpers for the RocksDB merge operator glue in
// ROCK.c.  Kept rocksdb-free (no <rocksdb/c.h>) so the pure arithmetic
// can be unit-tested without a live RocksDB.  See MEM-012.

#include "01.h"
#include "BUF.h"
#include "OK.h"

// Overflow / out-of-range record count or output-buffer size estimate.
con ok64 ROCKHUGE = 0x6d831445e40e;

// Number of records that fit on the stack array in ROCKmerge_full.
con size_t ROCKMERGE_STACK = 64;

// Compute the total record count = num_operands + (has_existing?1:0) as a
// size_t, rejecting a negative or overflowing count.  RocksDB hands us
// num_operands as a signed int; a hostile / corrupt count must not wrap
// the selector that chooses the stack vs heap record array.
fun ok64 ROCKmergeTotal(int num_operands, b8 has_existing, size_t* out) {
    out[0] = 0;
    if (num_operands < 0) return ROCKHUGE;
    size_t total = (size_t)num_operands;
    if (has_existing) {
        if (total == SIZE_MAX) return ROCKHUGE;
        total += 1;
    }
    out[0] = total;
    return OK;
}

// Pick the stack array when the record count fits, heap otherwise.
fun b8 ROCKmergeOnStack(size_t total) { return total <= ROCKMERGE_STACK; }

// Estimate the output buffer capacity: 2 * sum(len of every record),
// guarding both the running sum and the doubling against size_t overflow.
// records is the slice of u8cs built from existing_value + operands.
fun ok64 ROCKmergeCap(u8css records, size_t* out) {
    out[0] = 0;
    size_t cap = 0;
    for (int i = 0; i < $len(records); i++) {
        size_t l = $len($at(records, i));
        if (cap > SIZE_MAX - l) return ROCKHUGE;
        cap += l;
    }
    if (cap == 0) cap = ROCKMERGE_STACK;
    if (cap > SIZE_MAX / 2) return ROCKHUGE;
    cap *= 2;
    out[0] = cap;
    return OK;
}

#endif  // ABC_ROCKMERGE_H
