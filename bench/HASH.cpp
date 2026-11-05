//  abc/bench/HASH.cpp — BENCH-26B05APPSh: HASHx (library defaults) vs
//  std::unordered_map and absl::flat_hash_map (ABC_HAVE_ABSL) over 8..64 B
//  {key, val} records: put into an empty table, get hit and miss in random
//  order, erase every key, iterate.  n = 2^12 sits in L2, 2^22 is past L3.
//  bench/HASH.c is the HASHx knob grid; this is the STL comparison.
#include <benchmark/benchmark.h>

#include <unordered_map>
#ifdef ABC_HAVE_ABSL
#include <absl/container/flat_hash_map.h>
#endif

#include "INT.h"
#include "PRO.h"

uint8_t _pro_depth = 0;
thread_local u8 *ABC_BASS[4] = {};  // PRO.h scratch, as fuzz/HASH.cpp

template <int P>
struct pay {
    u64 p[P];
};
//  A record: hb##B is B bytes, a nonzero key keeps it off the empty slot.
#define HB_REC(B, K, V)                                                    \
    typedef struct hb##B {                                                 \
        K key;                                                             \
        V val;                                                             \
    } hb##B;                                                               \
    fun u64 hb##B##Hash(hb##B const *r) { return mix64(r->key); }          \
    fun b8 hb##B##KeyEq(hb##B const *a, hb##B const *b) {                  \
        return a->key == b->key;                                           \
    }                                                                      \
    fun b8 hb##B##Z(hb##B const *a, hb##B const *b) {                      \
        return a->key < b->key;                                            \
    }
HB_REC(8, u32, u32)
HB_REC(16, u64, u64)
HB_REC(64, u64, pay<7>)

#define X(M, name) M##hb8##name
#include "Bx.h"
#include "HASHx.h"
#undef X
#define X(M, name) M##hb16##name
#include "Bx.h"
#include "HASHx.h"
#undef X
#define X(M, name) M##hb64##name
#include "Bx.h"
#include "HASHx.h"
#undef X

//  Bijections fixing 0: keys of i < n hit, of n <= i < 2n miss; never 0.
static u64 hbkey(u64 i, u64 *) {
    i = (i + 1 ^ (i + 1) >> 30) * 0xbf58476d1ce4e5b9ULL;
    i = (i ^ (i >> 27)) * 0x94d049bb133111ebULL;
    return i ^ (i >> 31);
}
static u32 hbkey(u64 i, u32 *) {
    u32 x = (u32)i + 1;
    x = (x ^ (x >> 16)) * 0x85ebca6bU;
    x = (x ^ (x >> 13)) * 0xc2b2ae35U;
    return x ^ (x >> 16);
}
template <class R>
static R mk(u64 i) {
    R r{};
    r.key = hbkey(i, &r.key);
    memset((void *)&r.val, 1, sizeof r.val);
    return r;
}
//  The j-th probe of n (a power of 2): an odd multiplier permutes 0..n-1.
static u64 perm(u64 j, u64 n) { return (j * 0x9E3779B97F4A7C15ULL) & (n - 1); }

#define HB_ABC(R)                                                          \
    struct abc_##R {                                                       \
        R *b[4] = {};                                                      \
        explicit abc_##R(size_t n) {                                       \
            R##bMap(b, 16 * n + 64);                                       \
            R##bFed(b, 64);                                                \
        }                                                                  \
        ~abc_##R() { R##bUnMap(b); }                                       \
        void put(R const &r) { HASH##R##PutGrow(b, &r); }                  \
        bool get(R &r) {                                                   \
            R *d[2] = {b[1], b[2]};                                        \
            return OK == HASH##R##Get(&r, d);                              \
        }                                                                  \
        void del(R const &r) {                                             \
            R *d[2] = {b[1], b[2]};                                        \
            HASH##R##Del(d, &r);                                           \
        }                                                                  \
        u64 iter() {                                                       \
            R *d[2] = {b[1], b[2]};                                        \
            u64 s = 0;                                                     \
            for (size_t i = 0; OK == HASH##R##Next(&i, d); ++i)            \
                s += d[0][i].key;                                          \
            return s;                                                      \
        }                                                                  \
    };
HB_ABC(hb8)
HB_ABC(hb16)
HB_ABC(hb64)

template <class M, class R>
struct ref {
    M m;
    explicit ref(size_t) {}
    void put(R const &r) { m.emplace(r.key, r.val); }
    bool get(R &r) {
        auto it = m.find(r.key);
        if (it == m.end()) return false;
        r.val = it->second;
        return true;
    }
    void del(R const &r) { m.erase(r.key); }
    u64 iter() {
        u64 s = 0;
        for (auto const &kv : m) s += kv.first;
        return s;
    }
};
template <class R>
using stdmap = ref<std::unordered_map<decltype(R::key), decltype(R::val)>, R>;
#ifdef ABC_HAVE_ABSL
template <class R>
using abslmap = ref<absl::flat_hash_map<decltype(R::key), decltype(R::val)>, R>;
#endif

enum { PUT, GET, NOT, DEL, ITER };
static char const *OPS[] = {"put", "hit", "miss", "del", "iter"};

//  put and del time n ops on a fresh table, the rest reuse one table.
template <class C, class R>
static void run(benchmark::State &st, int op, size_t n) {
    C *c = new C(n);
    for (size_t i = 0; i < n; i++) c->put(mk<R>(i));
    u64 sum = 0;
    for (auto _ : st) {
        if (op == PUT || op == DEL) {
            st.PauseTiming();
            delete c;
            c = new C(n);
            if (op == DEL)
                for (size_t i = 0; i < n; i++) c->put(mk<R>(i));
            st.ResumeTiming();
        }
        if (op == ITER) sum += c->iter();
        for (size_t j = 0; op != ITER && j < n; j++) {
            R r = mk<R>(perm(j, n) + (op == NOT ? n : 0));
            if (op == PUT) c->put(r);
            else if (op == DEL) c->del(r);
            else sum += c->get(r);
        }
    }
    if (op == GET && sum != n * st.iterations()) st.SkipWithError("miss");
    if (op == NOT && sum != 0) st.SkipWithError("hit");
    benchmark::DoNotOptimize(sum);
    st.SetItemsProcessed((int64_t)(st.iterations() * n));
    delete c;
}

template <template <class> class C, class R>
static void reg(char const *impl) {
    for (size_t lg : {12, 22})
        for (int op = PUT; op <= ITER; op++) {
            char name[64];
            snprintf(name, sizeof name, "%s/%s/e%zu/%zu", OPS[op], impl,
                     sizeof(R), (size_t)1 << lg);
            benchmark::RegisterBenchmark(name, run<C<R>, R>, op,
                                         (size_t)1 << lg);
        }
}
template <class R>
using abc = std::conditional_t<
    sizeof(R) == 8, abc_hb8,
    std::conditional_t<sizeof(R) == 16, abc_hb16, abc_hb64>>;

template <class R>
static void reg_all() {
    reg<abc, R>("abc");
    reg<stdmap, R>("std");
#ifdef ABC_HAVE_ABSL
    reg<abslmap, R>("absl");
#endif
}

int main(int argc, char **argv) {
    reg_all<hb8>();
    reg_all<hb16>();
    reg_all<hb64>();
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
}
