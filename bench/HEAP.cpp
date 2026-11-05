//  abc/bench/HEAP.cpp — BENCH-26B05APPSh: HEAPx (inline Z and a Z function
//  pointer) vs std::priority_queue and std::push_heap/pop_heap, min-heaps of
//  u32: push n random keys, pop them all; an item is one push and one pop.
#include <benchmark/benchmark.h>

#include <algorithm>
#include <queue>

#include "INT.h"
#include "PRO.h"

uint8_t _pro_depth = 0;

extern "C" {
#define X(M, name) M##u32##name
#include "../HEAPx.h"
#undef X
}

static u32 key(u64 i) { return (u32)(i * 0x9E3779B97F4A7C15ULL >> 32); }

template <bool FN>
static void HeapAbc(benchmark::State &st) {
    size_t n = st.range(0);
    u32 *b[4] = {};
    u32bMap(b, n);
    u64 sum = 0;
    for (auto _ : st) {
        for (size_t i = 0; i < n; i++)
            if (FN) HEAPu32Push1Z(b, key(i), u32Z);
            else HEAPu32Push1(b, key(i));
        for (size_t i = 0; i < n; i++) {
            u32 v;
            if (FN) HEAPu32PopZ(&v, b, u32Z);
            else HEAPu32Pop(&v, b);
            sum += v;
        }
    }
    benchmark::DoNotOptimize(sum);
    st.SetItemsProcessed((int64_t)(st.iterations() * n));
    u32bUnMap(b);
}

static void HeapStdPQ(benchmark::State &st) {
    size_t n = st.range(0);
    std::vector<u32> v;
    v.reserve(n);
    std::priority_queue<u32, std::vector<u32>, std::greater<u32>> q(
        std::greater<u32>(), std::move(v));
    u64 sum = 0;
    for (auto _ : st) {
        for (size_t i = 0; i < n; i++) q.push(key(i));
        for (size_t i = 0; i < n; i++) sum += q.top(), q.pop();
    }
    benchmark::DoNotOptimize(sum);
    st.SetItemsProcessed((int64_t)(st.iterations() * n));
}

static void HeapStdAlgo(benchmark::State &st) {
    size_t n = st.range(0);
    std::vector<u32> v;
    v.reserve(n);
    u64 sum = 0;
    for (auto _ : st) {
        for (size_t i = 0; i < n; i++) {
            v.push_back(key(i));
            std::push_heap(v.begin(), v.end(), std::greater<u32>());
        }
        for (size_t i = 0; i < n; i++) {
            std::pop_heap(v.begin(), v.end(), std::greater<u32>());
            sum += v.back();
            v.pop_back();
        }
    }
    benchmark::DoNotOptimize(sum);
    st.SetItemsProcessed((int64_t)(st.iterations() * n));
}

BENCHMARK(HeapAbc<false>)->Arg(1 << 10)->Arg(1 << 20);
BENCHMARK(HeapAbc<true>)->Arg(1 << 10)->Arg(1 << 20);
BENCHMARK(HeapStdPQ)->Arg(1 << 10)->Arg(1 << 20);
BENCHMARK(HeapStdAlgo)->Arg(1 << 10)->Arg(1 << 20);

BENCHMARK_MAIN();
