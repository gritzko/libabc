//  BOX-26A26EbjLj: the run-buffer BOX — puts, ladder, Get/Range, merge op,
//  against a naive oracle.
#include "INT.h"
#include "KV.h"
#include "PRO.h"
#include "TEST.h"

#define X(M, name) M##u64##name
#include "BOXx.h"
#undef X

//  keyed lane, no merge op: the youngest row of a key wins
#define ABC_BOX_SEG (8 * sizeof(kv64))
#define X(M, name) M##kv64##name
#include "BOXx.h"
#undef X

//  keyed lane WITH a merge op: vals add up
typedef struct cnt64 {
    u64 key;
    u64 val;
} cnt64;
fun b8 cnt64Z(cnt64 const *a, cnt64 const *b) { return a->key < b->key; }
fun b8 cnt64KeyEq(cnt64 const *a, cnt64 const *b) { return a->key == b->key; }
fun void cnt64Merge(cnt64 *acc, cnt64 const *nxt) { acc->val += nxt->val; }
con cnt64 cnt64Max = {u64max, u64max};
#define X(M, name) M##cnt64##name
#include "Bx.h"
#include "QSORTx.h"
#define ABC_BOX_SEG (5 * sizeof(cnt64))
#define ABC_X_MERGE
#include "BOXx.h"
#undef X

//  ABC-26B02JHe_k: a signed scalar lane (i64Max = INT64_MAX, -1 is all-ones)
#define X(M, name) M##i64##name
#define ABC_BOX_SEG (8 * sizeof(i64))
#include "BOXx.h"
#undef X

//  ABC-26B02JHe_k: a float-keyed lane, Max = +INF (all-ones is a NaN)
typedef struct f64kv {
    f64 key;
    u64 val;
} f64kv;
fun b8 f64kvZ(f64kv const *a, f64kv const *b) { return a->key < b->key; }
fun b8 f64kvKeyEq(f64kv const *a, f64kv const *b) { return a->key == b->key; }
con f64kv f64kvMax = {__builtin_inf(), u64max};
#define X(M, name) M##f64kv##name
#include "Bx.h"
#include "QSORTx.h"
#define ABC_BOX_SEG (6 * sizeof(f64kv))
#include "BOXx.h"
#undef X

#define N_PUT 20000
#define KEYS 3000

static u64 rnd(u64 *s) {
    *s ^= *s << 13;
    *s ^= *s >> 7;
    *s ^= *s << 17;
    return *s;
}

//  every run ascending, strictly (one row per key), runs back to back in PAST,
//  the ladder holds
static ok64 check_runs(u64csb h) {
    sane(1);
    u64 **recs = BOXu64Mem(h);
    u64 const *at = recs[0];
    a_pad(u64, lens, ABC_HIT_MAX_RUNS);
    for (u64cs const *r = h[1]; r < h[2]; r++) {
        want((*r)[0] == at);
        for (u64 const *p = (*r)[0] + 1; p < (*r)[1]; p++) want(p[-1] < *p);
        at = (*r)[1];
        u64bFeed1(lens, (u64)$len(*r));
    }
    want(at == recs[1]);
    want(HITLadderOK(u64bDataC(lens)));
    done;
}

ok64 BOX0() {
    sane(1);
    a_box(u64, h);
    u64 **recs = BOXu64Mem(h);
    call(u64bMap, recs, 1 << 20);
    u8 seen[KEYS] = {};
    u64 s = 42;
    for (int i = 0; i < N_PUT; i++) {
        u64 v = 1 + rnd(&s) % KEYS;
        seen[v - 1] = 1;
        call(BOXu64Put, h, &v);
        if (i % 997 == 0) call(check_runs, h);
    }
    for (u64 k = 1; k <= KEYS + 5; k++) {
        u64 got = 0;
        ok64 o = BOXu64Get(&got, h, &k);
        if (k <= KEYS && seen[k - 1]) {
            want(o == OK && got == k);
        } else {
            want(o == NONE);
        }
    }
    //  Range [100, 200): the HIT merge equals the oracle
    u64cs view[ABC_HIT_MAX_RUNS];
    u64css into = {view, view + ABC_HIT_MAX_RUNS};
    u64 lo = 100, hi = 200;
    call(BOXu64Range, into, h, &lo, &hi);
    want(recs[1] == recs[2]);  //  the segment became a run
    call(check_runs, h);
    u64 out[256];
    u64s o = {out, out + 256};
    call(HITu64Merge, o, into);
    u64 *w = out;
    for (u64 k = lo; k < hi; k++)
        if (seen[k - 1]) want(*w++ == k);
    want(w == o[0]);
    //  collapse all: one run, every seen key once
    call(BOXu64Sort, h, YES);
    want(h[2] - h[1] == 1);
    size_t nseen = 0;
    for (int k = 0; k < KEYS; k++) nseen += seen[k];
    testeqv((long long)$len(h[1][0]), (long long)nseen, "%lld");
    call(check_runs, h);
    u64bUnMap(recs);
    done;
}

//  youngest wins across runs and within the segment
ok64 BOX1() {
    sane(1);
    a_box(kv64, h);
    kv64 **recs = BOXkv64Mem(h);
    call(kv64bMap, recs, 1 << 16);
    u64 last[64] = {};
    u64 s = 7;
    for (u64 i = 1; i <= 5000; i++) {
        kv64 r = {rnd(&s) % 64, i};
        last[r.key] = i;
        call(BOXkv64Put, h, &r);
        if (i % 333 == 0) {
            for (u64 k = 0; k < 64; k++) {
                kv64 key = {k, 0}, got = {};
                ok64 o = BOXkv64Get(&got, h, &key);
                if (last[k]) {
                    want(o == OK && got.val == last[k]);
                } else {
                    want(o == NONE);
                }
            }
        }
    }
    call(BOXkv64Sort, h, YES);
    want(h[2] - h[1] == 1);
    kv64 const *p = h[1][0][0];
    for (; p < h[1][0][1]; p++) want(p->val == last[p->key]);
    kv64bUnMap(recs);
    done;
}

//  the merge op folds in run order: vals sum up, in Get and after Sort
ok64 BOX2() {
    sane(1);
    a_box(cnt64, h);
    cnt64 **recs = BOXcnt64Mem(h);
    call(cnt64bMap, recs, 1 << 16);
    u64 sum[50] = {};
    u64 s = 99;
    for (u64 i = 1; i <= 4000; i++) {
        cnt64 r = {rnd(&s) % 50, i};
        sum[r.key] += i;
        call(BOXcnt64Put, h, &r);
    }
    for (u64 k = 0; k < 50; k++) {
        cnt64 key = {k, 0}, got = {};
        ok64 o = BOXcnt64Get(&got, h, &key);
        if (sum[k]) want(o == OK && got.val == sum[k]);
    }
    call(BOXcnt64Sort, h, YES);
    want(h[2] - h[1] == 1);
    for (cnt64 const *p = h[1][0][0]; p < h[1][0][1]; p++)
        want(p->val == sum[p->key]);
    cnt64bUnMap(recs);
    done;
}

//  the tombstone record is refused; a full buffer says NOROOM
ok64 BOX3() {
    sane(1);
    a_box(u64, h);
    u64 **recs = BOXu64Mem(h);
    call(u64bMap, recs, 16);
    u64 t = ~0ULL;
    want(BOXu64Put(h, &t) == BADARG);
    for (u64 i = 1; i <= 16; i++) call(BOXu64Put, h, &i);
    u64 x = 99;
    want(BOXu64Put(h, &x) == NOROOM);
    u64bUnMap(recs);
    done;
}

//  ascending puts take the in-order shortcut; a key repeated across a run
//  boundary must not, and dips into older keys must still merge
ok64 BOX4() {
    sane(1);
    a_box(kv64, h);
    kv64 **recs = BOXkv64Mem(h);
    call(kv64bMap, recs, 1 << 16);
    u64 last[4000] = {};
    for (u64 i = 1; i <= 6000; i++) {
        u64 k = i < 3000 ? i : (i % 7 ? i / 2 : 2999);
        if (k >= 4000) k = 3999;
        kv64 r = {k, i};
        last[k] = i;
        call(BOXkv64Put, h, &r);
        if (i % 500 == 0) {
            kv64 const *at = recs[0];
            for (kv64cs const *rr = h[1]; rr < h[2]; rr++) {
                want((*rr)[0] == at);
                for (kv64 const *p = (*rr)[0] + 1; p < (*rr)[1]; p++)
                    want(p[-1].key < p->key);
                at = (*rr)[1];
            }
            want(at == recs[1]);
        }
    }
    for (u64 k = 0; k < 4000; k++) {
        kv64 key = {k, 0}, got = {};
        ok64 o = BOXkv64Get(&got, h, &key);
        if (last[k]) {
            want(o == OK && got.val == last[k]);
        } else {
            want(o == NONE);
        }
    }
    call(BOXkv64Sort, h, YES);
    want(h[2] - h[1] == 1);
    for (kv64 const *p = h[1][0][0]; p < h[1][0][1]; p++)
        want(p->val == last[p->key]);
    kv64bUnMap(recs);
    done;
}

//  batches become runs: distinct keys per batch, youngest batch wins, mixed
//  with single puts; the ladder holds
ok64 BOX5() {
    sane(1);
    a_box(kv64, h);
    kv64 **recs = BOXkv64Mem(h);
    call(kv64bMap, recs, 1 << 18);
    u64 last[5000] = {};
    u64 s = 3, stamp = 0;
    kv64 batch[700];
    for (int b = 0; b < 120; b++) {
        u8 used[5000] = {};
        size_t n = 0;
        for (int i = 0; i < 700; i++) {
            u64 k = rnd(&s) % 5000;
            if (used[k]) continue;
            used[k] = 1;
            batch[n++] = (kv64){k, ++stamp};
            last[k] = stamp;
        }
        kv64cs bs = {batch, batch + n};
        call(BOXkv64PutBatch, h, bs);
        kv64 one = {rnd(&s) % 5000, ++stamp};
        last[one.key] = stamp;
        call(BOXkv64Put, h, &one);
        a_pad(u64, lens, ABC_HIT_MAX_RUNS);
        for (kv64cs const *r = h[1]; r < h[2]; r++) {
            for (kv64 const *p = (*r)[0] + 1; p < (*r)[1]; p++)
                want(p[-1].key < p->key);
            u64bFeed1(lens, (u64)$len(*r));
        }
        want(HITLadderOK(u64bDataC(lens)));
    }
    for (u64 k = 0; k < 5000; k++) {
        kv64 key = {k, 0}, got = {};
        ok64 o = BOXkv64Get(&got, h, &key);
        if (last[k]) {
            want(o == OK && got.val == last[k]);
        } else {
            want(o == NONE);
        }
    }
    kv64 t = kv64Max;
    kv64cs ts = {&t, &t + 1};
    want(BOXkv64PutBatch(h, ts) == BADARG);
    kv64bUnMap(recs);
    done;
}

//  ABC-26B02JHe_k (review repro1): a row with the Max KEY but another value
//  was taken, tied the tombstones under Z and leaked them into the run.  Now
//  the tombstone is tested by key: such a row is BADARG and stores nothing.
ok64 BOXmaxkey() {
    sane(1);
    a_box(kv64, h);
    kv64 **recs = BOXkv64Mem(h);
    call(kv64bMap, recs, 1 << 16);
    kv64 m = {u64max, 1};
    want(BOXkv64Put(h, &m) == BADARG);
    want(recs[2] == recs[1]);
    kv64cs ms = {&m, &m + 1};
    want(BOXkv64PutBatch(h, ms) == BADARG);
    want(recs[2] == recs[1]);
    u64 s = 5, last[100] = {};
    for (u64 i = 1; i <= 2000; i++) {
        kv64 r = {i % 3 ? rnd(&s) % 100 : u64max, i};
        ok64 o = BOXkv64Put(h, &r);
        if (r.key == u64max) {
            want(o == BADARG);
        } else {
            want(o == OK);
            last[r.key] = i;
        }
    }
    call(BOXkv64Sort, h, YES);
    size_t n = 0;
    for (u64 k = 0; k < 100; k++) n += last[k] != 0;
    testeqv((long long)$len(h[1][0]), (long long)n, "%lld");
    for (kv64 const *p = h[1][0][0]; p < h[1][0][1]; p++) {
        want(p->key != u64max && p->val == last[p->key]);
        if (p > h[1][0][0]) want(p[-1].key < p->key);
    }
    kv64 key = {u64max, 0}, got = {};
    want(BOXkv64Get(&got, h, &key) == NONE);
    kv64bUnMap(recs);
    done;
}

//  ABC-26B02JHe_k: a Put whose seal finds no run slot is FULL and stores
//  nothing (trunk stored the row, then said BOXFULL); PutBatch alike
ok64 BOXfullput() {
    sane(1);
    a_box(u64, h);
    u64 **recs = BOXu64Mem(h);
    call(u64bMap, recs, 256);
    h[1] = h[2] = h[3] - 1;  //  one free cell: PAST is not BOX's
    for (u64 i = 1; i <= 65; i++) call(BOXu64Put, h, &i);
    want(h[2] - h[1] == 1);  //  the one slot taken
    for (u64 i = 100; i < 164; i++) call(BOXu64Put, h, &i);
    u64 const *seg = recs[2];
    u64 x = 999, got = 0;
    want(BOXu64Put(h, &x) == FULL);
    want(recs[2] == seg);
    want(*seg != x);  //  BOX-26B04JISf_: IDLE not scribbled
    want(BOXu64Get(&got, h, &x) == NONE);
    u64cs xs = {&x, &x + 1};
    want(BOXu64PutBatch(h, xs) == FULL);
    want(BOXu64Get(&got, h, &x) == NONE);
    want(BOXu64Sort(h, NO) == FULL);
    want(recs[2] == seg);
    //  a record buffer out of IDLE is NOROOM, not FULL
    a_box(u64, rn3);
    call(u64bMap, BOXu64Mem(rn3), 4);
    for (u64 i = 1; i <= 4; i++) call(BOXu64Put, rn3, &i);
    want(BOXu64Put(rn3, &x) == NOROOM);
    u64bUnMap(BOXu64Mem(rn3));
    u64bUnMap(recs);
    done;
}

//  ABC-26B02JHe_k: Max is the greatest KEY, not all-ones bits: a signed
//  lane keeps -1 (all-ones) and INT64_MIN, refuses INT64_MAX; a float lane
//  keeps -INF, DBL_MAX, -0.0, refuses +INF; tombstones trim off the tail
ok64 BOXsigned() {
    sane(1);
    a_box(i64, h);
    i64 **recs = BOXi64Mem(h);
    call(i64bMap, recs, 1 << 16);
    i64 mx = INT64_MAX;
    want(BOXi64Put(h, &mx) == BADARG);
    i64 const keys[] = {-1, INT64_MIN, 0, 7, INT64_MAX - 1, -1000};
    for (int r = 0; r < 300; r++)
        for (int i = 0; i < 6; i++) call(BOXi64Put, h, keys + i);
    call(BOXi64Sort, h, YES);
    want(h[2] - h[1] == 1);
    i64 const want_[] = {INT64_MIN, -1000, -1, 0, 7, INT64_MAX - 1};
    testeqv((long long)$len(h[1][0]), 6LL, "%lld");
    for (int i = 0; i < 6; i++) want(h[1][0][0][i] == want_[i]);
    i64 got = 0, k = -1;
    want(BOXi64Get(&got, h, &k) == OK && got == -1);
    i64bUnMap(recs);

    a_box(f64kv, fruns);
    f64kv **fr = BOXf64kvMem(fruns);
    call(f64kvbMap, fr, 1 << 16);
    f64kv inf = {__builtin_inf(), 1};
    want(BOXf64kvPut(fruns, &inf) == BADARG);
    f64 const fk[] = {-__builtin_inf(), 1.5, -0.0, 1.7976931348623157e308, -2.5};
    u64 stamp = 0;
    for (int r = 0; r < 200; r++)
        for (int i = 0; i < 5; i++) {
            f64kv row = {fk[i], ++stamp};
            call(BOXf64kvPut, fruns, &row);
        }
    call(BOXf64kvSort, fruns, YES);
    want(fruns[2] - fruns[1] == 1);
    f64 const fw[] = {-__builtin_inf(), -2.5, -0.0, 1.5, 1.7976931348623157e308};
    testeqv((long long)$len(fruns[1][0]), 5LL, "%lld");
    for (int i = 0; i < 5; i++) {
        f64kv const *p = fruns[1][0][0] + i;
        want(p->key == fw[i]);
        u64 const age[] = {0, 4, 2, 1, 3};  //  fk index order -> fw order
        want(p->val == stamp - 4 + age[i]);
    }
    f64kvbUnMap(fr);
    done;
}

ok64 BOXtest() {
    sane(1);
    call(BOX0);
    call(BOX1);
    call(BOX2);
    call(BOX3);
    call(BOX4);
    call(BOX5);
    call(BOXmaxkey);
    call(BOXfullput);
    call(BOXsigned);
    done;
}

TEST(BOXtest);
