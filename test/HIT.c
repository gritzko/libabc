#include "INT.h"
#include "KV.h"
#include "PRO.h"
#include "TEST.h"

// DOG-027: no csSwap prerequisite — the heap swaps entry pointers now.
#define X(M, name) M##u64##name
#include "HITx.h"
#undef X

// MSETx for cross-validation
#define X(M, name) M##u64##name
#include "MSETx.h"
#undef X

// DOG-027: keyed lane — kv64Z compares keys only, so equal keys are
// genuine ties and the tie winner is observable in the merged output.
// No csSwap needed: the heap swaps entry pointers now.
#define X(M, name) M##kv64##name
#include "HITx.h"
#undef X

// HIT0: merge 3 sorted runs -> sorted deduped
ok64 HIT0() {
    sane(1);
    u64 a[] = {1, 3, 5, 7};
    u64 b[] = {2, 4, 6, 8};
    u64 c[] = {1, 4, 7, 10};
    u64cs runs[3] = {{a, a + 4}, {b, b + 4}, {c, c + 4}};
    u64css heap = {runs, runs + 3};
    HITu64Start(heap);
    u64 buf[12];
    u64s out = {buf, buf + sizeof(buf) / sizeof(u64)};
    call(HITu64Merge, heap, out);
    size_t olen = out[0] - buf;
    // expected: 1,2,3,4,5,6,7,8,10
    testeqv((long long)(olen), (long long)((size_t)9), "%lld");
    testeqv((long long)(buf[0]), (long long)((u64)1), "%lld");
    testeqv((long long)(buf[1]), (long long)((u64)2), "%lld");
    testeqv((long long)(buf[2]), (long long)((u64)3), "%lld");
    testeqv((long long)(buf[3]), (long long)((u64)4), "%lld");
    testeqv((long long)(buf[4]), (long long)((u64)5), "%lld");
    testeqv((long long)(buf[5]), (long long)((u64)6), "%lld");
    testeqv((long long)(buf[6]), (long long)((u64)7), "%lld");
    testeqv((long long)(buf[7]), (long long)((u64)8), "%lld");
    testeqv((long long)(buf[8]), (long long)((u64)10), "%lld");
    done;
}

// HIT1: cross-validate merge vs MSETu64Merge
ok64 HIT1() {
    sane(1);
    u64 a[] = {1, 3, 5, 7, 9};
    u64 b[] = {2, 3, 6, 8, 10};
    u64 c[] = {1, 4, 5, 9, 11};

    // MSET merge
    u64cs mruns[3] = {{a, a + 5}, {b, b + 5}, {c, c + 5}};
    u64css miter = {mruns, mruns + 3};
    u64 mbuf[15];
    u64s minto = {mbuf, mbuf + 15};
    call(MSETu64Merge, minto, miter);
    size_t mlen = minto[0] - mbuf;

    // HIT merge
    u64cs hruns[3] = {{a, a + 5}, {b, b + 5}, {c, c + 5}};
    u64css hheap = {hruns, hruns + 3};
    HITu64Start(hheap);
    u64 hbuf[15];
    u64s hout = {hbuf, hbuf + sizeof(hbuf) / sizeof(u64)};
    call(HITu64Merge, hheap, hout);
    size_t hlen = hout[0] - hbuf;

    testeqv((long long)(hlen), (long long)(mlen), "%lld");
    for (size_t i = 0; i < mlen; i++)
        testeqv((long long)(hbuf[i]), (long long)(mbuf[i]), "%lld");
    done;
}

// HIT2: intersection of 3 runs
ok64 HIT2() {
    sane(1);
    u64 a[] = {1, 2, 3, 5, 7};
    u64 b[] = {2, 3, 4, 5, 8};
    u64 c[] = {1, 3, 5, 6, 9};
    u64cs runs[3] = {{a, a + 5}, {b, b + 5}, {c, c + 5}};
    u64css heap = {runs, runs + 3};
    HITu64Start(heap);
    u64 buf[15];
    u64s out = {buf, buf + sizeof(buf) / sizeof(u64)};
    call(HITu64Intersect, heap, out, 3);
    size_t olen = out[0] - buf;
    // intersection = {3,5}
    testeqv((long long)(olen), (long long)((size_t)2), "%lld");
    testeqv((long long)(buf[0]), (long long)((u64)3), "%lld");
    testeqv((long long)(buf[1]), (long long)((u64)5), "%lld");
    done;
}

// HIT3: Start filters empties, heap is correct
ok64 HIT3() {
    sane(1);
    u64 a[] = {1, 3, 5};
    u64 b[] = {2, 4, 6};
    u64cs runs[4] = {
        {a, a + 3},
        {NULL, NULL},       // empty
        {b, b + 3},
        {a + 3, a + 3},    // empty (begin == end)
    };
    u64css heap = {runs, runs + 4};
    HITu64Start(heap);
    testeqv((long long)($len(heap)), (long long)((size_t)2), "%lld");
    // top should be minimum (1)
    testeqv((long long)(*(*heap[0])[0]), (long long)((u64)1), "%lld");
    done;
}

// HIT4: Start with empties + merge, cross-validate vs MSET
ok64 HIT4() {
    sane(1);
    u64 a[] = {1, 5, 9};
    u64 b[] = {2, 5, 8};
    u64 c[] = {3, 6, 7};

    // HIT: with empties interspersed
    u64cs hruns[5] = {
        {a, a + 3},
        {NULL, NULL},
        {b, b + 3},
        {c + 3, c + 3},    // empty
        {c, c + 3},
    };
    u64css hheap = {hruns, hruns + 5};
    HITu64Start(hheap);
    u64 hbuf[12];
    u64s hout = {hbuf, hbuf + sizeof(hbuf) / sizeof(u64)};
    call(HITu64Merge, hheap, hout);
    size_t hlen = hout[0] - hbuf;

    // MSET: same data, no empties
    u64cs mruns[3] = {{a, a + 3}, {b, b + 3}, {c, c + 3}};
    u64css miter = {mruns, mruns + 3};
    u64 mbuf[12];
    u64s minto = {mbuf, mbuf + 12};
    call(MSETu64Merge, minto, miter);
    size_t mlen = minto[0] - mbuf;

    testeqv((long long)(hlen), (long long)(mlen), "%lld");
    for (size_t i = 0; i < mlen; i++)
        testeqv((long long)(hbuf[i]), (long long)(mbuf[i]), "%lld");
    done;
}

// HIT5: Seek basic — all entries advance past key
ok64 HIT5() {
    sane(1);
    u64 a[] = {1, 3, 5, 7, 9};
    u64 b[] = {2, 4, 6, 8, 10};
    u64 c[] = {3, 5, 7, 11, 13};
    u64cs runs[3] = {{a, a + 5}, {b, b + 5}, {c, c + 5}};
    u64css heap = {runs, runs + 3};
    HITu64Start(heap);
    u64 key = 5;
    ok64 o = HITu64Seek(heap, &key);
    testeqv((long long)(o), (long long)(OK), "%lld");
    // top must be >= 5
    test(*(*heap[0])[0] >= 5, FAILSANITY);
    // drain and verify all values >= 5
    u64 buf[15];
    u64s out = {buf, buf + sizeof(buf) / sizeof(u64)};
    call(HITu64Merge, heap, out);
    size_t olen = out[0] - buf;
    for (size_t i = 0; i < olen; i++)
        test(buf[i] >= 5, FAILSANITY);
    // expected: 5,6,7,8,9,10,11,13
    testeqv((long long)(olen), (long long)((size_t)8), "%lld");
    done;
}

// HIT6: Seek + merge, cross-validate vs MSET Seek
ok64 HIT6() {
    sane(1);
    u64 a[] = {10, 20, 30, 40, 50};
    u64 b[] = {15, 25, 35, 45, 55};
    u64 c[] = {12, 22, 32, 42, 52};

    // HIT: Start + Seek + merge
    u64cs hruns[3] = {{a, a + 5}, {b, b + 5}, {c, c + 5}};
    u64css hheap = {hruns, hruns + 3};
    HITu64Start(hheap);
    u64 key = 25;
    HITu64Seek(hheap, &key);
    u64 hbuf[15];
    u64s hout = {hbuf, hbuf + sizeof(hbuf) / sizeof(u64)};
    call(HITu64Merge, hheap, hout);
    size_t hlen = hout[0] - hbuf;

    // MSET: Start + Seek + drain
    u64cs mruns[3] = {{a, a + 5}, {b, b + 5}, {c, c + 5}};
    u64css miter = {mruns, mruns + 3};
    MSETu64Start(miter);
    MSETu64Seek(miter, 25);
    u64 mbuf[15];
    size_t mlen = 0;
    while (!$empty(miter)) {
        mbuf[mlen++] = ****miter;
        MSETu64Next(miter);
    }

    testeqv((long long)(hlen), (long long)(mlen), "%lld");
    for (size_t i = 0; i < mlen; i++)
        testeqv((long long)(hbuf[i]), (long long)(mbuf[i]), "%lld");
    done;
}

// HIT7: Seek past all data → NODATA
ok64 HIT7() {
    sane(1);
    u64 a[] = {1, 2, 3};
    u64 b[] = {4, 5, 6};
    u64cs runs[2] = {{a, a + 3}, {b, b + 3}};
    u64css heap = {runs, runs + 2};
    HITu64Start(heap);
    u64 key = 100;
    ok64 o = HITu64Seek(heap, &key);
    testeqv((long long)(o), (long long)(NODATA), "%lld");
    testeqv((long long)($len(heap)), (long long)((size_t)0), "%lld");
    done;
}

// HIT8: Seek to before all data (no-op)
ok64 HIT8() {
    sane(1);
    u64 a[] = {5, 10, 15};
    u64 b[] = {7, 12, 20};
    u64cs runs[2] = {{a, a + 3}, {b, b + 3}};
    u64css heap = {runs, runs + 2};
    HITu64Start(heap);
    u64 key = 1;
    ok64 o = HITu64Seek(heap, &key);
    testeqv((long long)(o), (long long)(OK), "%lld");
    // top should still be 5 (unchanged)
    testeqv((long long)(*(*heap[0])[0]), (long long)((u64)5), "%lld");
    done;
}

// HIT9: Seek to exact value present in data
ok64 HIT9() {
    sane(1);
    u64 a[] = {1, 3, 5, 7};
    u64 b[] = {2, 4, 6, 8};
    u64cs runs[2] = {{a, a + 4}, {b, b + 4}};
    u64css heap = {runs, runs + 2};
    HITu64Start(heap);
    u64 key = 4;
    HITu64Seek(heap, &key);
    // DOG-027: entries stay oldest-first (heap[0] is the FIRST run, not the
    // root) — the post-condition is that every surviving head is >= the key.
    for (u64cs *r = heap[0]; r < heap[1]; r++) test(*(*r)[0] >= key, FAILSANITY);
    // drain rest
    u64 buf[8];
    u64s out = {buf, buf + sizeof(buf) / sizeof(u64)};
    call(HITu64Merge, heap, out);
    size_t olen = out[0] - buf;
    // expected: 4,5,6,7,8
    testeqv((long long)(olen), (long long)((size_t)5), "%lld");
    testeqv((long long)(buf[0]), (long long)((u64)4), "%lld");
    testeqv((long long)(buf[1]), (long long)((u64)5), "%lld");
    testeqv((long long)(buf[2]), (long long)((u64)6), "%lld");
    testeqv((long long)(buf[3]), (long long)((u64)7), "%lld");
    testeqv((long long)(buf[4]), (long long)((u64)8), "%lld");
    done;
}

// HIT10: Start on all-empty → empty heap
ok64 HIT10() {
    sane(1);
    u64cs runs[3] = {
        {NULL, NULL},
        {NULL, NULL},
        {NULL, NULL},
    };
    u64css heap = {runs, runs + 3};
    HITu64Start(heap);
    testeqv((long long)($len(heap)), (long long)((size_t)0), "%lld");
    u64 key = 1;
    ok64 o = HITu64Seek(heap, &key);
    testeqv((long long)(o), (long long)(NODATA), "%lld");
    done;
}

// HIT11: sIntersectMerge — 2 inner HITs with partial overlap
ok64 HIT11() {
    sane(1);
    // HIT a: runs [1,3,5,7] + [2,4] → merged {1,2,3,4,5,7}
    u64 a1[] = {1, 3, 5, 7};
    u64 a2[] = {2, 4};
    u64cs ra[] = {{a1, a1 + 4}, {a2, a2 + 2}};
    // HIT b: runs [2,3,6] + [1,5,8] → merged {1,2,3,5,6,8}
    u64 b1[] = {2, 3, 6};
    u64 b2[] = {1, 5, 8};
    u64cs rb[] = {{b1, b1 + 3}, {b2, b2 + 3}};

    u64cs *oh[2][2];
    oh[0][0] = ra; oh[0][1] = ra + 2;
    oh[1][0] = rb; oh[1][1] = rb + 2;
    HITu64Start(oh[0]);
    HITu64Start(oh[1]);

    u64csss heap = {oh, oh + 2};
    u64 buf[20]; u64s out = {buf, buf + sizeof(buf) / sizeof(u64)};
    call(HITu64sIntersectMerge, heap, out);
    size_t n = out[0] - buf;
    // {1,2,3,4,5,7} ∩ {1,2,3,5,6,8} = {1,2,3,5}
    testeqv((long long)(n), (long long)((size_t)4), "%lld");
    testeqv((long long)(buf[0]), (long long)((u64)1), "%lld"); testeqv((long long)(buf[1]), (long long)((u64)2), "%lld");
    testeqv((long long)(buf[2]), (long long)((u64)3), "%lld"); testeqv((long long)(buf[3]), (long long)((u64)5), "%lld");
    done;
}

// HIT12: sIntersectMerge — 3 inner HITs
ok64 HIT12() {
    sane(1);
    u64 a[] = {1, 2, 3, 4, 5};
    u64 b[] = {2, 3, 4, 5, 6};
    u64 c[] = {3, 4, 5, 6, 7};
    u64cs ra[] = {{a, a + 5}};
    u64cs rb[] = {{b, b + 5}};
    u64cs rc[] = {{c, c + 5}};

    u64cs *oh[3][2];
    oh[0][0] = ra; oh[0][1] = ra + 1;
    oh[1][0] = rb; oh[1][1] = rb + 1;
    oh[2][0] = rc; oh[2][1] = rc + 1;
    HITu64Start(oh[0]);
    HITu64Start(oh[1]);
    HITu64Start(oh[2]);

    u64csss heap = {oh, oh + 3};
    u64 buf[20]; u64s out = {buf, buf + sizeof(buf) / sizeof(u64)};
    call(HITu64sIntersectMerge, heap, out);
    size_t n = out[0] - buf;
    // {1..5} ∩ {2..6} ∩ {3..7} = {3,4,5}
    testeqv((long long)(n), (long long)((size_t)3), "%lld");
    testeqv((long long)(buf[0]), (long long)((u64)3), "%lld"); testeqv((long long)(buf[1]), (long long)((u64)4), "%lld"); testeqv((long long)(buf[2]), (long long)((u64)5), "%lld");
    done;
}

// HIT13: sIntersectMerge — no common elements → empty
ok64 HIT13() {
    sane(1);
    u64 a[] = {1, 2, 3};
    u64 b[] = {4, 5, 6};
    u64cs ra[] = {{a, a + 3}};
    u64cs rb[] = {{b, b + 3}};

    u64cs *oh[2][2];
    oh[0][0] = ra; oh[0][1] = ra + 1;
    oh[1][0] = rb; oh[1][1] = rb + 1;
    HITu64Start(oh[0]);
    HITu64Start(oh[1]);

    u64csss heap = {oh, oh + 2};
    u64 buf[10]; u64s out = {buf, buf + sizeof(buf) / sizeof(u64)};
    call(HITu64sIntersectMerge, heap, out);
    testeqv((long long)((size_t)(out[0] - buf)), (long long)((size_t)0), "%lld");
    done;
}

// HIT14: sIntersectMerge — single inner HIT equals Merge
ok64 HIT14() {
    sane(1);
    u64 a1[] = {1, 3, 5};
    u64 a2[] = {2, 4, 6};
    u64cs ra[] = {{a1, a1 + 3}, {a2, a2 + 3}};

    u64cs *oh[1][2];
    oh[0][0] = ra; oh[0][1] = ra + 2;
    HITu64Start(oh[0]);

    u64csss heap = {oh, oh + 1};
    u64 buf[10]; u64s out = {buf, buf + sizeof(buf) / sizeof(u64)};
    call(HITu64sIntersectMerge, heap, out);
    size_t n = out[0] - buf;
    testeqv((long long)(n), (long long)((size_t)6), "%lld");
    for (u64 i = 0; i < 6; i++) testeqv((long long)(buf[i]), (long long)(i + 1), "%lld");
    done;
}

// HIT15: sIntersectMerge — duplicates within runs
ok64 HIT15() {
    sane(1);
    // HIT a: [1,2,3] + [2,3,4] → merged {1,2,3,4}
    u64 a1[] = {1, 2, 3};
    u64 a2[] = {2, 3, 4};
    u64cs ra[] = {{a1, a1 + 3}, {a2, a2 + 3}};
    // HIT b: [2,2,3] + [3,4,5] → merged {2,3,4,5}
    u64 b1[] = {2, 2, 3};
    u64 b2[] = {3, 4, 5};
    u64cs rb[] = {{b1, b1 + 3}, {b2, b2 + 3}};

    u64cs *oh[2][2];
    oh[0][0] = ra; oh[0][1] = ra + 2;
    oh[1][0] = rb; oh[1][1] = rb + 2;
    HITu64Start(oh[0]);
    HITu64Start(oh[1]);

    u64csss heap = {oh, oh + 2};
    u64 buf[20]; u64s out = {buf, buf + sizeof(buf) / sizeof(u64)};
    call(HITu64sIntersectMerge, heap, out);
    size_t n = out[0] - buf;
    // {1,2,3,4} ∩ {2,3,4,5} = {2,3,4}
    testeqv((long long)(n), (long long)((size_t)3), "%lld");
    testeqv((long long)(buf[0]), (long long)((u64)2), "%lld"); testeqv((long long)(buf[1]), (long long)((u64)3), "%lld"); testeqv((long long)(buf[2]), (long long)((u64)4), "%lld");
    done;
}

// HIT16: sIntersectMerge — cross-validate against Merge + manual intersect
ok64 HIT16() {
    sane(1);
    u64 a1[] = {1, 4, 7, 10};
    u64 a2[] = {2, 5, 8, 11};
    u64 b1[] = {1, 3, 5, 7, 9, 11};
    u64 b2[] = {2, 4, 6};

    // Method 1: sIntersectMerge
    u64cs ra1[] = {{a1, a1 + 4}, {a2, a2 + 4}};
    u64cs rb1[] = {{b1, b1 + 6}, {b2, b2 + 3}};
    u64cs *oh[2][2];
    oh[0][0] = ra1; oh[0][1] = ra1 + 2;
    oh[1][0] = rb1; oh[1][1] = rb1 + 2;
    HITu64Start(oh[0]);
    HITu64Start(oh[1]);
    u64csss heap = {oh, oh + 2};
    u64 rbuf[20]; u64s rout = {rbuf, rbuf + sizeof(rbuf) / sizeof(u64)};
    call(HITu64sIntersectMerge, heap, rout);
    size_t rlen = rout[0] - rbuf;

    // Method 2: separate Merge each, then two-pointer intersect
    u64cs ra2[] = {{a1, a1 + 4}, {a2, a2 + 4}};
    u64css ha = {ra2, ra2 + 2};
    HITu64Start(ha);
    u64 ma[20]; u64s mao = {ma, ma + sizeof(ma) / sizeof(u64)};
    call(HITu64Merge, ha, mao);
    size_t malen = mao[0] - ma;

    u64cs rb2[] = {{b1, b1 + 6}, {b2, b2 + 3}};
    u64css hb = {rb2, rb2 + 2};
    HITu64Start(hb);
    u64 mb[20]; u64s mbo = {mb, mb + sizeof(mb) / sizeof(u64)};
    call(HITu64Merge, hb, mbo);
    size_t mblen = mbo[0] - mb;

    u64 ibuf[20]; size_t ilen = 0;
    size_t ia = 0, ib = 0;
    while (ia < malen && ib < mblen) {
        if (ma[ia] < mb[ib]) ia++;
        else if (ma[ia] > mb[ib]) ib++;
        else { ibuf[ilen++] = ma[ia]; ia++; ib++; }
    }

    testeqv((long long)(rlen), (long long)(ilen), "%lld");
    for (size_t i = 0; i < ilen; i++)
        testeqv((long long)(rbuf[i]), (long long)(ibuf[i]), "%lld");
    done;
}

// HIT17: sIntersectMerge — empty inner HITs filtered out
ok64 HIT17() {
    sane(1);
    u64 a[] = {1, 2, 3};
    u64cs ra[] = {{a, a + 3}};

    u64cs *oh[3][2];
    oh[0][0] = ra; oh[0][1] = ra + 1;
    oh[1][0] = NULL; oh[1][1] = NULL;  // empty
    oh[2][0] = ra; oh[2][1] = ra + 0;  // empty (head==tail)
    HITu64Start(oh[0]);

    u64csss heap = {oh, oh + 3};
    u64 buf[10]; u64s out = {buf, buf + sizeof(buf) / sizeof(u64)};
    call(HITu64sIntersectMerge, heap, out);
    // Only 1 non-empty inner HIT → intersection = merge = {1,2,3}
    size_t n = out[0] - buf;
    testeqv((long long)(n), (long long)((size_t)3), "%lld");
    testeqv((long long)(buf[0]), (long long)((u64)1), "%lld"); testeqv((long long)(buf[1]), (long long)((u64)2), "%lld"); testeqv((long long)(buf[2]), (long long)((u64)3), "%lld");
    done;
}

// HIT18: sIntersectMerge — identical inner HITs → same as merge
ok64 HIT18() {
    sane(1);
    u64 d1[] = {1, 3, 5};
    u64 d2[] = {2, 4};
    u64cs r1[] = {{d1, d1 + 3}, {d2, d2 + 2}};
    u64cs r2[] = {{d1, d1 + 3}, {d2, d2 + 2}};

    u64cs *oh[2][2];
    oh[0][0] = r1; oh[0][1] = r1 + 2;
    oh[1][0] = r2; oh[1][1] = r2 + 2;
    HITu64Start(oh[0]);
    HITu64Start(oh[1]);

    u64csss heap = {oh, oh + 2};
    u64 buf[10]; u64s out = {buf, buf + sizeof(buf) / sizeof(u64)};
    call(HITu64sIntersectMerge, heap, out);
    size_t n = out[0] - buf;
    // Both merge to {1,2,3,4,5}, intersection = {1,2,3,4,5}
    testeqv((long long)(n), (long long)((size_t)5), "%lld");
    for (u64 i = 0; i < 5; i++) testeqv((long long)(buf[i]), (long long)(i + 1), "%lld");
    done;
}

// HIT19: IsCompact predicate.
ok64 HIT19() {
    sane(1);
    // oldest-first: 100, 10, 1 — each < 1/8 of preceding → compact.
    u64 a[100], b[10], c[1];
    for (int i = 0; i < 100; i++) a[i] = (u64)i;
    for (int i = 0; i < 10; i++) b[i] = (u64)(100 + i);
    c[0] = 200;
    u64cs runs[3] = {{a, a + 100}, {b, b + 10}, {c, c + 1}};
    u64css stack = {runs, runs + 3};
    want(HITu64IsCompact(stack) == YES);

    // 8, 8 — second not < 1/8 of first → not compact.
    u64 d[8], e[8];
    for (int i = 0; i < 8; i++) d[i] = (u64)i;
    for (int i = 0; i < 8; i++) e[i] = (u64)(8 + i);
    u64cs runs2[2] = {{d, d + 8}, {e, e + 8}};
    u64css stack2 = {runs2, runs2 + 2};
    want(HITu64IsCompact(stack2) == NO);

    // Single run is trivially compact.
    u64cs runs3[1] = {{a, a + 100}};
    u64css stack3 = {runs3, runs3 + 1};
    want(HITu64IsCompact(stack3) == YES);
    done;
}

// HIT20: Compact merges youngest violators, restoring 1/8 invariant.
//        big(1000), med(5), sml(8): 8*8=64 > 5 → merge sml+med → 13;
//        13*8=104 < 1000 → stop.
ok64 HIT20() {
    sane(1);
    u64 big[1000];
    for (int i = 0; i < 1000; i++) big[i] = (u64)(i * 3);
    u64 med[] = {1, 4, 7, 10, 13};
    u64 sml[] = {0, 2, 5, 8, 11, 14, 17, 20};
    u64cs runs[3] = {{big, big + 1000}, {med, med + 5}, {sml, sml + 8}};
    u64css stack = {runs, runs + 3};
    want(HITu64IsCompact(stack) == NO);

    u64 buf[13];
    u64s into = {buf, buf + 13};
    call(HITu64Compact, stack, into);
    testeqv((long long)($len(stack)), (long long)((size_t)2), "%lld");
    testeqv((long long)($len(stack[0][0])), (long long)((size_t)1000), "%lld");
    testeqv((long long)($len(stack[0][1])), (long long)((size_t)13), "%lld");
    want(HITu64IsCompact(stack) == YES);
    for (int i = 0; i + 1 < 13; i++) want(buf[i] <= buf[i + 1]);
    done;
}

// HIT21: cascading — 10,10,10 all equal → all three merge into one 30-run.
ok64 HIT21() {
    sane(1);
    u64 a[10], b[10], c[10];
    for (int i = 0; i < 10; i++) a[i] = (u64)(i * 3);
    for (int i = 0; i < 10; i++) b[i] = (u64)(i * 3 + 1);
    for (int i = 0; i < 10; i++) c[i] = (u64)(i * 3 + 2);
    u64cs runs[3] = {{a, a + 10}, {b, b + 10}, {c, c + 10}};
    u64css stack = {runs, runs + 3};
    u64 buf[30];
    u64s into = {buf, buf + 30};
    call(HITu64Compact, stack, into);
    testeqv((long long)($len(stack)), (long long)((size_t)1), "%lld");
    testeqv((long long)($len(stack[0][0])), (long long)((size_t)30), "%lld");
    want(HITu64IsCompact(stack) == YES);
    for (int i = 0; i + 1 < 30; i++) want(buf[i] <= buf[i + 1]);
    done;
}

// HIT22: dedup across runs — duplicates in different runs collapse.
ok64 HIT22() {
    sane(1);
    u64 a[] = {1, 2, 3, 4, 5};   // 5
    u64 b[] = {2, 4, 6};          // 3
    u64 c[] = {3, 5, 7};          // 3
    u64cs runs[3] = {{a, a + 5}, {b, b + 3}, {c, c + 3}};
    u64css stack = {runs, runs + 3};
    u64 buf[11];
    u64s into = {buf, buf + 11};
    call(HITu64Compact, stack, into);
    // Single sorted, deduped run: {1,2,3,4,5,6,7} — 7 distinct.
    testeqv((long long)($len(stack)), (long long)((size_t)1), "%lld");
    testeqv((long long)($len(stack[0][0])), (long long)((size_t)7), "%lld");
    for (u64 i = 0; i < 7; i++) testeqv((long long)(buf[i]), (long long)(i + 1), "%lld");
    done;
}

// HIT23: cross-validate Compact's output against MSETCompact on the same input.
ok64 HIT23() {
    sane(1);
    u64 a[] = {1, 4, 7, 10, 13, 16, 19, 22};
    u64 b[] = {2, 5, 8};
    u64 c[] = {3, 6, 9, 12};

    u64cs h_runs[3] = {{a, a + 8}, {b, b + 3}, {c, c + 4}};
    u64css h_stack = {h_runs, h_runs + 3};
    u64 h_buf[15];
    u64s h_into = {h_buf, h_buf + 15};
    call(HITu64Compact, h_stack, h_into);

    u64cs m_runs[3] = {{a, a + 8}, {b, b + 3}, {c, c + 4}};
    u64css m_stack = {m_runs, m_runs + 3};
    u64 m_buf[15];
    u64s m_into = {m_buf, m_buf + 15};
    call(MSETu64Compact, m_stack, m_into);

    // Both compactors produce the same shape and same merged contents.
    testeqv((long long)($len(h_stack)), (long long)($len(m_stack)), "%lld");
    for (i64 i = 0; i < $len(h_stack); i++) {
        testeqv((long long)($len(h_stack[0][i])), (long long)($len(m_stack[0][i])), "%lld");
    }
    size_t merged = $len(h_stack[0][$len(h_stack) - 1]);
    u64 const *h_run = h_stack[0][$len(h_stack) - 1][0];
    u64 const *m_run = m_stack[0][$len(m_stack) - 1][0];
    for (size_t i = 0; i < merged; i++) testeqv((long long)(h_run[i]), (long long)(m_run[i]), "%lld");
    done;
}

// ABC-015: drains take a bounded slice now; a too-small output must
// return OKNOROOM instead of writing past the end.
ok64 HIT24() {
    sane(1);
    u64 a[] = {1, 3, 5, 7};
    u64 b[] = {2, 4, 6, 8};
    u64cs runs[2] = {{a, a + 4}, {b, b + 4}};
    u64css heap = {runs, runs + 2};
    HITu64Start(heap);
    u64 buf[3];
    u64s out = {buf, buf + 3};
    testeqv((long long)(HITu64Merge(heap, out)), (long long)(OKNOROOM),
            "%lld");
    // the room it had was filled in order
    testeqv((long long)(buf[0]), (long long)((u64)1), "%lld");
    testeqv((long long)(buf[2]), (long long)((u64)3), "%lld");
    // Compact propagates NOROOM for an undersized `into`
    u64 c[8], d[8];
    for (int i = 0; i < 8; i++) c[i] = (u64)i, d[i] = (u64)(8 + i);
    u64cs runs2[2] = {{c, c + 8}, {d, d + 8}};
    u64css stack = {runs2, runs2 + 2};
    u64 small[4];
    u64s into = {small, small + 4};
    testeqv((long long)(HITu64Compact(stack, into)), (long long)(OKNOROOM),
            "%lld");
    done;
}

// HIT25: keyed kv64 Merge ties — the youngest run (highest entry in the
// oldest-first array) must win; table-driven.
ok64 HIT25() {
    sane(1);
    typedef struct {
        kv64 runs[3][4];  // up to 3 runs, oldest-first
        size_t len[3];    // per-run length, 0 = run absent
        kv64 want[8];
        size_t wlen;
    } kase;
    static kase const K[] = {
        //  cross-run winner by age: key 1 in all three runs
        {{{{1, 10}, {3, 30}}, {{1, 11}}, {{1, 12}, {2, 22}}},
         {2, 1, 2},
         {{1, 12}, {2, 22}, {3, 30}},
         3},
        //  tie group of three on key 7, youngest val 73 wins
        {{{{5, 51}, {7, 71}}, {{7, 72}}, {{7, 73}, {9, 93}}},
         {2, 1, 2},
         {{5, 51}, {7, 73}, {9, 93}},
         3},
        //  two-run tie amid untied keys
        {{{{2, 20}, {4, 40}, {6, 60}}, {{4, 44}}, {{0, 0}}},
         {3, 1, 0},
         {{2, 20}, {4, 44}, {6, 60}},
         3},
    };
    for (size_t k = 0; k < sizeof(K) / sizeof(K[0]); k++) {
        kv64cs runs[3];
        size_t n = 0;
        for (size_t r = 0; r < 3; r++) {
            if (K[k].len[r] == 0) continue;
            runs[n][0] = K[k].runs[r];
            runs[n][1] = K[k].runs[r] + K[k].len[r];
            n++;
        }
        kv64css heap = {runs, runs + n};
        HITkv64Start(heap);
        kv64 buf[8];
        kv64s out = {buf, buf + 8};
        call(HITkv64Merge, heap, out);
        size_t olen = out[0] - buf;
        testeqv((long long)(olen), (long long)(K[k].wlen), "%lld");
        for (size_t i = 0; i < K[k].wlen; i++) {
            testeqv((long long)(buf[i].key), (long long)(K[k].want[i].key),
                    "%lld");
            testeqv((long long)(buf[i].val), (long long)(K[k].want[i].val),
                    "%lld");
        }
    }
    done;
}

// HIT26: keyed kv64 Compact — the compaction drain resolves ties the
// same way (youngest run wins), so query and compaction agree.
ok64 HIT26() {
    sane(1);
    kv64 old[] = {{1, 10}, {5, 50}};
    kv64 yng[] = {{1, 11}, {9, 90}};
    kv64cs runs[2] = {{old, old + 2}, {yng, yng + 2}};
    kv64css stack = {runs, runs + 2};
    kv64 buf[4];
    kv64s into = {buf, buf + 4};
    call(HITkv64Compact, stack, into);
    testeqv((long long)($len(stack)), (long long)((size_t)1), "%lld");
    testeqv((long long)($len(stack[0][0])), (long long)((size_t)3), "%lld");
    testeqv((long long)(buf[0].key), (long long)((u64)1), "%lld");
    testeqv((long long)(buf[0].val), (long long)((u64)11), "%lld");
    testeqv((long long)(buf[1].key), (long long)((u64)5), "%lld");
    testeqv((long long)(buf[1].val), (long long)((u64)50), "%lld");
    testeqv((long long)(buf[2].key), (long long)((u64)9), "%lld");
    testeqv((long long)(buf[2].val), (long long)((u64)90), "%lld");
    done;
}

// DOG-027: one cap (HIT_MAX_RUNS) for every entry point.  Above it the leaf
// just refuses — no windowing, no repair cascade, no youngest-N batching.
static void hit27fill(u64 *v, u64cs *runs, u64css st, size_t n) {
    for (size_t i = 0; i < n; i++) {
        v[i] = (u64)i;
        runs[i][0] = v + i;
        runs[i][1] = v + i + 1;
    }
    st[0] = runs;
    st[1] = runs + n;
}

ok64 HIT27() {
    sane(1);
    u64 v[HIT_MAX_RUNS + 1];
    u64cs runs[HIT_MAX_RUNS + 1];
    u64 buf[HIT_MAX_RUNS + 1];
    u64css st;
    u64s out;
    u64 lo = 0, hi = 1000, key = 3;
    size_t over = HIT_MAX_RUNS + 1;

    // --- one run past the cap: every entry point returns HITTOOMANY ---
#define HIT27OVER(EXPR)                                                     \
    hit27fill(v, runs, st, over);                                           \
    out[0] = buf;                                                           \
    out[1] = buf + over;                                                    \
    testeqv((long long)(EXPR), (long long)(HITTOOMANY), "%lld")
    HIT27OVER(HITu64Merge(st, out));
    HIT27OVER(HITu64MergeBag(st, out));
    HIT27OVER(HITu64Intersect(st, out, over));
    HIT27OVER(HITu64Seek(st, &key));
    HIT27OVER(HITu64SeekRange(st, &lo, &hi));
    HIT27OVER(HITu64Compact(st, out));
    HIT27OVER(HITu64SkipValue(st));
#undef HIT27OVER

    // --- exactly at the cap: every entry point works ---
#define HIT27AT(EXPR)                                                       \
    hit27fill(v, runs, st, (size_t)HIT_MAX_RUNS);                           \
    out[0] = buf;                                                           \
    out[1] = buf + HIT_MAX_RUNS;                                            \
    testeqv((long long)(EXPR), (long long)(OK), "%lld")
    HIT27AT(HITu64Merge(st, out));
    testeqv((long long)((size_t)(out[0] - buf)), (long long)((size_t)HIT_MAX_RUNS),
            "%lld");
    HIT27AT(HITu64MergeBag(st, out));
    HIT27AT(HITu64Intersect(st, out, (size_t)HIT_MAX_RUNS));
    HIT27AT(HITu64Seek(st, &key));
    HIT27AT(HITu64SeekRange(st, &lo, &hi));
    HIT27AT(HITu64Compact(st, out));
    testeqv((long long)($len(st)), (long long)((size_t)1), "%lld");
    HIT27AT(HITu64SkipValue(st));
#undef HIT27AT
    done;
}

ok64 HITtest() {
    sane(1);
    call(HIT0);
    call(HIT1);
    call(HIT2);
    call(HIT3);
    call(HIT4);
    call(HIT5);
    call(HIT6);
    call(HIT7);
    call(HIT8);
    call(HIT9);
    call(HIT10);
    call(HIT11);
    call(HIT12);
    call(HIT13);
    call(HIT14);
    call(HIT15);
    call(HIT16);
    call(HIT17);
    call(HIT18);
    call(HIT19);
    call(HIT20);
    call(HIT21);
    call(HIT22);
    call(HIT23);
    call(HIT24);
    call(HIT25);
    call(HIT26);
    call(HIT27);
    done;
}

TEST(HITtest);
