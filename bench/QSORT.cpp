//  abc/bench/QSORT.cpp: QSORTx introsort vs qsort(3) and std::sort over u64;
//  BENCH-26B05APPSh adds std::sort and the presorted input (arg 1).
#include <benchmark/benchmark.h>

#include <algorithm>

extern "C" {
#include "INT.h"
}
#include "PRO.h"

ABC_INIT

// Fill buffer with pseudo-random data
static void fill_random(u64 *arr, size_t n, u64 seed) {
    for (size_t i = 0; i < n; i++) {
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        arr[i] = seed;
    }
}

static void fill_random_dups(u64 *arr, size_t n, u64 seed) {
    for (size_t i = 0; i < n; i++) {
        seed = seed * 6364136223846793005ULL + 1;
        arr[i] = seed % (n / 4 + 1);
    }
}

static void fill(u64 *arr, size_t n, int sorted) {
    if (!sorted) return fill_random(arr, n, 42);
    for (size_t i = 0; i < n; i++) arr[i] = i * 3;
}

// --- stdlib qsort (function pointer comparator) ---

static int u64cmp_local(const void *a, const void *b) {
    u64 ua = *(u64 const *)a, ub = *(u64 const *)b;
    return ua < ub ? -1 : (ua > ub ? 1 : 0);
}

static void BM_StdlibQSort(benchmark::State &state) {
    size_t n = (size_t)state.range(0);
    u64 *arr = new u64[n];
    for (auto _ : state) {
        state.PauseTiming();
        fill(arr, n, (int)state.range(1));
        state.ResumeTiming();
        qsort(arr, n, sizeof(u64), u64cmp_local);
        benchmark::DoNotOptimize(arr[0]);
    }
    delete[] arr;
    state.SetItemsProcessed((int64_t)state.iterations() * (int64_t)n);
}

// --- inline introsort (no function pointer) ---

static void BM_QSort(benchmark::State &state) {
    size_t n = (size_t)state.range(0);
    u64 *arr = new u64[n];
    for (auto _ : state) {
        state.PauseTiming();
        fill(arr, n, (int)state.range(1));
        state.ResumeTiming();
        u64s data = {arr, arr + n};
        u64sSort(data);
        benchmark::DoNotOptimize(arr[0]);
    }
    delete[] arr;
    state.SetItemsProcessed((int64_t)state.iterations() * (int64_t)n);
}

static void BM_StdSort(benchmark::State &state) {
    size_t n = (size_t)state.range(0);
    u64 *arr = new u64[n];
    for (auto _ : state) {
        state.PauseTiming();
        fill(arr, n, (int)state.range(1));
        state.ResumeTiming();
        std::sort(arr, arr + n);
        benchmark::DoNotOptimize(arr[0]);
    }
    delete[] arr;
    state.SetItemsProcessed((int64_t)state.iterations() * (int64_t)n);
}

// --- sort + dedup with duplicates ---

static void BM_StdlibSortDedup(benchmark::State &state) {
    size_t n = (size_t)state.range(0);
    u64 *arr = new u64[n];
    for (auto _ : state) {
        state.PauseTiming();
        fill_random_dups(arr, n, 42);
        state.ResumeTiming();
        qsort(arr, n, sizeof(u64), u64cmp_local);
        // manual dedup
        u64 *w = arr + 1;
        for (u64 *r = arr + 1; r < arr + n; r++)
            if (*r != *(r - 1)) *w++ = *r;
        benchmark::DoNotOptimize(w);
    }
    delete[] arr;
    state.SetItemsProcessed((int64_t)state.iterations() * (int64_t)n);
}

static void BM_QSortDedup(benchmark::State &state) {
    size_t n = (size_t)state.range(0);
    u64 *arr = new u64[n];
    for (auto _ : state) {
        state.PauseTiming();
        fill_random_dups(arr, n, 42);
        state.ResumeTiming();
        u64s data = {arr, arr + n};
        u64sSort(data);
        u64sDedup(data);
        benchmark::DoNotOptimize(data[1]);
    }
    delete[] arr;
    state.SetItemsProcessed((int64_t)state.iterations() * (int64_t)n);
}

#define QS_ARGS ArgsProduct({{1 << 12, 1 << 22}, {0, 1}})
BENCHMARK(BM_StdlibQSort)->QS_ARGS;
BENCHMARK(BM_QSort)->QS_ARGS;
BENCHMARK(BM_StdSort)->QS_ARGS;
BENCHMARK(BM_StdlibSortDedup)->Range(1 << 10, 1 << 20);
BENCHMARK(BM_QSortDedup)->Range(1 << 10, 1 << 20);

BENCHMARK_MAIN();
