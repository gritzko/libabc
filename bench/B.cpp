//  abc/bench/B.cpp — BENCH-26B05APPSh: appending n u64 to a fresh buffer,
//  B (bMap reserves the max range once, memory/abc-reserve-mmap) vs a
//  std::vector growing by push_back, with and without reserve(n).
#include <benchmark/benchmark.h>

#include <vector>

#include "INT.h"
#include "PRO.h"

ABC_INIT

#define BB_MAX (1ULL << 28)  // u64 slots reserved, 2 GB of address space

static void FeedB(benchmark::State &st) {
    size_t n = st.range(0);
    for (auto _ : st) {
        u64 *b[4] = {};
        u64bMap(b, BB_MAX);
        for (size_t i = 0; i < n; i++) u64bFeed1(b, i);
        benchmark::DoNotOptimize(b[2][-1]);
        u64bUnMap(b);
    }
    st.SetItemsProcessed((int64_t)(st.iterations() * n));
}

template <bool RESERVE>
static void FeedVec(benchmark::State &st) {
    size_t n = st.range(0);
    for (auto _ : st) {
        std::vector<u64> v;
        if (RESERVE) v.reserve(n);
        for (size_t i = 0; i < n; i++) v.push_back(i);
        benchmark::DoNotOptimize(v.back());
    }
    st.SetItemsProcessed((int64_t)(st.iterations() * n));
}

BENCHMARK(FeedB)->Arg(1 << 12)->Arg(1 << 22);
BENCHMARK(FeedVec<false>)->Arg(1 << 12)->Arg(1 << 22);
BENCHMARK(FeedVec<true>)->Arg(1 << 12)->Arg(1 << 22);

BENCHMARK_MAIN();
