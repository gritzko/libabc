//  abc/bench/BOXb.hpp — BENCH-26B0559PCK: the harness BOX.cpp and PUPS.cpp
//  share: records, key sets, std::set, absl::btree_set (with ABC_HAVE_ABSL),
//  a sorted vector and an LSM adapter over the C side, every one sorted by
//  T##Z; the op benchmarks; one data set and one built container live at a
//  time, a container whose estimate exceeds MemAvailable is skipped.
#include <benchmark/benchmark.h>

#include <algorithm>
#include <functional>
#include <random>
#include <set>
#include <vector>
#ifdef ABC_HAVE_ABSL
#include <absl/container/btree_set.h>
#endif

#include "PUPb.h"

uint8_t _pro_depth = 0;
extern "C" {
thread_local u8 *ABC_BASS[4] = {};  //  PRO.h's scratch arena, PUPS calls use it
}

#define BB_PROBES (1 << 16)
#define BB_SCANS 1024
#define BB_BATCH 1024

//  The C entry points of one prefix (bb: BOX, pp: PUPS) for record T.
#define BB_TRAITS(S, P, H, T)                           \
    template <>                                         \
    struct S<T> {                                       \
        using House = H;                                \
        static constexpr auto Init = P##T##Init;        \
        static constexpr auto Free = P##T##Free;        \
        static constexpr auto Put = P##T##Put;          \
        static constexpr auto Compact = P##T##Compact;  \
        static constexpr auto Get = P##T##Get;          \
        static constexpr auto Scan = P##T##Scan;        \
        static constexpr auto Mixed = P##T##Mixed;      \
        static constexpr auto Bytes = P##T##Bytes;      \
        static constexpr auto Runs = P##T##Runs;        \
    };
#define BB_TRAITS_ALL(S, P, H) \
    template <class T>         \
    struct S;                  \
    BB_TRAITS(S, P, H, e4)     \
    BB_TRAITS(S, P, H, e8)     \
    BB_TRAITS(S, P, H, e16)    \
    BB_TRAITS(S, P, H, e32)    \
    BB_TRAITS(S, P, H, e64)
BB_TRAITS_ALL(BB, bb, bbhouse)
BB_TRAITS_ALL(PP, pp, pphouse)

template <class T>
struct Rec;
#define BB_REC(T)                                                  \
    template <>                                                    \
    struct Rec<T> {                                                \
        static inline T const Max = T##Max;                        \
        static b8 Z(T const *a, T const *b) { return T##Z(a, b); } \
    };
BB_REC(e4)
BB_REC(e8)
BB_REC(e16)
BB_REC(e32)
BB_REC(e64)

template <class T>
struct Less {
    bool operator()(T const &a, T const &b) const { return Rec<T>::Z(&a, &b); }
};

//  Bijective mixers: distinct x, distinct keys; even x present, odd x absent.
static u64 bbmix64(u64 x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
static u32 bbmix32(u32 x) {
    x = (x ^ (x >> 16)) * 0x85ebca6bU;
    x = (x ^ (x >> 13)) * 0xc2b2ae35U;
    return x ^ (x >> 16);
}

template <class T>
static T mk(u64 x, u64 v) {
    T r{};
    if constexpr (sizeof(r.key) == 4) r.key = bbmix32((u32)x);
    else r.key = bbmix64(x);
    //  the Max key is the tombstone: shift x by an even 2^31, out of range
    if (r.key == Rec<T>::Max.key) return mk<T>(x + (1ULL << 31), v);
    if constexpr (requires { r.pay; })
        for (auto &p : r.pay) p = (std::remove_reference_t<decltype(p)>)v;
    return r;
}

//  Slot 0 the data set, slot 1 the built container: one of each lives.
static std::function<void()> g_free[2];
static void const *g_owner[2];

static void drop(int slot) {
    if (g_free[slot]) g_free[slot]();
    g_free[slot] = nullptr;
    g_owner[slot] = nullptr;
}

//  `make` may refuse (NULL, no room): nothing is kept then.
template <class O>
static O *keep(int slot, u64 cfg, std::function<O *()> make) {
    static O *p = nullptr;
    static u64 at = 0;
    if (p && at == cfg && g_owner[slot] == &p) return p;
    drop(slot);
    if (slot == 0) drop(1);
    p = make();
    if (!p) return nullptr;
    at = cfg;
    g_owner[slot] = &p;
    g_free[slot] = [] {
        delete p;
        p = nullptr;
    };
    return p;
}

template <class T>
static u64 cfg(size_t n) {
    return (u64)sizeof(T) << 40 | n;
}

//  The rows in random or in sorted order (in place: one copy), the probes.
template <class T>
struct Data {
    std::vector<T> rows, hit, miss, lo, hi, mixed;
    bool sorted = false;
    explicit Data(size_t n) {
        rows.reserve(n);
        for (size_t i = 0; i < n; i++) rows.push_back(mk<T>(2 * i, i));
        u64 s = 7;
        auto draw = [&] { return (size_t)((s = bbmix64(s)) % n); };
        for (size_t i = 0; i < BB_PROBES; i++) {
            hit.push_back(rows[draw()]);
            miss.push_back(mk<T>(2 * draw() + 1, 0));
            size_t j = draw();
            mixed.push_back(mk<T>(2 * j, j + 1));
        }
        //  keys are uniform: 100 rows span 100/n of the key space
        u64 span = (u64)Rec<T>::Max.key / n * 100;
        for (size_t i = 0; i < BB_SCANS; i++) {
            T l = rows[draw()], h = l;
            h.key = l.key > Rec<T>::Max.key - span ? Rec<T>::Max.key
                                                    : l.key + span;
            lo.push_back(l);
            hi.push_back(h);
        }
    }
    std::vector<T> const &order(bool want) {
        if (want && !sorted) std::sort(rows.begin(), rows.end(), Less<T>());
        if (!want && sorted) std::shuffle(rows.begin(), rows.end(), std::mt19937_64(42));
        sorted = want;
        return rows;
    }
};

static u64 g_data = 0;

template <class T>
static Data<T> &data(size_t n) {
    g_data = cfg<T>(n);
    return *keep<Data<T>>(0, g_data, [n] { return new Data<T>(n); });
}

//  MemAvailable (/proc/meminfo) less a 1 GB margin: never swap.
static bool fits(size_t bytes) {
    FILE *f = fopen("/proc/meminfo", "r");
    size_t kb = 0;
    char line[128];
    while (f && fgets(line, sizeof line, f))
        if (sscanf(line, "MemAvailable: %zu kB", &kb) == 1) break;
    if (f) fclose(f);
    return bytes + (1ULL << 30) <= kb * 1024;
}

//  Room for container A of n rows, and for the data set unless it is built.
template <class A, class T>
static bool room(size_t n) {
    size_t need = A::est(n);
    if (g_data != cfg<T>(n)) need += n * sizeof(T);
    return fits(need);
}

//  A BOX or a PUPS house; ONE: Compact after the build (BOX Sort(all), PUPS
//  Commit).  The puts of an erase, a restore or a mix go in as they come.
template <class T, template <class> class F, bool ONE>
struct Lsm {
    typename F<T>::House h;
    static size_t est(size_t n) { return 2 * n * sizeof(T); }
    explicit Lsm(size_t n) {
        if (F<T>::Init(&h, n) != OK) abort();
    }
    ~Lsm() { F<T>::Free(&h); }
    void put(std::vector<T> const &v) {
        if (F<T>::Put(&h, v.data(), v.size()) != OK) abort();
    }
    void build(std::vector<T> const &v, bool) {
        put(v);
        if (ONE) F<T>::Compact(&h);
    }
    size_t get(std::vector<T> const &k, u64 *sum) {
        return F<T>::Get(&h, k.data(), k.size(), sum);
    }
    size_t scan(T const &lo, T const &hi, u64 *sum) {
        return F<T>::Scan(&h, &lo, &hi, sum);
    }
    size_t iter(u64 *sum) {
        T lo{};
        return F<T>::Scan(&h, &lo, &Rec<T>::Max, sum);
    }
    static constexpr bool can_erase = requires(T r) { r.pay; };
    void erase(std::vector<T> const &dead) { put(dead); }
    void restore(std::vector<T> const &live) { put(live); }
    size_t mixed(std::vector<T> const &v, u64 *sum) {
        return F<T>::Mixed(&h, v.data(), v.size(), sum);
    }
    size_t bytes() const { return F<T>::Bytes(&h); }
    double runs() const { return (double)F<T>::Runs(&h); }
};

//  Counts the bytes a node container asks for (malloc's own overhead aside).
template <class T>
struct Counted {
    using value_type = T;
    size_t *ctr;
    explicit Counted(size_t *c) : ctr(c) {}
    template <class U>
    Counted(Counted<U> const &o) : ctr(o.ctr) {}
    T *allocate(size_t n) {
        *ctr += n * sizeof(T);
        return std::allocator<T>().allocate(n);
    }
    void deallocate(T *p, size_t n) {
        *ctr -= n * sizeof(T);
        std::allocator<T>().deallocate(p, n);
    }
    template <class U>
    bool operator==(Counted<U> const &o) const { return ctr == o.ctr; }
};

template <class T, template <class...> class S>
struct Tree {
    size_t ctr = 0;
    S<T, Less<T>, Counted<T>> s{Less<T>(), Counted<T>(&ctr)};
    //  a std::set node: 3 links and a color, the row; malloc adds ~16 B
    static size_t est(size_t n) {
        if (std::is_same_v<S<T, Less<T>>, std::set<T, Less<T>>>)
            return n * ((32 + sizeof(T) + 7) / 8 * 8 + 16);
        return n * sizeof(T) * 3 / 2;
    }
    explicit Tree(size_t) {}
    void build(std::vector<T> const &v, bool sorted) {
        if (sorted)
            for (T const &r : v) s.insert(s.end(), r);
        else
            for (T const &r : v) s.insert(r);
    }
    size_t get(std::vector<T> const &k, u64 *sum) {
        size_t hits = 0;
        for (T const &r : k) {
            auto it = s.find(r);
            if (it == s.end()) continue;
            hits++;
            *sum += it->key;
        }
        return hits;
    }
    size_t scan(T const &lo, T const &hi, u64 *sum) {
        size_t rows = 0;
        for (auto it = s.lower_bound(lo); it != s.end() && it->key < hi.key;
             ++it, rows++)
            *sum += it->key;
        return rows;
    }
    size_t iter(u64 *sum) {
        for (T const &r : s) *sum += r.key;
        return s.size();
    }
    static constexpr bool can_erase = true;
    void erase(std::vector<T> const &k) {
        for (T const &r : k) s.erase(r);
    }
    void restore(std::vector<T> const &v) { build(v, false); }
    //  the payload is not the key: overwriting it in place keeps the order
    size_t mixed(std::vector<T> const &v, u64 *sum) {
        size_t hits = 0;
        for (size_t i = 0; i < v.size(); i++) {
            if (i % 10 == 9) {
                auto [it, ins] = s.insert(v[i]);
                if (!ins) const_cast<T &>(*it) = v[i];
            } else {
                auto it = s.find(v[i]);
                if (it == s.end()) continue;
                hits++;
                *sum += it->key;
            }
        }
        return hits;
    }
    size_t bytes() const { return ctr; }
    double runs() const { return 0; }
};

template <class T>
using Set = Tree<T, std::set>;
#ifdef ABC_HAVE_ABSL
template <class T>
using Btree = Tree<T, absl::btree_set>;
#endif

//  Built in bulk (append, sort), probed by lower_bound; no erase.
template <class T>
struct Vec {
    std::vector<T> v;
    static size_t est(size_t n) { return n * sizeof(T); }
    explicit Vec(size_t n) { v.reserve(n); }
    void build(std::vector<T> const &in, bool sorted) {
        v.insert(v.end(), in.begin(), in.end());
        if (!sorted) std::sort(v.begin(), v.end(), Less<T>());
    }
    typename std::vector<T>::iterator find(T const &r) {
        auto it = std::lower_bound(v.begin(), v.end(), r, Less<T>());
        return it != v.end() && it->key == r.key ? it : v.end();
    }
    size_t get(std::vector<T> const &k, u64 *sum) {
        size_t hits = 0;
        for (T const &r : k) {
            auto it = find(r);
            if (it == v.end()) continue;
            hits++;
            *sum += it->key;
        }
        return hits;
    }
    size_t scan(T const &lo, T const &hi, u64 *sum) {
        size_t rows = 0;
        for (auto it = std::lower_bound(v.begin(), v.end(), lo, Less<T>());
             it != v.end() && it->key < hi.key; ++it, rows++)
            *sum += it->key;
        return rows;
    }
    size_t iter(u64 *sum) {
        for (T const &r : v) *sum += r.key;
        return v.size();
    }
    static constexpr bool can_erase = false;
    void erase(std::vector<T> const &) {}
    void restore(std::vector<T> const &) {}
    //  mixed puts overwrite present keys only, so no insert shifts the tail
    size_t mixed(std::vector<T> const &k, u64 *sum) {
        size_t hits = 0;
        for (size_t i = 0; i < k.size(); i++) {
            auto it = find(k[i]);
            if (it == v.end()) continue;
            if (i % 10 == 9) {
                *it = k[i];
            } else {
                hits++;
                *sum += it->key;
            }
        }
        return hits;
    }
    size_t bytes() const { return v.capacity() * sizeof(T); }
    double runs() const { return 0; }
};

using benchmark::State;

template <class A, class T>
static A *built(size_t n) {
    return keep<A>(1, cfg<T>(n), [n]() -> A * {
        if (!room<A, T>(n)) return nullptr;
        A *a = new A(n);
        a->build(data<T>(n).order(false), false);
        return a;
    });
}

#define BB_BUILT(a)                           \
    A *a = built<A, T>(n);                    \
    if (!a) return st.SkipWithMessage("n/a (RAM)");

template <class A>
static void mem(State &st, A const &a, size_t n) {
    st.counters["B/entry"] = (double)a.bytes() / (double)n;
    st.counters["runs"] = a.runs();
}

template <class A, class T, bool SORTED>
static void Ins(State &st) {
    size_t n = (size_t)st.range(0);
    drop(1);
    if (!room<A, T>(n)) return st.SkipWithMessage("n/a (RAM)");
    std::vector<T> const &in = data<T>(n).order(SORTED);
    for (auto _ : st) {
        st.PauseTiming();
        A *a = new A(n);
        st.ResumeTiming();
        a->build(in, SORTED);
        st.PauseTiming();
        mem(st, *a, n);
        delete a;
        st.ResumeTiming();
    }
    st.SetItemsProcessed((int64_t)st.iterations() * (int64_t)n);
}

template <class A, class T, bool HIT>
static void Get(State &st) {
    size_t n = (size_t)st.range(0);
    BB_BUILT(a);
    Data<T> &d = data<T>(n);
    u64 sum = 0;
    size_t want = HIT ? d.hit.size() : 0;
    for (auto _ : st)
        if (a->get(HIT ? d.hit : d.miss, &sum) != want) abort();
    benchmark::DoNotOptimize(sum);
    mem(st, *a, n);
    st.SetItemsProcessed((int64_t)st.iterations() * BB_PROBES);
}

template <class A, class T>
static void Scan(State &st) {
    size_t n = (size_t)st.range(0);
    BB_BUILT(a);
    Data<T> &d = data<T>(n);
    u64 sum = 0;
    int64_t rows = 0;
    for (auto _ : st)
        for (size_t i = 0; i < BB_SCANS; i++)
            rows += (int64_t)a->scan(d.lo[i], d.hi[i], &sum);
    benchmark::DoNotOptimize(sum);
    st.counters["rows/scan"] =
        (double)rows / (double)(st.iterations() * BB_SCANS);
    st.SetItemsProcessed(rows);
}

template <class A, class T>
static void Iter(State &st) {
    size_t n = (size_t)st.range(0);
    BB_BUILT(a);
    u64 sum = 0;
    for (auto _ : st)
        if (a->iter(&sum) != n) abort();
    benchmark::DoNotOptimize(sum);
    st.SetItemsProcessed((int64_t)st.iterations() * (int64_t)n);
}

template <class A, class T>
static void Mixed(State &st) {
    size_t n = (size_t)st.range(0);
    BB_BUILT(a);
    Data<T> &d = data<T>(n);
    u64 sum = 0;
    for (auto _ : st) a->mixed(d.mixed, &sum);
    benchmark::DoNotOptimize(sum);
    mem(st, *a, n);
    st.SetItemsProcessed((int64_t)st.iterations() * BB_PROBES);
}

//  Erase a batch of present keys, restore them untimed; an LSM puts BB_DEAD
//  rows.  Last of a container's ops: it leaves the container changed.
template <class A, class T>
static void Erase(State &st) {
    size_t n = (size_t)st.range(0);
    if constexpr (!A::can_erase) {
        st.SkipWithMessage("no erase");
    } else {
        BB_BUILT(a);
        Data<T> &d = data<T>(n);
        size_t b = n < BB_BATCH ? n : BB_BATCH, at = 0;
        std::vector<T> dead(b), live(b);
        for (auto _ : st) {
            st.PauseTiming();
            for (size_t i = 0; i < b; i++, at = (at + 1) % n) {
                live[i] = dead[i] = d.rows[at];
                if constexpr (requires { dead[i].pay; })
                    dead[i].pay[0] = (decltype(+dead[i].pay[0]))BB_DEAD;
            }
            st.ResumeTiming();
            a->erase(dead);
            st.PauseTiming();
            a->restore(live);
            st.ResumeTiming();
        }
        mem(st, *a, n);
        st.SetItemsProcessed((int64_t)st.iterations() * (int64_t)b);
    }
}

//  One container's ops for record T, and its footprint estimate.
struct Sub {
    char const *name;
    size_t (*est)(size_t);
    void (*fn[8])(State &);
};

template <template <class> class A, class T>
static Sub sub(char const *name) {
    using C = A<T>;
    return {name, C::est,
            {Ins<C, T, false>, Ins<C, T, true>, Get<C, T, true>,
             Get<C, T, false>, Scan<C, T>, Iter<C, T>, Mixed<C, T>,
             Erase<C, T>}};
}

template <class T>
static std::vector<Sub> baselines() {
    std::vector<Sub> v = {sub<Set, T>("set"), sub<Vec, T>("vec")};
#ifdef ABC_HAVE_ABSL
    v.push_back(sub<Btree, T>("btree"));
#endif
    return v;
}

//  The inserts of every container first (one sort, one shuffle of the rows),
//  then each container's reads on one build; a container past `cap` bytes
//  is not registered.
static void reg_cfg(char const *rec, size_t n, std::vector<Sub> const &subs,
                    size_t cap) {
    static char const *ops[8] = {"ins_rand", "ins_sorted", "get_hit",
                                 "get_miss", "scan100",   "iter",
                                 "mixed",    "erase"};
    auto add = [&](Sub const &s, int op) {
        if (s.est(n) > cap) return;
        std::string name = std::string(rec) + "/" + ops[op] + "/" + s.name;
        benchmark::RegisterBenchmark(name, s.fn[op])->Arg((int64_t)n);
    };
    for (int op = 0; op < 2; op++)
        for (Sub const &s : subs) add(s, op);
    for (Sub const &s : subs)
        for (int op = 2; op < 8; op++) add(s, op);
}

static int bench_main(int argc, char **argv) {
    if (u8bMap(ABC_BASS, 1 << 26) != OK) return 1;
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
