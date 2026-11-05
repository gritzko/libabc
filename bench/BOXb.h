//  abc/bench/BOXb.h — BENCH-26B0559PCK: the records and the C entry points
//  BOX.cpp benchmarks.  BOXx.h is C only, so BOXb.c instantiates one BOX per
//  record and wraps each op as a loop (no call per row); the comparator here
//  is the one std::set, the vector and the btree use too.
#ifndef ABC_BENCH_BOXB_H
#define ABC_BENCH_BOXB_H
#include "HIT.h"
#include "INT.h"

#ifdef __cplusplus
extern "C" {
#endif

//  The key leads, the payload pads the record to 4..64 bytes; pay[0] of a
//  zero-payload record does not exist, so e4 has no erase (no dead mark).
typedef struct e4 { u32 key; } e4;
typedef struct e8 { u32 key; u32 pay[1]; } e8;
typedef struct e16 { u64 key; u64 pay[1]; } e16;
typedef struct e32 { u64 key; u64 pay[3]; } e32;
typedef struct e64 { u64 key; u64 pay[7]; } e64;

#define BB_DEAD u64max  //  truncated to pay[0]; real payloads stay below 2^32

//  A house of 64 run cells, its last two the memtable buffer (BOX.md);
//  Put retries a NOROOM/FULL once after Sort(all).
typedef struct bbhouse {
    void const *cells[ABC_HIT_MAX_RUNS][2];
    void *h[4];
} bbhouse;

#define BB_KEY(T, ...)                                                   \
    fun b8 T##Z(T const *a, T const *b) { return a->key < b->key; }      \
    fun b8 T##KeyEq(T const *a, T const *b) { return a->key == b->key; } \
    con T T##Max = {__VA_ARGS__};

BB_KEY(e4, u32max)
BB_KEY(e8, u32max, {})
BB_KEY(e16, u64max, {})
BB_KEY(e32, u64max, {})
BB_KEY(e64, u64max, {})

//  The erase mark: e4 has no payload, so nothing is ever dead.
fun b8 bbe4_dead(e4 const *r) { return NO; }
#define BB_DEAD_FN(T) \
    fun b8 bb##T##_dead(T const *r) { return r->pay[0] == (typeof(r->pay[0]))BB_DEAD; }
BB_DEAD_FN(e8)
BB_DEAD_FN(e16)
BB_DEAD_FN(e32)
BB_DEAD_FN(e64)

//  Rows in, hits or rows out; `sum` folds what was read so nothing is elided.
//  Init maps room for `n` rows and their later puts; Mixed puts every 10th.
#define BB_DECL(P, H, T)                                              \
    ok64 P##T##Init(H *b, size_t n);                                  \
    void P##T##Free(H *b);                                            \
    ok64 P##T##Put(H *b, T const *rows, size_t n);                    \
    ok64 P##T##Compact(H *b);                                         \
    size_t P##T##Get(H *b, T const *keys, size_t n, u64 *sum);        \
    size_t P##T##Scan(H *b, T const *lo, T const *hi, u64 *sum);      \
    size_t P##T##Mixed(H *b, T const *rows, size_t n, u64 *sum);      \
    size_t P##T##Bytes(H const *b);                                   \
    size_t P##T##Runs(H const *b);

BB_DECL(bb, bbhouse, e4)
BB_DECL(bb, bbhouse, e8)
BB_DECL(bb, bbhouse, e16)
BB_DECL(bb, bbhouse, e32)
BB_DECL(bb, bbhouse, e64)

#ifdef __cplusplus
}
#endif
#endif
