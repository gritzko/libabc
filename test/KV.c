//  KV-26A273cxFy: kv128 {u128 key; u128 val} as a HASH map, a BOX lane, QSORTed
#include "INT.h"
#include "KV.h"
#include "PRO.h"
#include "TEST.h"

#define X(M, name) M##kv128##name
#define ABC_HASH_LINE 16  // 16-slot tables below
#include "HASHx.h"
#include "BOXx.h"
#undef X

#define X(M, name) M##k96v32##name
#define ABC_HASH_LINE 16  // 16-slot tables below
#include "HASHx.h"
#include "BOXx.h"
#undef X

fun kv128 kv128of(u64 k, u64 v) {
    kv128 r = {.key = {._64 = {k, ~k}}, .val = {._64 = {v, v ^ k}}};
    return r;
}

//  map: same key, other value = one entry; values survive growth
ok64 KV128hash() {
    sane(1);
    Bkv128 buf = {};
    call(kv128bMap, buf, 1 << 20);
    call(kv128bFed, buf, 16);
    for (u64 k = 1; k <= 3000; ++k) {
        kv128 r = kv128of(k, k * 7);
        call(HASHkv128PutGrow, buf, &r);
    }
    kv128 r = kv128of(42, 1);
    call(HASHkv128PutGrow, buf, &r);
    size_t live = 0;
    $for(kv128, p, kv128bData(buf)) if (p->key._64[0]) ++live;
    testeqv((long long)live, 3000LL, "%lld");
    for (u64 k = 1; k <= 3000; ++k) {
        kv128 g = {.key = kv128of(k, 0).key};
        call(HASHkv128Get, &g, kv128bData(buf));
        kv128 w = kv128of(k, k == 42 ? 1 : k * 7);
        want(g.val._64[0] == w.val._64[0] && g.val._64[1] == w.val._64[1]);
    }
    kv128 miss = kv128of(5000, 0);
    want(HASHkv128Get(&miss, kv128bData(buf)) == HASHNONE);
    call(kv128bUnMap, buf);
    done;
}

//  BOX: the youngest row of a key wins
ok64 KV128box() {
    sane(1);
    a_box(kv128, h);
    kv128 **recs = BOXkv128Mem(h);
    call(kv128bMap, recs, 1 << 14);
    u64 last[100] = {};
    for (u64 i = 1; i <= 2000; i++) {
        u64 k = (i * 37) % 100;
        kv128 r = kv128of(k, i);
        last[k] = i;
        call(BOXkv128Put, h, &r);
    }
    for (u64 k = 0; k < 100; k++) {
        kv128 key = kv128of(k, 0), got = {};
        call(BOXkv128Get, &got, h, &key);
        want(got.val._64[0] == last[k]);
    }
    call(BOXkv128Sort, h, YES);
    want(h[2] - h[1] == 1);
    want(h[1][0][1] - h[1][0][0] == 100);
    for (kv128 const *p = h[1][0][0]; p < h[1][0][1]; p++)
        want(p->val._64[0] == last[p->key._64[0]]);
    kv128bUnMap(recs);
    done;
}

//  QSORT: by key only (high u64 first), stable on equal keys
ok64 KV128sort() {
    sane(1);
    kv128 arr[] = {
        {{{1, 2}}, {{0, 0}}}, {{{9, 1}}, {{1, 0}}}, {{{0, 2}}, {{2, 0}}},
        {{{1, 2}}, {{3, 0}}}, {{{5, 1}}, {{0, 4}}},
    };
    QSORTkv128InSort(arr, arr + 5);
    static u64 const keys[5][2] = {{5, 1}, {9, 1}, {0, 2}, {1, 2}, {1, 2}};
    for (int i = 0; i < 5; i++)
        want(arr[i].key._64[0] == keys[i][0] && arr[i].key._64[1] == keys[i][1]);
    want(arr[3].val._64[0] == 0 && arr[4].val._64[0] == 3);
    want(!kv128Z(&kv128Max, &arr[4]) && kv128Z(&arr[4], &kv128Max));
    done;
}

//  KV-26A274c3W6: k96v32, the key spread over both key words
fun k96v32 k96v32of(u64 k, u32 v) {
    k96v32 r = {.k0 = k * 0x9e3779b97f4a7c15ULL, .k1 = (u32)(k >> 3) ^ 0x5a5a, .val = v};
    return r;
}

//  map: same key, other value = one entry; Get fills the value; PutGrow
//  keeps every entry across growth; keys equal in k0 or in k1 alone differ
ok64 K96V32hash() {
    sane(1);
    Bk96v32 buf = {};
    call(k96v32bMap, buf, 1 << 20);
    call(k96v32bFed, buf, 16);
    for (u64 k = 1; k <= 3000; ++k) {
        k96v32 r = k96v32of(k, (u32)(k * 7));
        call(HASHk96v32PutGrow, buf, &r);
    }
    want($len(k96v32bData(buf)) > 3000);
    k96v32 r = k96v32of(42, 1);
    call(HASHk96v32PutGrow, buf, &r);
    k96v32 lo = {.k0 = 1, .k1 = 0, .val = 11}, hi = {.k0 = 0, .k1 = 1, .val = 12};
    call(HASHk96v32PutGrow, buf, &lo);
    call(HASHk96v32PutGrow, buf, &hi);
    size_t live = 0;
    $for(k96v32, p, k96v32bData(buf)) if (p->k0 || p->k1) ++live;
    testeqv((long long)live, 3002LL, "%lld");
    for (u64 k = 1; k <= 3000; ++k) {
        k96v32 g = k96v32of(k, 0);
        call(HASHk96v32Get, &g, k96v32bData(buf));
        want(g.val == (k == 42 ? 1 : (u32)(k * 7)));
    }
    k96v32 g = {.k0 = 1};
    call(HASHk96v32Get, &g, k96v32bData(buf));
    want(g.val == 11);
    g = (k96v32){.k1 = 1};
    call(HASHk96v32Get, &g, k96v32bData(buf));
    want(g.val == 12);
    k96v32 miss = k96v32of(5000, 0);
    want(HASHk96v32Get(&miss, k96v32bData(buf)) == HASHNONE);
    //  the value takes no part in hashing or key equality
    k96v32 a = k96v32of(7, 1), b = k96v32of(7, 2);
    want(k96v32Hash(&a) == k96v32Hash(&b) && k96v32KeyEq(&a, &b));
    call(k96v32bUnMap, buf);
    done;
}

//  BOX: the youngest row of a key wins
ok64 K96V32box() {
    sane(1);
    a_box(k96v32, h);
    k96v32 **recs = BOXk96v32Mem(h);
    call(k96v32bMap, recs, 1 << 14);
    u32 last[100] = {};
    for (u32 i = 1; i <= 2000; i++) {
        u64 k = (i * 37) % 100;
        k96v32 r = {.k0 = k, .k1 = (u32)(k % 3), .val = i};
        last[k] = i;
        call(BOXk96v32Put, h, &r);
    }
    for (u64 k = 0; k < 100; k++) {
        k96v32 key = {.k0 = k, .k1 = (u32)(k % 3)}, got = {};
        call(BOXk96v32Get, &got, h, &key);
        want(got.val == last[k]);
    }
    call(BOXk96v32Sort, h, YES);
    want(h[2] - h[1] == 1);
    want(h[1][0][1] - h[1][0][0] == 100);
    for (k96v32 const *p = h[1][0][0]; p < h[1][0][1]; p++)
        want(p->val == last[p->k0] && p->k1 == p->k0 % 3);
    k96v32bUnMap(recs);
    done;
}

//  QSORT: by key only (k1 high, then k0), the value ignored
ok64 K96V32sort() {
    sane(1);
    k96v32 arr[] = {
        {1, 2, 0}, {9, 1, 1}, {0, 2, 2}, {1, 2, 3}, {5, 1, 4}, {u64max, 0, 5},
    };
    QSORTk96v32InSort(arr, arr + 6);
    static u64 const keys[6][2] = {{u64max, 0}, {5, 1}, {9, 1},
                                   {0, 2},      {1, 2}, {1, 2}};
    for (int i = 0; i < 6; i++)
        want(arr[i].k0 == keys[i][0] && arr[i].k1 == keys[i][1]);
    want(arr[4].val == 0 && arr[5].val == 3);
    k96v32 v0 = {1, 2, u32max}, v1 = {1, 2, 0};
    want(!k96v32Z(&v0, &v1) && !k96v32Z(&v1, &v0));
    want(!k96v32Z(&k96v32Max, &arr[5]) && k96v32Z(&arr[5], &k96v32Max));
    want(sizeof(k96v32) == 16);
    done;
}

//  ABC-26B02JHe_k: X(,Max) is the greatest key under Z for every type
#define MAXLAST(T, ...)                                    \
    do {                                                   \
        T const v_[] = {__VA_ARGS__};                      \
        for (size_t i_ = 0; i_ < sizeof(v_) / sizeof(T); i_++) { \
            want(T##Z(v_ + i_, &T##Max));                  \
            want(!T##Z(&T##Max, v_ + i_));                 \
        }                                                  \
        want(!T##Z(&T##Max, &T##Max));                     \
    } while (0)
ok64 MAXlast() {
    sane(1);
    MAXLAST(u8, 0, 1, 0xfe);
    MAXLAST(u16, 0, 1, 0xfffe);
    MAXLAST(u32, 0, 1, 0xfffffffe);
    MAXLAST(u64, 0, 1, u64max - 1);
    MAXLAST(i8, INT8_MIN, -1, 0, INT8_MAX - 1);
    MAXLAST(i16, INT16_MIN, -1, 0, INT16_MAX - 1);
    MAXLAST(i32, INT32_MIN, -1, 0, INT32_MAX - 1);
    MAXLAST(i64, INT64_MIN, -1, 0, INT64_MAX - 1);
    MAXLAST(f32, -__builtin_inff(), -1.0f, -0.0f, 0.0f, 3.4028235e38f);
    MAXLAST(f64, -__builtin_inf(), -1.0, -0.0, 0.0, 1.7976931348623157e308);
    MAXLAST(u128, {{0, 0}}, {{u64max, u64max - 1}}, {{0, u64max}});
    MAXLAST(n128, {{0, 0}}, {{u64max - 1, u64max}});
    MAXLAST(u256, {{u64max, u64max, u64max, u64max - 1}}, {{0}});
    MAXLAST(u512, {{u64max - 1, u64max, u64max, u64max, u64max, u64max, u64max, u64max}});
    MAXLAST(kv32, {0, u32max}, {u32max - 1, u32max});
    MAXLAST(kv64, {0, u64max}, {u64max - 1, u64max});
    MAXLAST(kv128, {{{u64max, u64max - 1}}, {{u64max, u64max}}});
    MAXLAST(k96v32, {u64max, u32max - 1, u32max}, {u64max - 1, u32max, 0});
    //  all-ones bits are a NaN as a double: unordered, never a Max
    f64 nan = 0;
    u64 ones = u64max;
    memcpy(&nan, &ones, sizeof(nan));
    want(!f64Z(&nan, &f64Max) && !f64Z(&f64Max, &nan));
    done;
}

ok64 KV() {
    sane(1);
    call(KV128hash);
    call(KV128box);
    call(KV128sort);
    call(K96V32hash);
    call(K96V32box);
    call(K96V32sort);
    call(MAXlast);
    done;
}

TEST(KV);
