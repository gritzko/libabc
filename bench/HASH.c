//  HASH-26B02GGjI3: HASHx perf bench, plain C.  ns/op, min of reps:
//  PutGrow from a 64-slot table, Get hit, Get miss, Move into a 2x
//  table, churn (Del + PutGrow), the load and memory PutGrow ends at.
//  The line is the library default; -DHB_LINE=16|64 pins it.
//  Build against any tree (its build/ done), no WITH_BENCH needed:
//    clang -O2 -std=gnu23 -I$T -I$T/ryu --rtlib=compiler-rt \
//          bench/HASH.c $T/build/libabc-core.a -o HASHbench
//  Run serialized: flock $D/tmp/bench.lock ./HASHbench [reps] [n...]
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "HASH.h"
#include "INT.h"
#include "KV.h"
#include "PRO.h"

#define HB_NOPS 5
#define HB_OPS (1u << 22)   // small tables loop to this many ops
#define HB_MAP (1ul << 27)  // slots reserved per table, never touched

static double hbnow() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}
static u64 hbmix(u64 x) {  // splitmix64 finalizer: a bijection
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

#define X(M, name) M##u64##name
#define ABC_HASH_CONVERGE 1
#ifdef HB_LINE
#define ABC_HASH_LINE HB_LINE
#endif
#include "HASHx.h"
#define T u64
fun void HBu64mk(size_t i, u64 *r) { *r = hbmix(i) | 1ull << 63; }
#define BNAME "u64c"
#include "HASHBx.h"
#undef T
#undef X

#define X(M, name) M##kv64##name
#ifdef HB_LINE
#define ABC_HASH_LINE HB_LINE
#endif
#include "HASHx.h"
#define T kv64
fun void HBkv64mk(size_t i, kv64 *r) { r->key = hbmix(i), r->val = i + 1; }
#define BNAME "kv64"
#include "HASHBx.h"
#undef T
#undef X

#define X(M, name) M##kv128##name
#ifdef HB_LINE
#define ABC_HASH_LINE HB_LINE
#endif
#include "HASHx.h"
#define T kv128
fun void HBkv128mk(size_t i, kv128 *r) {
    r->key._64[0] = hbmix(i), r->key._64[1] = hbmix(~i);
    r->val._64[0] = i + 1, r->val._64[1] = 0;
}
#define BNAME "kv128"
#include "HASHBx.h"
#undef T
#undef X

ok64 hashbench() {
    sane(1);
    int reps = 5;
    size_t ns[8] = {1u << 14, 1u << 22};
    size_t nn = 2;
    if ($arglen > 1) reps = atoi((char const *)$arg(1)[0]);
    if ($arglen > 2) {
        nn = 0;
        for (size_t a = 2; a < (size_t)$arglen && nn < 8; a++)
            ns[nn++] = strtoul((char const *)$arg(a)[0], 0, 0);
    }
    printf("%-10s %9s %6s %6s %6s %6s %6s %5s %7s\n", "type", "n", "put",
           "hit", "miss", "move", "churn", "load", "mem");
    for (size_t k = 0; k < nn; k++) {
        call(HBu64run, ns[k], reps);
        call(HBkv64run, ns[k], reps);
        call(HBkv128run, ns[k], reps);
    }
    done;
}
MAIN(hashbench);
