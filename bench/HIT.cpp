//  abc/bench/HIT.cpp — BENCH-26B05APPSh: HITx Merge (HITb.c) of k sorted u64
//  runs vs std::merge (k = 2) and a std::priority_queue k-way merge.  The n
//  keys are distinct and dealt to runs at random; an item is one row out.
#include <benchmark/benchmark.h>

#include <algorithm>
#include <queue>
#include <vector>

#include "INT.h"
#include "PRO.h"

ABC_INIT
extern "C" ok64 hbMerge(u64 *out, size_t *len, u64cs *runs, size_t k);

static u64 mix(u64 x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

struct Runs {
    std::vector<std::vector<u64>> run;
    std::vector<u64> out;
    Runs(size_t n, size_t k) : run(k), out(n) {
        for (size_t i = 0; i < n; i++) run[mix(i + n) % k].push_back(mix(i));
        for (auto &r : run) std::sort(r.begin(), r.end());
    }
};

static void MergeAbc(benchmark::State &st) {
    Runs d(st.range(0), st.range(1));
    std::vector<u64cs> cs(d.run.size());
    for (auto _ : st) {
        for (size_t i = 0; i < cs.size(); i++)
            cs[i][0] = d.run[i].data(), cs[i][1] = cs[i][0] + d.run[i].size();
        size_t len = d.out.size();
        if (OK != hbMerge(d.out.data(), &len, cs.data(), cs.size()) ||
            len != d.out.size())
            st.SkipWithError("merge");
        benchmark::DoNotOptimize(d.out[len / 2]);
    }
    st.SetItemsProcessed((int64_t)(st.iterations() * d.out.size()));
}

static void MergeStd(benchmark::State &st) {
    Runs d(st.range(0), 2);
    for (auto _ : st) {
        std::merge(d.run[0].begin(), d.run[0].end(), d.run[1].begin(),
                   d.run[1].end(), d.out.begin());
        benchmark::DoNotOptimize(d.out[d.out.size() / 2]);
    }
    st.SetItemsProcessed((int64_t)(st.iterations() * d.out.size()));
}

//  The textbook k-way merge: a min-heap of (head, run).
static void MergeStdPQ(benchmark::State &st) {
    Runs d(st.range(0), st.range(1));
    typedef std::pair<u64, size_t> hd;
    std::vector<size_t> at(d.run.size());
    for (auto _ : st) {
        std::vector<hd> v;
        v.reserve(d.run.size());
        std::priority_queue<hd, std::vector<hd>, std::greater<hd>> q(
            std::greater<hd>(), std::move(v));
        for (size_t i = 0; i < d.run.size(); i++) {
            at[i] = 0;
            if (!d.run[i].empty()) q.push({d.run[i][0], i});
        }
        u64 *o = d.out.data();
        while (!q.empty()) {
            auto [key, i] = q.top();
            q.pop();
            *o++ = key;
            if (++at[i] < d.run[i].size()) q.push({d.run[i][at[i]], i});
        }
        benchmark::DoNotOptimize(d.out[d.out.size() / 2]);
    }
    st.SetItemsProcessed((int64_t)(st.iterations() * d.out.size()));
}

#define HM_ARGS ArgsProduct({{1 << 14, 1 << 22}, {2, 8, 32}})
BENCHMARK(MergeAbc)->HM_ARGS;
BENCHMARK(MergeStd)->Arg(1 << 14)->Arg(1 << 22);
BENCHMARK(MergeStdPQ)->HM_ARGS;

BENCHMARK_MAIN();
