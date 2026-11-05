//
// (c) Victor Grishchenko, 2020-2023
//
//  BENCH-26B05APPSh: SORT.c's k-way merge sort (Yx.h) vs std::stable_sort,
//  both stable and out of place, over random (arg 1 = 0) or presorted u64.
#include <benchmark/benchmark.h>

#include <algorithm>
#include <vector>

#include "01.h"

extern "C" {
#include "SORT.h"
}

uint8_t _pro_depth = 0;

static u64 val(size_t i, int sorted) {
    return sorted ? i * 3 : i * 0x9E3779B97F4A7C15ULL ^ i >> 7;
}

static void YSort(benchmark::State &state) {
    aB(u64, ints);
    aB(u64, ints2);
    size_t n = state.range(0);
    u64bAlloc(intsbuf, n);
    u64bAlloc(ints2buf, n);
    for (auto _ : state) {
        state.PauseTiming();
        Breset(intsbuf);
        Breset(ints2buf);
        for (size_t i = 0; i < n; ++i)
            u64sFeed1(intsidle, val(i, (int)state.range(1)));
        state.ResumeTiming();
        SORTu64(ints2idle, intsdata);
        assert(*$head(ints2data) <= *$last(ints2data));
        assert($len(ints2data) == n);
    }
    state.SetItemsProcessed((int64_t)(state.iterations() * n));
    u64bFree(intsbuf);
    u64bFree(ints2buf);
}

static void StdStable(benchmark::State &state) {
    size_t n = state.range(0);
    std::vector<u64> v(n);
    for (auto _ : state) {
        state.PauseTiming();
        for (size_t i = 0; i < n; ++i) v[i] = val(i, (int)state.range(1));
        state.ResumeTiming();
        std::stable_sort(v.begin(), v.end());
        benchmark::DoNotOptimize(v[0]);
    }
    state.SetItemsProcessed((int64_t)(state.iterations() * n));
}

BENCHMARK(YSort)->ArgsProduct({{1 << 12, 1 << 22}, {0, 1}});
BENCHMARK(StdStable)->ArgsProduct({{1 << 12, 1 << 22}, {0, 1}});

BENCHMARK_MAIN();
