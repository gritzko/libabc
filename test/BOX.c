//  BOX-26A26EbjLj: the run-buffer BOX — puts, ladder, Get/Range, merge op,
//  against a naive oracle.
#include "INT.h"
#include "KV.h"
#include "PRO.h"
#include "TEST.h"

#define X(M, name) M##u64##name
#include "HITx.h"
#include "BOXx.h"
#undef X

//  keyed lane, no merge op: the youngest row of a key wins
#define BOXx_C (8 * sizeof(kv64))
#define X(M, name) M##kv64##name
#include "HITx.h"
#include "BOXx.h"
#undef X

//  keyed lane WITH a merge op: vals add up
typedef struct cnt64 {
    u64 key;
    u64 val;
} cnt64;
fun b8 cnt64Z(cnt64 const *a, cnt64 const *b) { return a->key < b->key; }
con cnt64 cnt64max = {u64max, u64max};
#define X(M, name) M##cnt64##name
#include "Bx.h"
#include "QSORTx.h"
#include "HITx.h"
#define BOXx_C (5 * sizeof(cnt64))
#define BOXx_MERGE(acc, nxt) ((acc)->val += (nxt)->val)
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
static ok64 check_runs(u64b recs, u64csb runs) {
    sane(1);
    u64 const *at = recs[0];
    a_pad(u64, lens, HIT_MAX_RUNS);
    for (u64cs *r = runs[1]; r < runs[2]; r++) {
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
    Bu64 recs = {};
    call(u64bMap, recs, 1 << 20);
    u64cs slots[HIT_MAX_RUNS];
    u64csb runs = {slots, slots, slots, slots + HIT_MAX_RUNS};
    u8 seen[KEYS] = {};
    u64 s = 42;
    for (int i = 0; i < N_PUT; i++) {
        u64 v = 1 + rnd(&s) % KEYS;
        seen[v - 1] = 1;
        call(BOXu64Put, recs, runs, &v);
        if (i % 997 == 0) call(check_runs, recs, runs);
    }
    for (u64 k = 1; k <= KEYS + 5; k++) {
        u64 got = 0;
        ok64 o = BOXu64Get(recs, runs, &k, &got);
        if (k <= KEYS && seen[k - 1]) {
            want(o == OK && got == k);
        } else {
            want(o == NONE);
        }
    }
    //  Range [100, 200): the HIT merge equals the oracle
    u64cs view[HIT_MAX_RUNS];
    u64css into = {view, view + HIT_MAX_RUNS};
    u64 lo = 100, hi = 200;
    call(BOXu64Range, into, recs, runs, &lo, &hi);
    want(recs[1] == recs[2]);  //  the segment became a run
    call(check_runs, recs, runs);
    u64 out[256];
    u64s o = {out, out + 256};
    call(HITu64Merge, into, o);
    u64 *w = out;
    for (u64 k = lo; k < hi; k++)
        if (seen[k - 1]) want(*w++ == k);
    want(w == o[0]);
    //  collapse all: one run, every seen key once
    call(BOXu64Sort, recs, runs, YES);
    want(runs[2] - runs[1] == 1);
    size_t nseen = 0;
    for (int k = 0; k < KEYS; k++) nseen += seen[k];
    testeqv((long long)$len(runs[1][0]), (long long)nseen, "%lld");
    call(check_runs, recs, runs);
    u64bUnMap(recs);
    done;
}

//  youngest wins across runs and within the segment
ok64 BOX1() {
    sane(1);
    Bkv64 recs = {};
    call(kv64bMap, recs, 1 << 16);
    kv64cs slots[HIT_MAX_RUNS];
    kv64csb runs = {slots, slots, slots, slots + HIT_MAX_RUNS};
    u64 last[64] = {};
    u64 s = 7;
    for (u64 i = 1; i <= 5000; i++) {
        kv64 r = {rnd(&s) % 64, i};
        last[r.key] = i;
        call(BOXkv64Put, recs, runs, &r);
        if (i % 333 == 0) {
            for (u64 k = 0; k < 64; k++) {
                kv64 key = {k, 0}, got = {};
                ok64 o = BOXkv64Get(recs, runs, &key, &got);
                if (last[k]) {
                    want(o == OK && got.val == last[k]);
                } else {
                    want(o == NONE);
                }
            }
        }
    }
    call(BOXkv64Sort, recs, runs, YES);
    want(runs[2] - runs[1] == 1);
    kv64 const *p = runs[1][0][0];
    for (; p < runs[1][0][1]; p++) want(p->val == last[p->key]);
    kv64bUnMap(recs);
    done;
}

//  the merge op folds in run order: vals sum up, in Get and after Sort
ok64 BOX2() {
    sane(1);
    Bcnt64 recs = {};
    call(cnt64bMap, recs, 1 << 16);
    cnt64cs slots[HIT_MAX_RUNS];
    cnt64csb runs = {slots, slots, slots, slots + HIT_MAX_RUNS};
    u64 sum[50] = {};
    u64 s = 99;
    for (u64 i = 1; i <= 4000; i++) {
        cnt64 r = {rnd(&s) % 50, i};
        sum[r.key] += i;
        call(BOXcnt64Put, recs, runs, &r);
    }
    for (u64 k = 0; k < 50; k++) {
        cnt64 key = {k, 0}, got = {};
        ok64 o = BOXcnt64Get(recs, runs, &key, &got);
        if (sum[k]) want(o == OK && got.val == sum[k]);
    }
    call(BOXcnt64Sort, recs, runs, YES);
    want(runs[2] - runs[1] == 1);
    for (cnt64 const *p = runs[1][0][0]; p < runs[1][0][1]; p++)
        want(p->val == sum[p->key]);
    cnt64bUnMap(recs);
    done;
}

//  the tombstone record is refused; a full buffer says BOXFULL
ok64 BOX3() {
    sane(1);
    u64 mem[16];
    u64b recs = {mem, mem, mem, mem + 16};
    u64cs slots[4];
    u64csb runs = {slots, slots, slots, slots + 4};
    u64 t = ~0ULL;
    want(BOXu64Put(recs, runs, &t) == BOXFAIL);
    for (u64 i = 1; i <= 16; i++) call(BOXu64Put, recs, runs, &i);
    u64 x = 99;
    want(BOXu64Put(recs, runs, &x) == BOXFULL);
    done;
}

//  ascending puts take the in-order shortcut; a key repeated across a run
//  boundary must not, and dips into older keys must still merge
ok64 BOX4() {
    sane(1);
    Bkv64 recs = {};
    call(kv64bMap, recs, 1 << 16);
    kv64cs slots[HIT_MAX_RUNS];
    kv64csb runs = {slots, slots, slots, slots + HIT_MAX_RUNS};
    u64 last[4000] = {};
    for (u64 i = 1; i <= 6000; i++) {
        u64 k = i < 3000 ? i : (i % 7 ? i / 2 : 2999);
        if (k >= 4000) k = 3999;
        kv64 r = {k, i};
        last[k] = i;
        call(BOXkv64Put, recs, runs, &r);
        if (i % 500 == 0) {
            kv64 const *at = recs[0];
            for (kv64cs *rr = runs[1]; rr < runs[2]; rr++) {
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
        ok64 o = BOXkv64Get(recs, runs, &key, &got);
        if (last[k]) {
            want(o == OK && got.val == last[k]);
        } else {
            want(o == NONE);
        }
    }
    call(BOXkv64Sort, recs, runs, YES);
    want(runs[2] - runs[1] == 1);
    for (kv64 const *p = runs[1][0][0]; p < runs[1][0][1]; p++)
        want(p->val == last[p->key]);
    kv64bUnMap(recs);
    done;
}

ok64 BOXtest() {
    sane(1);
    call(BOX0);
    call(BOX1);
    call(BOX2);
    call(BOX3);
    call(BOX4);
    done;
}

TEST(BOXtest);
