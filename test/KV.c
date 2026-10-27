//  KV-26A273cxFy: kv128 {u128 key; u128 val} as a HASH map, a BOX lane, QSORTed
#include "INT.h"
#include "KV.h"
#include "PRO.h"
#include "TEST.h"

#define X(M, name) M##kv128##name
#include "HASHx.h"
#include "HITx.h"
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
    call(kv128bMap, buf, 16);
    for (u64 k = 1; k <= 3000; ++k) {
        kv128 r = kv128of(k, k * 7);
        call(HASHkv128PutGrow, buf, &r);
    }
    kv128 r = kv128of(42, 1);
    call(HASHkv128PutGrow, buf, &r);
    size_t live = 0;
    $for(kv128, p, kv128bIdle(buf)) if (p->key._64[0]) ++live;
    testeqv((long long)live, 3000LL, "%lld");
    for (u64 k = 1; k <= 3000; ++k) {
        kv128 g = {.key = kv128of(k, 0).key};
        call(HASHkv128Get, &g, kv128bIdle(buf));
        kv128 w = kv128of(k, k == 42 ? 1 : k * 7);
        want(g.val._64[0] == w.val._64[0] && g.val._64[1] == w.val._64[1]);
    }
    kv128 miss = kv128of(5000, 0);
    want(HASHkv128Get(&miss, kv128bIdle(buf)) == HASHNONE);
    call(kv128bUnMap, buf);
    done;
}

//  BOX: the youngest row of a key wins
ok64 KV128box() {
    sane(1);
    Bkv128 recs = {};
    call(kv128bMap, recs, 1 << 14);
    kv128cs slots[HIT_MAX_RUNS];
    kv128csb runs = {slots, slots, slots, slots + HIT_MAX_RUNS};
    u64 last[100] = {};
    for (u64 i = 1; i <= 2000; i++) {
        u64 k = (i * 37) % 100;
        kv128 r = kv128of(k, i);
        last[k] = i;
        call(BOXkv128Put, recs, runs, &r);
    }
    for (u64 k = 0; k < 100; k++) {
        kv128 key = kv128of(k, 0), got = {};
        call(BOXkv128Get, recs, runs, &key, &got);
        want(got.val._64[0] == last[k]);
    }
    call(BOXkv128Sort, recs, runs, YES);
    want(runs[2] - runs[1] == 1);
    want(runs[1][0][1] - runs[1][0][0] == 100);
    for (kv128 const *p = runs[1][0][0]; p < runs[1][0][1]; p++)
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
    want(!kv128Z(&kv128max, &arr[4]) && kv128Z(&arr[4], &kv128max));
    done;
}

ok64 KV() {
    sane(1);
    call(KV128hash);
    call(KV128box);
    call(KV128sort);
    done;
}

TEST(KV);
