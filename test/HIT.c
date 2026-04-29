#include "INT.h"
#include "PRO.h"
#include "TEST.h"

// Manual Swap for u64cs (array type, can't use Sx.h)
fun void u64csSwap(u64cs *a, u64cs *b) {
    u64c *t0 = (*a)[0], *t1 = (*a)[1];
    (*a)[0] = (*b)[0];
    (*a)[1] = (*b)[1];
    (*b)[0] = t0;
    (*b)[1] = t1;
}

#define X(M, name) M##u64##name
#include "HITx.h"
#undef X

// MSETx for cross-validation
#define X(M, name) M##u64##name
#include "MSETx.h"
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
    u64p out = buf;
    HITu64Merge(heap, &out);
    size_t olen = out - buf;
    // expected: 1,2,3,4,5,6,7,8,10
    testeq(olen, (size_t)9);
    testeq(buf[0], (u64)1);
    testeq(buf[1], (u64)2);
    testeq(buf[2], (u64)3);
    testeq(buf[3], (u64)4);
    testeq(buf[4], (u64)5);
    testeq(buf[5], (u64)6);
    testeq(buf[6], (u64)7);
    testeq(buf[7], (u64)8);
    testeq(buf[8], (u64)10);
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
    u64p hout = hbuf;
    HITu64Merge(hheap, &hout);
    size_t hlen = hout - hbuf;

    testeq(hlen, mlen);
    for (size_t i = 0; i < mlen; i++)
        testeq(hbuf[i], mbuf[i]);
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
    u64p out = buf;
    HITu64Intersect(heap, &out, 3);
    size_t olen = out - buf;
    // intersection = {3,5}
    testeq(olen, (size_t)2);
    testeq(buf[0], (u64)3);
    testeq(buf[1], (u64)5);
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
    testeq($len(heap), (size_t)2);
    // top should be minimum (1)
    testeq(*(*heap[0])[0], (u64)1);
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
    u64p hout = hbuf;
    HITu64Merge(hheap, &hout);
    size_t hlen = hout - hbuf;

    // MSET: same data, no empties
    u64cs mruns[3] = {{a, a + 3}, {b, b + 3}, {c, c + 3}};
    u64css miter = {mruns, mruns + 3};
    u64 mbuf[12];
    u64s minto = {mbuf, mbuf + 12};
    call(MSETu64Merge, minto, miter);
    size_t mlen = minto[0] - mbuf;

    testeq(hlen, mlen);
    for (size_t i = 0; i < mlen; i++)
        testeq(hbuf[i], mbuf[i]);
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
    testeq(o, OK);
    // top must be >= 5
    test(*(*heap[0])[0] >= 5, FAILSANITY);
    // drain and verify all values >= 5
    u64 buf[15];
    u64p out = buf;
    HITu64Merge(heap, &out);
    size_t olen = out - buf;
    for (size_t i = 0; i < olen; i++)
        test(buf[i] >= 5, FAILSANITY);
    // expected: 5,6,7,8,9,10,11,13
    testeq(olen, (size_t)8);
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
    u64p hout = hbuf;
    HITu64Merge(hheap, &hout);
    size_t hlen = hout - hbuf;

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

    testeq(hlen, mlen);
    for (size_t i = 0; i < mlen; i++)
        testeq(hbuf[i], mbuf[i]);
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
    testeq(o, NODATA);
    testeq($len(heap), (size_t)0);
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
    testeq(o, OK);
    // top should still be 5 (unchanged)
    testeq(*(*heap[0])[0], (u64)5);
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
    // top should be exactly 4
    testeq(*(*heap[0])[0], (u64)4);
    // drain rest
    u64 buf[8];
    u64p out = buf;
    HITu64Merge(heap, &out);
    size_t olen = out - buf;
    // expected: 4,5,6,7,8
    testeq(olen, (size_t)5);
    testeq(buf[0], (u64)4);
    testeq(buf[1], (u64)5);
    testeq(buf[2], (u64)6);
    testeq(buf[3], (u64)7);
    testeq(buf[4], (u64)8);
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
    testeq($len(heap), (size_t)0);
    u64 key = 1;
    ok64 o = HITu64Seek(heap, &key);
    testeq(o, NODATA);
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
    u64 buf[20]; u64p out = buf;
    HITu64sIntersectMerge(heap, &out);
    size_t n = out - buf;
    // {1,2,3,4,5,7} ∩ {1,2,3,5,6,8} = {1,2,3,5}
    testeq(n, (size_t)4);
    testeq(buf[0], (u64)1); testeq(buf[1], (u64)2);
    testeq(buf[2], (u64)3); testeq(buf[3], (u64)5);
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
    u64 buf[20]; u64p out = buf;
    HITu64sIntersectMerge(heap, &out);
    size_t n = out - buf;
    // {1..5} ∩ {2..6} ∩ {3..7} = {3,4,5}
    testeq(n, (size_t)3);
    testeq(buf[0], (u64)3); testeq(buf[1], (u64)4); testeq(buf[2], (u64)5);
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
    u64 buf[10]; u64p out = buf;
    HITu64sIntersectMerge(heap, &out);
    testeq((size_t)(out - buf), (size_t)0);
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
    u64 buf[10]; u64p out = buf;
    HITu64sIntersectMerge(heap, &out);
    size_t n = out - buf;
    testeq(n, (size_t)6);
    for (u64 i = 0; i < 6; i++) testeq(buf[i], i + 1);
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
    u64 buf[20]; u64p out = buf;
    HITu64sIntersectMerge(heap, &out);
    size_t n = out - buf;
    // {1,2,3,4} ∩ {2,3,4,5} = {2,3,4}
    testeq(n, (size_t)3);
    testeq(buf[0], (u64)2); testeq(buf[1], (u64)3); testeq(buf[2], (u64)4);
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
    u64 rbuf[20]; u64p rout = rbuf;
    HITu64sIntersectMerge(heap, &rout);
    size_t rlen = rout - rbuf;

    // Method 2: separate Merge each, then two-pointer intersect
    u64cs ra2[] = {{a1, a1 + 4}, {a2, a2 + 4}};
    u64css ha = {ra2, ra2 + 2};
    HITu64Start(ha);
    u64 ma[20]; u64p mao = ma;
    HITu64Merge(ha, &mao);
    size_t malen = mao - ma;

    u64cs rb2[] = {{b1, b1 + 6}, {b2, b2 + 3}};
    u64css hb = {rb2, rb2 + 2};
    HITu64Start(hb);
    u64 mb[20]; u64p mbo = mb;
    HITu64Merge(hb, &mbo);
    size_t mblen = mbo - mb;

    u64 ibuf[20]; size_t ilen = 0;
    size_t ia = 0, ib = 0;
    while (ia < malen && ib < mblen) {
        if (ma[ia] < mb[ib]) ia++;
        else if (ma[ia] > mb[ib]) ib++;
        else { ibuf[ilen++] = ma[ia]; ia++; ib++; }
    }

    testeq(rlen, ilen);
    for (size_t i = 0; i < ilen; i++)
        testeq(rbuf[i], ibuf[i]);
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
    u64 buf[10]; u64p out = buf;
    HITu64sIntersectMerge(heap, &out);
    // Only 1 non-empty inner HIT → intersection = merge = {1,2,3}
    size_t n = out - buf;
    testeq(n, (size_t)3);
    testeq(buf[0], (u64)1); testeq(buf[1], (u64)2); testeq(buf[2], (u64)3);
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
    u64 buf[10]; u64p out = buf;
    HITu64sIntersectMerge(heap, &out);
    size_t n = out - buf;
    // Both merge to {1,2,3,4,5}, intersection = {1,2,3,4,5}
    testeq(n, (size_t)5);
    for (u64 i = 0; i < 5; i++) testeq(buf[i], i + 1);
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
    testeq($len(stack), (size_t)2);
    testeq($len(stack[0][0]), (size_t)1000);
    testeq($len(stack[0][1]), (size_t)13);
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
    testeq($len(stack), (size_t)1);
    testeq($len(stack[0][0]), (size_t)30);
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
    testeq($len(stack), (size_t)1);
    testeq($len(stack[0][0]), (size_t)7);
    for (u64 i = 0; i < 7; i++) testeq(buf[i], i + 1);
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
    testeq($len(h_stack), $len(m_stack));
    for (size_t i = 0; i < $len(h_stack); i++) {
        testeq($len(h_stack[0][i]), $len(m_stack[0][i]));
    }
    size_t merged = $len(h_stack[0][$len(h_stack) - 1]);
    u64 const *h_run = h_stack[0][$len(h_stack) - 1][0];
    u64 const *m_run = m_stack[0][$len(m_stack) - 1][0];
    for (size_t i = 0; i < merged; i++) testeq(h_run[i], m_run[i]);
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
    done;
}

TEST(HITtest);
