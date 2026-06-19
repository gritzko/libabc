//  ABC-004: table-driven tests for abc/BIT.h (u1 bitmap + set algebra).
//  Includes a REGRESSION case proving "clear bit N" leaves all other
//  bits untouched — the exact old BitUnset `|= ~(1<<bit)` bug.

#include <stdio.h>

#include "01.h"
#include "BIT.h"
#include "OK.h"
#include "PRO.h"
#include "TEST.h"

//  ---- a small reference set model over u32 ----------------------------
#define REFMAX 4096
typedef struct ref_s {
    u8 bit[REFMAX];
    u32 n;
} ref;

fun void refClr(ref *r, u32 n) {
    r->n = n;
    for (u32 i = 0; i < n; i++) r->bit[i] = 0;
}
fun void refLoad(ref *r, u1cs s) {
    refClr(r, u1sLen(s));
    for (u32 i = 0; i < r->n; i++) r->bit[i] = (u8)u1At(s, i);
}
fun b8 refEq(ref const *r, u1cs s) {
    if (r->n != u1sLen(s)) return NO;
    for (u32 i = 0; i < r->n; i++)
        if (r->bit[i] != (u8)u1At(s, i)) return NO;
    return YES;
}

//  ---- element get/set/clr/put -----------------------------------------
ok64 BITelem_test() {
    sane(1);
    u1b b = {};
    call(u1bMap, &b, 200);
    for (int i = 0; i < 200; i++) call(u1bFeed1, &b, 0);
    u1s s = u1bData(&b);
    testeqv((long long)u1sLen(u1sConst(s)), 200LL, "%lld");

    //  Set a scattered pattern of small ints, verify membership.
    u32 want[] = {0, 1, 63, 64, 65, 127, 128, 199};
    for (size_t i = 0; i < sizeof(want) / sizeof(want[0]); i++)
        u1sSet(s, want[i]);
    for (int i = 0; i < 200; i++) {
        b8 expect = NO;
        for (size_t k = 0; k < sizeof(want) / sizeof(want[0]); k++)
            if (want[k] == (u32)i) expect = YES;
        testeqv((long long)u1At(u1sConst(s), i), (long long)expect, "%lld");
    }
    testeqv((long long)u1sCount(u1sConst(s)),
            (long long)(sizeof(want) / sizeof(want[0])), "%lld");

    //  u1sPut both ways.
    u1sPut(s, 100, 1);
    testeqv((long long)u1At(u1sConst(s), 100), 1LL, "%lld");
    u1sPut(s, 100, 0);
    testeqv((long long)u1At(u1sConst(s), 100), 0LL, "%lld");
    call(u1bUnMap, &b);
    done;
}

//  REGRESSION (ABC-004): clearing bit N must leave EVERY other bit as-is.
//  The old BUF.h BitUnset did `|= ~(1<<bit)`, which SET all the other
//  bits in the byte instead of clearing the target.  Set all 200 bits,
//  clear them one at a time, and assert the rest stay set.
ok64 BITclr_regression_test() {
    sane(1);
    u1b b = {};
    call(u1bMap, &b, 200);
    for (int i = 0; i < 200; i++) call(u1bFeed1, &b, 1);
    u1s s = u1bData(&b);
    testeqv((long long)u1sCount(u1sConst(s)), 200LL, "%lld");
    for (u32 clr = 0; clr < 200; clr++) {
        u1sClr(s, clr);
        //  Exactly one bit gone; bit `clr` is 0, all others still 1.
        testeqv((long long)u1At(u1sConst(s), clr), 0LL, "%lld");
        for (u32 j = clr + 1; j < 200; j++)
            testeqv((long long)u1At(u1sConst(s), j), 1LL, "%lld");
        u1sSet(s, clr);  // restore for the next round
    }
    call(u1bUnMap, &b);
    done;
}

//  ---- tail-mask: junk bits past nbits never count ---------------------
//  Allocate 64-bit-word backing but expose only 10 bits; smear the
//  backing word full, then a 10-bit view must report only its 10 bits.
ok64 BITtail_test() {
    sane(1);
    u1b b = {};
    call(u1bMap, &b, 64);
    for (int i = 0; i < 64; i++) call(u1bFeed1, &b, 1);  // word all-ones
    //  A 10-bit slice over the same all-ones word.
    u1cs ten = {(u8 const *)b.words[0], 10};
    testeqv((long long)u1sCount(ten), 10LL, "%lld");
    testeqv((long long)u1sAny(ten), 1LL, "%lld");
    testeqv((long long)u1sNext(ten, 0), 0LL, "%lld");
    testeqv((long long)u1sNext(ten, 9), 9LL, "%lld");
    testeqv((long long)u1sNext(ten, 10), 10LL, "%lld");  // none past 10
    call(u1bUnMap, &b);
    done;
}

//  ---- algebra vs reference model --------------------------------------
ok64 BITalgebra_test() {
    sane(1);
    enum { N = 130 };
    u1b ba = {}, bb = {};
    call(u1bMap, &ba, N);
    call(u1bMap, &bb, N);
    for (int i = 0; i < N; i++) { call(u1bFeed1, &ba, 0); call(u1bFeed1, &bb, 0); }
    u1s a = u1bData(&ba), b = u1bData(&bb);
    //  a = multiples of 3, b = multiples of 5 (within N).
    for (u32 i = 0; i < N; i++) { if (i % 3 == 0) u1sSet(a, i); if (i % 5 == 0) u1sSet(b, i); }

    ref ra = {}, rb = {};
    refLoad(&ra, u1sConst(a));
    refLoad(&rb, u1sConst(b));

    //  Or: union.
    {
        u1b bd = {}; call(u1bMap, &bd, N);
        for (int i = 0; i < N; i++) call(u1bFeed1, &bd, 0);
        u1s d = u1bData(&bd);
        call(u1sOr, d, u1sConst(a)); want(u1sEq(u1sConst(d), u1sConst(a)));
        call(u1sOr, d, u1sConst(b));
        ref rd; refClr(&rd, N);
        for (u32 i = 0; i < N; i++) rd.bit[i] = ra.bit[i] | rb.bit[i];
        want(refEq(&rd, u1sConst(d)));
        call(u1bUnMap, &bd);
    }
    //  And: intersection.
    {
        u1b bd = {}; call(u1bMap, &bd, N);
        for (int i = 0; i < N; i++) call(u1bFeed1, &bd, 0);
        u1s d = u1bData(&bd);
        call(u1sOr, d, u1sConst(a));
        call(u1sAnd, d, u1sConst(b));
        ref rd; refClr(&rd, N);
        for (u32 i = 0; i < N; i++) rd.bit[i] = ra.bit[i] & rb.bit[i];
        want(refEq(&rd, u1sConst(d)));
        call(u1bUnMap, &bd);
    }
    //  AndNot: difference a \ b.
    {
        u1b bd = {}; call(u1bMap, &bd, N);
        for (int i = 0; i < N; i++) call(u1bFeed1, &bd, 0);
        u1s d = u1bData(&bd);
        call(u1sOr, d, u1sConst(a));
        call(u1sAndNot, d, u1sConst(b));
        ref rd; refClr(&rd, N);
        for (u32 i = 0; i < N; i++) rd.bit[i] = ra.bit[i] & ~rb.bit[i] & 1;
        want(refEq(&rd, u1sConst(d)));
        call(u1bUnMap, &bd);
    }
    //  Xor: symmetric difference.
    {
        u1b bd = {}; call(u1bMap, &bd, N);
        for (int i = 0; i < N; i++) call(u1bFeed1, &bd, 0);
        u1s d = u1bData(&bd);
        call(u1sOr, d, u1sConst(a));
        call(u1sXor, d, u1sConst(b));
        ref rd; refClr(&rd, N);
        for (u32 i = 0; i < N; i++) rd.bit[i] = ra.bit[i] ^ rb.bit[i];
        want(refEq(&rd, u1sConst(d)));
        call(u1bUnMap, &bd);
    }
    //  Count + Next iteration over `a` matches the reference.
    {
        u32 rc = 0; for (u32 i = 0; i < N; i++) rc += ra.bit[i];
        testeqv((long long)u1sCount(u1sConst(a)), (long long)rc, "%lld");
        u32 seen = 0;
        u1$for (i, u1sConst(a)) { want(ra.bit[i]); seen++; }
        testeqv((long long)seen, (long long)rc, "%lld");
    }
    //  Length-mismatch guard.
    {
        u1cs shorter = {(u8 const *)bb.words[0], N - 1};
        testeqv((long long)u1sOr(a, shorter), (long long)BITLEN, "%lld");
        want(!u1sEq(u1sConst(a), shorter));
    }
    call(u1bUnMap, &ba);
    call(u1bUnMap, &bb);
    done;
}

//  ---- buffer family: Feed1 / DataLen / Reset / Idle -------------------
ok64 BITbuf_test() {
    sane(1);
    u1b b = {};
    call(u1bMap, &b, 70);
    testeqv((long long)u1bDataLen(&b), 0LL, "%lld");
    want(!u1bIdle(&b));
    //  Feed a known pattern, check DataLen tracks bits and reads back.
    for (int i = 0; i < 70; i++) call(u1bFeed1, &b, (u1)(i & 1));
    testeqv((long long)u1bDataLen(&b), 70LL, "%lld");
    u1cs d = u1bDataC(&b);
    for (int i = 0; i < 70; i++)
        testeqv((long long)u1At(d, i), (long long)(i & 1), "%lld");
    //  Reset clears DATA; reused space reads back as zero.
    u1bReset(&b);
    testeqv((long long)u1bDataLen(&b), 0LL, "%lld");
    for (int i = 0; i < 70; i++) call(u1bFeed1, &b, 0);
    testeqv((long long)u1sCount(u1bDataC(&b)), 0LL, "%lld");
    call(u1bUnMap, &b);
    done;
}

ok64 BITtest() {
    sane(1);
    call(BITelem_test);
    call(BITclr_regression_test);
    call(BITtail_test);
    call(BITalgebra_test);
    call(BITbuf_test);
    done;
}

TEST(BITtest)
