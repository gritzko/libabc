#include "HASH.h"
#include "RAP.h"

#include <stdint.h>
#include <unistd.h>

#include "01.h"
#include "INT.h"
#include "KV.h"
#include "OK.h"
#include "TEST.h"

#define X(M, name) M##u32##name
#define ABC_HASH_CONVERGE 1
#include "HASHx.h"
#undef X

#define X(M, name) M##kv32##name
#include "HASHx.h"
#undef X

// HASH-26A274tV3C: Probe/ProbeGrow on u64 (convergent), kv64, k96v32
#define X(M, name) M##u64##name
#define ABC_HASH_CONVERGE 1
#include "HASHx.h"
#undef X

#define X(M, name) M##kv64##name
#include "HASHx.h"
#undef X

#define X(M, name) M##k96v32##name
#include "HASHx.h"
#undef X

ok64 HASH0() {
    sane(1);
    a_pad(u32, pad, 1024);
    zerob(pad);
    u32 one = 1;
    call(HASHu32Put, pad_idle, &one);
    call(HASHu32Get, &one, pad_idle);
    call(HASHu32Del, pad_idle, &one);
    want(HASHNONE == HASHu32Get(&one, pad_idle));
    done;
}

ok64 HASH1() {
    sane(1);
    a_pad(u32, pad, 1024 * 2);
    zerob(pad);
    for (u32 i = 1; i < 1000; i += 2) {
        call(HASHu32Put, pad_idle, &i);
    }
    for (u32 i = 2; i < 1000; i += 2) {
        u32 n = i;
        ok64 o = HASHu32Get(&n, pad_idle);
        want(HASHNONE == o);
    }
    for (u32 i = 1; i < 1000; i += 2) {
        u32 v = i;
        ok64 o = HASHu32Get(&v, pad_idle);
        want(o == OK);
        want(v == i);
    }
    for (u32 i = 1; i < 1000; i += 4) {
        call(HASHu32Del, pad_idle, &i);
    }
    for (u32 i = 1; i < 1000; i += 4) {
        u32 n = i;
        want(HASHNONE == HASHu32Get(&n, pad_idle));
    }
    for (u32 i = 3; i < 1000; i += 4) {
        u32 v = i;
        call(HASHu32Get, &v, pad_idle);
        want(v == i);
    }
    done;
}

ok64 HASH3() {
    sane(1);
    Bkv32 dictbuf = {};
    kv32bAllocate(dictbuf, 1024);
    kv32$ dict = kv32bIdle(dictbuf);

    kv32 a = {.key = 6220, .val = 2};
    call(HASHkv32Put, dict, &a);
    kv32 b = {.key = 22, .val = 3};
    call(HASHkv32Put, dict, &b);

    kv32 a2 = {.key = 6220};
    kv32 b2 = {.key = 22};
    call(HASHkv32Get, &a2, dict);
    call(HASHkv32Get, &b2, dict);
    testeqv((long long)(a2.val), (long long)(2), "%lld");
    testeqv((long long)(b2.val), (long long)(3), "%lld");

    call(HASHkv32Del, dict, &b);

    a.val = 0;
    call(HASHkv32Get, &a, dict);
    testeqv((long long)(a.val), (long long)(2), "%lld");
    mute(HASHkv32Get(&b, dict), HASHNONE);

    kv32bFree(dictbuf);
    done;
}

#define LENd 24

ok64 HASHd() {
    sane(1);
    Bu32 dictbuf = {};
    u32bAllocate(dictbuf, 16);
    u32$ dict = u32bData(dictbuf);
    void **b = (void **)dictbuf;
    b[2] = b[3];
    Bu32 copybuf = {};
    u32bAllocate(copybuf, 16);

    i32 ins[LENd] = {1,  2,  3,  4,   -4, 5,   -5, 5,   6,  7,  8,  9,
                     10, 11, 12, -12, 12, -12, 13, -13, 14, 15, 15, 15};

    for (int i = 0; i < LENd; ++i) {
        if (ins[i] > 0) {
            u32 rec = ins[i];
            HASHu32Put(dict, &rec);
        } else {
            u32 rec = -ins[i];
            HASHu32Del(dict, &rec);
        }
    }
    u32sFeed(u32bIdle(copybuf), u32bDataC(dictbuf));

    for (int j = 0; j < 1000; ++j) {
        zerob(dictbuf);
        int p = rand() % (LENd - 1);
        if (ins[p] != -ins[p + 1]) i32Swap(ins + p, ins + p + 1);
        for (int i = 0; i < LENd; ++i) {
            if (ins[i] > 0) {
                u32 rec = ins[i];
                HASHu32Put(dict, &rec);
            } else {
                u32 rec = -ins[i];
                HASHu32Del(dict, &rec);
            }
        }
        if (!$eq(u32bData(dictbuf), u32bData(copybuf))) {
            fprintf(stderr, "INS\t");
            for (int i = 0; i < LENd; ++i) fprintf(stderr, "%i\t", ins[i]);
            fprintf(stderr, "\n");
            fprintf(stderr, "COPY\t");
            for (int i = 0; i < 16; ++i)
                fprintf(stderr, "%i\t", Bat(copybuf, i));
            fprintf(stderr, "\n");
            fprintf(stderr, "DICT\t");
            for (int i = 0; i < 16; ++i)
                fprintf(stderr, "%i\t", Bat(dictbuf, i));
            fprintf(stderr, "\n");
            fprintf(stderr, "FIT\n");
            for (u32 u = 0; u < 16; ++u)
                fprintf(stderr, "%u\t%" PRIu64 "\n", u, u32hash(&u) % 16);

            zerob(dictbuf);
            for (int i = 0; i < LENd; ++i) {
                if (ins[i] > 0) {
                    u32 rec = ins[i];
                    HASHu32Put(dict, &rec);
                } else {
                    u32 rec = -ins[i];
                    HASHu32Del(dict, &rec);
                }
                fprintf(stderr, "#%i\t", i);
                for (int i = 0; i < 16; ++i)
                    fprintf(stderr, "%i\t", Bat(dictbuf, i));
                fprintf(stderr, "\n");
            }
        }
        $testeq(u32bData(dictbuf), u32bData(copybuf));
    }

    u32bFree(dictbuf);
    u32bFree(copybuf);
    done;
}

// ABC-015: convergent insert must be op-order-independent; the stale
// carried hash after Swap made the layout depend on insertion order.
ok64 HASHo() {
    sane(1);
    // A,B: fit 7 with hA < hB;  C: fit 8 with hA < hC < hB.
    u32 A = 0, B = 0, C = 0;
    u64 hA = 0, hB = 0;
    for (u32 k = 1; k < 4000000 && !(A && B && C); k++) {
        u64 h = u32hash(&k);
        u64 fit = h & 15;
        if (fit == 7) {
            if (!A) {
                A = k;
                hA = h;
            } else if (!B && h > hA) {
                B = k;
                hB = h;
            }
        } else if (fit == 8 && A && B && !C && h > hA && h < hB) {
            C = k;
        }
    }
    want(A && B && C);
    u32 keyset[3] = {A, B, C};
    int perms[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2},
                       {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    u32 ref[16] = {};
    for (int p = 0; p < 6; p++) {
        u32 tab[16] = {};
        $u32 data = {tab, tab + 16};
        for (int i = 0; i < 3; i++) call(HASHu32Put, data, keyset + perms[p][i]);
        if (p == 0) {
            memcpy(ref, tab, sizeof(ref));
        } else {
            want(0 == memcmp(ref, tab, sizeof(ref)));
        }
        for (int i = 0; i < 3; i++) {
            u32 v = keyset[i];
            call(HASHu32Get, &v, data);
        }
    }
    done;
}

// HASH-26A26Cq06k: Put into a 16-slot bMap-ed table until a line is full,
// Grow, carry on; every key must survive every Grow.
ok64 HASHgrow() {
    sane(1);
    Bu32 buf = {};
    call(u32bMap, buf, 16);
    u32 grows = 0;
    for (u32 i = 1; i <= 5000; ++i) {
        // CONVERGE Put may evict a record on NOROOM: probe with find first
        size_t ndx = 0;
        while (HASHNOROOM == HASHu32find(&ndx, u32bIdle(buf), &i)) {
            size_t was = u32bIdleLen(buf);
            call(HASHu32Grow, buf);
            want(u32bIdleLen(buf) > was);
            ++grows;
        }
        call(HASHu32Put, u32bIdle(buf), &i);
    }
    want(grows > 3);
    want(0 == (u32bIdleLen(buf) & (u32bIdleLen(buf) - 1)));
    for (u32 i = 1; i <= 5000; ++i) {
        u32 v = i;
        call(HASHu32Get, &v, u32bIdle(buf));
        want(v == i);
    }
    u32 n = 5001;
    want(HASHNONE == HASHu32Get(&n, u32bIdle(buf)));
    for (u32 i = 1; i <= 5000; i += 2) call(HASHu32Del, u32bIdle(buf), &i);
    call(HASHu32Grow, buf);  // deleted entries must not come back
    for (u32 i = 1; i <= 5000; ++i) {
        u32 v = i;
        want((i & 1 ? HASHNONE : OK) == HASHu32Get(&v, u32bIdle(buf)));
    }
    call(u32bUnMap, buf);
    done;
}

// PutGrow: kv records, values survive growth, overwrite keeps one entry.
ok64 HASHputgrow() {
    sane(1);
    Bkv32 buf = {};
    call(kv32bMap, buf, 16);
    for (u32 k = 1; k <= 3000; ++k) {
        kv32 r = {.key = k, .val = k * 7};
        call(HASHkv32PutGrow, buf, &r);
    }
    kv32 r = {.key = 42, .val = 1};
    call(HASHkv32PutGrow, buf, &r);
    size_t live = 0;
    $for(kv32, p, kv32bIdle(buf)) if (p->key) ++live;
    testeqv((long long)live, 3000LL, "%lld");
    for (u32 k = 1; k <= 3000; ++k) {
        kv32 g = {.key = k};
        call(HASHkv32Get, &g, kv32bIdle(buf));
        want(g.val == (k == 42 ? 1 : k * 7));
    }
    call(kv32bUnMap, buf);
    done;
}

// HASH-26A274tV3C: Probe hit/miss/fill on kv64; a full line is NOROOM
ok64 HASHprobe() {
    sane(1);
    a_pad(kv64, pad, 64);
    zerob(pad);
    kv64 *slot = NULL;
    kv64 r = {.key = 5, .val = 50};
    want(HASHNONE == HASHkv64Probe(&slot, pad_idle, &r));
    want(slot != NULL && slot->key == 0 && slot->val == 0);
    *slot = r;
    kv64 q = {.key = 5, .val = 99};
    kv64 *hit = NULL;
    want(OK == HASHkv64Probe(&hit, pad_idle, &q));
    want(hit == slot && hit->val == 50 && q.val == 99);
    kv64 g = {.key = 5};
    call(HASHkv64Get, &g, pad_idle);
    want(g.val == 50);
    // the filled slot is where Put puts it: Put then Probe agree
    kv64 p = {.key = 6, .val = 60};
    call(HASHkv64Put, pad_idle, &p);
    want(OK == HASHkv64Probe(&hit, pad_idle, &p) && hit->val == 60);
    // one line (16 slots): 16 keys fit, the 17th is NOROOM, slot untouched
    a_pad(kv64, line, 16);
    zerob(line);
    for (u64 k = 1; k <= 16; ++k) {
        kv64 x = {.key = k, .val = k * 3};
        want(HASHNONE == HASHkv64Probe(&slot, line_idle, &x));
        *slot = x;
    }
    for (u64 k = 1; k <= 16; ++k) {
        kv64 x = {.key = k};
        want(OK == HASHkv64Probe(&slot, line_idle, &x) && slot->val == k * 3);
    }
    kv64 full = {.key = 17, .val = 1};
    slot = NULL;
    want(HASHNOROOM == HASHkv64Probe(&slot, line_idle, &full));
    want(slot == NULL);
    done;
}

// ProbeGrow: get-or-insert across growth, one entry per key
ok64 HASHprobegrow() {
    sane(1);
    Bkv64 buf = {};
    call(kv64bMap, buf, 16);
    for (int pass = 0; pass < 2; ++pass)
        for (u64 k = 1; k <= 3000; ++k) {
            kv64 r = {.key = k * 0x10001, .val = k * 7}, *slot = NULL;
            ok64 o = HASHkv64ProbeGrow(&slot, buf, &r);
            want(o == (pass ? OK : HASHNONE));
            if (o == HASHNONE) *slot = r;
            want(slot->key == r.key && slot->val == k * 7);
        }
    want(kv64bIdleLen(buf) > 3000);
    size_t live = 0;
    $for(kv64, p, kv64bIdle(buf)) if (p->key) ++live;
    testeqv((long long)live, 3000LL, "%lld");
    for (u64 k = 1; k <= 3000; ++k) {
        kv64 g = {.key = k * 0x10001};
        call(HASHkv64Get, &g, kv64bIdle(buf));
        want(g.val == k * 7);
    }
    call(kv64bUnMap, buf);

    Bk96v32 kb = {};
    call(k96v32bMap, kb, 16);
    for (int pass = 0; pass < 2; ++pass)
        for (u32 k = 1; k <= 3000; ++k) {
            k96v32 r = {.k0 = k % 50, .k1 = k / 50, .val = k}, *slot = NULL;
            r.val += pass;  // the value never takes part in the match
            ok64 o = HASHk96v32ProbeGrow(&slot, kb, &r);
            want(o == (pass ? OK : HASHNONE));
            if (o == HASHNONE) *slot = r;
            want(slot->k0 == r.k0 && slot->k1 == r.k1 && slot->val == k);
        }
    live = 0;
    $for(k96v32, p, k96v32bIdle(kb)) if (p->k0 || p->k1) ++live;
    testeqv((long long)live, 3000LL, "%lld");
    call(k96v32bUnMap, kb);
    done;
}

// ABC_HASH_CONVERGE: Probe+fill leaves the very layout Put leaves
ok64 HASHprobeconv() {
    sane(1);
    a_pad(u64, put, 1 << 10);
    a_pad(u64, prb, 1 << 10);
    zerob(put);
    zerob(prb);
    u64 x = 0x9e3779b97f4a7c15ULL;
    for (u64 i = 0; i < 400; ++i) {  // ~40% load, no line full
        x = x * 6364136223846793005ULL + 1442695040888963407ULL;
        u64 v = (x >> 20) | 1, *slot = NULL;
        ok64 o = HASHu64Probe(&slot, prb_idle, &v);
        if (o == HASHNONE) *slot = v;
        else want(o == OK && *slot == v);
        call(HASHu64Put, put_idle, &v);
    }
    want(0 == memcmp(put[0], prb[0], sizeof(u64) << 10));
    for (u64 const *p = prb[0]; p < prb[0] + (1 << 10); ++p) {
        if (!*p) continue;
        u64 *slot = NULL;
        want(OK == HASHu64Probe(&slot, prb_idle, p) && slot == p);
    }
    Bu64 buf = {};
    call(u64bMap, buf, 16);
    for (u64 k = 1; k <= 3000; ++k) {
        u64 *slot = NULL;
        want(HASHNONE == HASHu64ProbeGrow(&slot, buf, &k));
        *slot = k;
    }
    for (u64 k = 1; k <= 3000; ++k) {
        u64 v = k;
        call(HASHu64Get, &v, u64bIdle(buf));
    }
    call(u64bUnMap, buf);
    done;
}

ok64 RAPtest() {
    sane(1);
    u8c hello[] = "hello world";
    u8cs data = {hello, hello + 11};

    u64 h1 = RAPHash(data);
    u64 h2 = RAPMicro(data);
    u64 h3 = RAPNano(data);

    // Different variants may produce different hashes
    test(h1 != 0, FAIL);
    test(h2 != 0, FAIL);
    test(h3 != 0, FAIL);

    // Same input same hash
    test(h1 == RAPHash(data), FAIL);
    test(h2 == RAPMicro(data), FAIL);
    test(h3 == RAPNano(data), FAIL);

    // Seed changes hash
    test(h1 != RAPHashSeed(data, 42), FAIL);
    test(h2 != RAPMicroSeed(data, 42), FAIL);
    test(h3 != RAPNanoSeed(data, 42), FAIL);

    done;
}

ok64 HASH() {
    sane(1);
    call(HASH0);
    call(HASH1);
    call(HASH3);
    call(HASHd);
    call(HASHo);
    call(HASHgrow);
    call(HASHputgrow);
    call(HASHprobe);
    call(HASHprobegrow);
    call(HASHprobeconv);
    call(RAPtest);
    done;
}

TEST(HASH);
