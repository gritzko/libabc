//  abc/bench/PUPb.h — BENCH-26B0559PCK: the PUPS entry points PUPS.cpp
//  benchmarks, BOXb.h's records and loops over a PUPS house whose runs are
//  files in a tmpfs directory (PUPS has no file-less mode), one per handle.
#ifndef ABC_BENCH_PUPB_H
#define ABC_BENCH_PUPB_H
#include "BOXb.h"

#ifdef __cplusplus
extern "C" {
#endif

//  The house as bbhouse, the file keys of its PAST and its run directory.
typedef struct pphouse {
    void const *cells[ABC_HIT_MAX_RUNS][2];
    void *h[4];
    u64 keyv[ABC_HIT_MAX_RUNS - 2];
    u64 *keys[4];
    char dir[256];
} pphouse;

BB_DECL(pp, pphouse, e4)
BB_DECL(pp, pphouse, e8)
BB_DECL(pp, pphouse, e16)
BB_DECL(pp, pphouse, e32)
BB_DECL(pp, pphouse, e64)

#ifdef __cplusplus
}
#endif
#endif
