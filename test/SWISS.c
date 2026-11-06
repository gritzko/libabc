//  SWISS-26B05A2Jxv: SWISSx.h on kv64 (16 B), kv128 (32 B), a 64 B record
//  and a 32 B X(,Merge) lane.  Built twice: SWISStest (SSE2/NEON) and
//  SWISSswartest (-DABC_SWISS_SWAR).
#include "SWISS.h"

#include "01.h"
#include "INT.h"
#include "KV.h"
#include "OK.h"
#include "TEST.h"

#define X(M, name) M##kv64##name
#include "SWISSx.h"
#undef X

#define X(M, name) M##kv128##name
#include "SWISSx.h"
#undef X

typedef struct big64 {
    u64 key;
    u64 pad[7];
} big64;
fun u64 big64Hash(big64 const *v) { return mix64(v->key); }
fun b8 big64Z(big64 const *a, big64 const *b) { return a->key < b->key; }
fun b8 big64KeyEq(big64 const *a, big64 const *b) { return a->key == b->key; }
#define X(M, name) M##big64##name
#include "Bx.h"
#include "SWISSx.h"
#undef X

typedef struct sum128 {
    u32 key;
    u32 val;
    u64 pad[3];
} sum128;
fun u64 sum128Hash(sum128 const *v) { return mix32(v->key); }
fun b8 sum128Z(sum128 const *a, sum128 const *b) { return a->key < b->key; }
fun b8 sum128KeyEq(sum128 const *a, sum128 const *b) { return a->key == b->key; }
fun void sum128Merge(sum128 *acc, sum128 const *nxt) { acc->val += nxt->val; }
#define X(M, name) M##sum128##name
#include "Bx.h"
#define ABC_SWISS_MERGE
#include "SWISSx.h"
#undef X

//  the group scans against a byte loop
ok64 SWISSscan() {
    sane(1);
    alignas(16) u8 g[16];
    u64 r = 1;
    for (int n = 0; n < 1000; ++n) {
        u32 m = 0, l = 0, e = 0;
        for (int i = 0; i < 16; ++i) {
            r = r * 6364136223846793005ULL + 1442695040888963407ULL;
            u8 c = (u8)(r >> 56);
            g[i] = (r >> 40 & 3) ? c : (u8)(0x80 | (n & 0x7f));
            if (n & 1 && (r >> 44 & 3) == 0) g[i] = 0;
        }
        u8 tag = (u8)(0x80 | (n & 0x7f));
        for (int i = 0; i < 16; ++i) {
            m |= (u32)(g[i] == tag) << i;
            l |= (u32)(g[i] >> 7) << i;
            e |= (u32)(g[i] == 0) << i;
        }
        same(SWISSMatch(g, tag), m);
        same(SWISSLive(g), l);
        same(SWISSEmpty(g), e);
    }
    done;
}

//  the all-zero record is storable: the control byte says live or not
ok64 SWISS0() {
    sane(1);
    Bkv64 t = {};
    call(kv64bMap, t, 1024);
    call(SWISSkv64Init, t, 16);
    kv64 z = {}, q = {};
    want(NONE == SWISSkv64Get(&q, t));
    call(SWISSkv64Put, t, &z);
    call(SWISSkv64Get, &q, t);
    for (u64 i = 1; i < 14; ++i) {
        kv64 r = {i, i * 10};
        call(SWISSkv64Put, t, &r);
    }
    kv64 x = {99, 1};
    want(FULL == SWISSkv64Put(t, &x));  // 14 = 7/8 of 16
    want(NONE == SWISSkv64Get(&x, t));
    kv64 o = {5, 77};
    call(SWISSkv64Put, t, &o);  // overwrite fits
    o.val = 0;
    call(SWISSkv64Get, &o, t);
    want(o.val == 77);
    call(SWISSkv64Del, t, &z);
    want(NONE == SWISSkv64Del(t, &z));
    want(NONE == SWISSkv64Get(&z, t));
    call(SWISSkv64Put, t, &x);  // the freed room
    want(BADARG == SWISSkv64Init(t, 24));
    want(NOROOM == SWISSkv64Init(t, 1024));
    call(kv64bUnMap, t);
    done;
}

//  CODE-26B065sXWC: a mapped but un-Init-ed (empty) buffer is refused
ok64 SWISSnoinit() {
    sane(1);
    Bkv64 t = {};
    call(kv64bMap, t, 1 << 20);
    kv64 r = {5, 7}, *s = NULL;
    kv64 const *c = NULL;
    size_t i = 0;
    want(FAILSANITY == SWISSkv64Put(t, &r));
    want(FAILSANITY == SWISSkv64PutGrow(t, &r));
    want(FAILSANITY == SWISSkv64Probe(&s, t, &r));
    want(FAILSANITY == SWISSkv64ProbeGrow(&s, t, &r));
    want(FAILSANITY == SWISSkv64Get(&r, t));
    want(FAILSANITY == SWISSkv64At(&c, t, &r));
    want(FAILSANITY == SWISSkv64Del(t, &r));
    want(NONE == SWISSkv64DelAt(t, 0));
    want(NONE == SWISSkv64Next(&i, t));
    want(BADARG == SWISSkv64Grow(t));
    call(kv64bUnMap, t);
    done;
}

//  PutGrow from 16 slots to 2^17, then delete half, churn, iterate
ok64 SWISSgrow() {
    sane(1);
    Bkv128 t = {};
    call(kv128bMap, t, 1 << 18);
    call(SWISSkv128Init, t, 16);
    u64 const N = 100000;
    for (u64 i = 0; i < N; ++i) {
        kv128 r = {.key = {._64 = {i, ~i}}, .val = {._64 = {i, 0}}};
        call(SWISSkv128PutGrow, t, &r);
    }
    want(kv128bDataLen(t) == 1 << 17);
    for (u64 i = 0; i < N; i += 2) {
        kv128 r = {.key = {._64 = {i, ~i}}};
        call(SWISSkv128Del, t, &r);
    }
    //  churn: tombs pile up, a same-length rehash purges them
    for (u64 j = 0; j < 4 * N; ++j) {
        kv128 r = {.key = {._64 = {N + j, 0}}};
        call(SWISSkv128PutGrow, t, &r);
        call(SWISSkv128Del, t, &r);
    }
    want(kv128bDataLen(t) == 1 << 17);
    for (u64 i = 0; i < N; ++i) {
        kv128 r = {.key = {._64 = {i, ~i}}};
        ok64 o = SWISSkv128Get(&r, t);
        want(o == (i & 1 ? OK : NONE));
        if (o == OK) want(r.val._64[0] == i);
    }
    size_t live = 0;
    for (size_t i = 0; OK == SWISSkv128Next(&i, t); ++i) ++live;
    want(live == N / 2);
    call(kv128bUnMap, t);
    done;
}

//  Probe fills or Unprobes; ProbeGrow; At
ok64 SWISSprobe() {
    sane(1);
    Bbig64 t = {};
    call(big64bMap, t, 4096);
    call(SWISSbig64Init, t, 16);
    for (u64 i = 1; i <= 1000; ++i) {
        big64 r = {.key = i};
        big64 *s = NULL;
        want(NONE == SWISSbig64ProbeGrow(&s, t, &r));
        want(s->key == 0 && s->pad[3] == 0);
        if (i % 3) {
            *s = r;
            s->pad[0] = i;
        } else {
            call(SWISSbig64Unprobe, t, s);
        }
    }
    for (u64 i = 1; i <= 1000; ++i) {
        big64 r = {.key = i};
        big64 const *s = NULL;
        big64 *p = NULL;
        ok64 o = SWISSbig64At(&s, t, &r);
        want(o == (i % 3 ? OK : NONE));
        if (o != OK) continue;
        want(s->pad[0] == i);
        want(OK == SWISSbig64Probe(&p, t, &r) && p == s);
    }
    call(SWISSbig64Grow, t);
    want(NOROOM == SWISSbig64Grow(t));  // 4096 records map < 4096 slots
    call(big64bUnMap, t);
    done;
}

//  erase while iterating, SWISS.md: nothing moves, DelAt says NONE
ok64 SWISSdelat() {
    sane(1);
    Bkv64 t = {};
    call(kv64bMap, t, 1 << 12);
    call(SWISSkv64Init, t, 16);
    for (u64 i = 0; i < 1000; ++i) {
        kv64 r = {i, i};
        call(SWISSkv64PutGrow, t, &r);
    }
    for (size_t i = 0; OK == SWISSkv64Next(&i, t); ++i)
        if (!(t[1][i].key % 3)) want(NONE == SWISSkv64DelAt(t, i));
    size_t live = 0;
    for (size_t i = 0; OK == SWISSkv64Next(&i, t); ++i, ++live)
        want(t[1][i].key % 3);
    want(live == 666);
    done;
}

//  Move into bigger and smaller tables; FULL keeps the prefix; Merge
ok64 SWISSmove() {
    sane(1);
    Bkv64 a = {}, b = {}, c = {};
    call(kv64bMap, a, 1 << 12);
    call(kv64bMap, b, 1 << 12);
    call(kv64bMap, c, 1 << 12);
    call(SWISSkv64Init, a, 512);
    for (u64 i = 1; i <= 300; ++i) {
        kv64 r = {i, i};
        call(SWISSkv64Put, a, &r);
    }
    call(SWISSkv64Init, b, 1024);
    call(SWISSkv64Move, b, a);
    for (u64 i = 1; i <= 300; ++i) {
        kv64 r = {i, 0};
        call(SWISSkv64Get, &r, b);
        want(r.val == i);
    }
    call(SWISSkv64Init, c, 256);  // 224 fit
    want(FULL == SWISSkv64Move(c, a));
    size_t live = 0;
    for (size_t i = 0; OK == SWISSkv64Next(&i, c); ++i) ++live;
    want(live == 224);
    want(BADARG == SWISSkv64Move(a, a));
    Bsum128 s = {};
    call(sum128bMap, s, 64);
    call(SWISSsum128Init, s, 16);
    for (u32 i = 1; i <= 10; i++) {
        sum128 r = {.key = i % 3 + 1, .val = i};
        call(SWISSsum128Put, s, &r);
    }
    sum128 q = {.key = 2};
    call(SWISSsum128Get, &q, s);
    want(q.val == 1 + 4 + 7 + 10);
    done;
}

ok64 SWISS() {
    sane(1);
    call(SWISSscan);
    call(SWISS0);
    call(SWISSnoinit);
    call(SWISSgrow);
    call(SWISSprobe);
    call(SWISSdelat);
    call(SWISSmove);
    done;
}

TEST(SWISS);
