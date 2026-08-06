//  ABC-004: table-driven tests for abc/BIT.h (u1 bitmap over u64 words +
//  set algebra).  Includes a REGRESSION case proving "clear bit N"
//  leaves all other bits untouched — the exact old BitUnset
//  `|= ~(1<<bit)` bug.  A u1 map is just a u64 word slice/buffer; lengths
//  are whole words (u1sLen == words*64).

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
fun void refLoad(ref *r, u64cs s) {
    refClr(r, u1sLen(s));
    for (u32 i = 0; i < r->n; i++) r->bit[i] = (u8)u1At(s, i);
}
fun b8 refEq(ref const *r, u64cs s) {
    if (r->n != u1sLen(s)) return NO;
    for (u32 i = 0; i < r->n; i++)
        if (r->bit[i] != (u8)u1At(s, i)) return NO;
    return YES;
}

//  ---- element get/set/clr/put -----------------------------------------
//  256-bit map (4 words); a scattered pattern that straddles word edges.
ok64 BITelem_test() {
    sane(1);
    Bu1 b = {};
    call(u1bMap, b, 256);
    u64s s; u64sDup(s, u1bData(b));
    testeqv((long long)u1sLen(u1sConst(s)), 256LL, "%lld");

    u32 want[] = {0, 1, 63, 64, 65, 127, 128, 199, 255};
    for (size_t i = 0; i < sizeof(want) / sizeof(want[0]); i++)
        u1sSet(s, want[i]);
    for (u32 i = 0; i < 256; i++) {
        b8 expect = NO;
        for (size_t k = 0; k < sizeof(want) / sizeof(want[0]); k++)
            if (want[k] == i) expect = YES;
        testeqv((long long)u1At(u1sConst(s), i), (long long)expect, "%lld");
    }
    testeqv((long long)u1sCount(u1sConst(s)),
            (long long)(sizeof(want) / sizeof(want[0])), "%lld");

    u1sPut(s, 100, 1);
    testeqv((long long)u1At(u1sConst(s), 100), 1LL, "%lld");
    u1sPut(s, 100, 0);
    testeqv((long long)u1At(u1sConst(s), 100), 0LL, "%lld");
    call(u1bUnMap, b);
    done;
}

//  REGRESSION (ABC-004): clearing bit N must leave EVERY other bit as-is.
//  The old BUF.h BitUnset did `|= ~(1<<bit)`, which SET all the other
//  bits in the byte instead of clearing the target.
ok64 BITclr_regression_test() {
    sane(1);
    enum { N = 200 };
    Bu1 b = {};
    call(u1bMap, b, N);
    u64s s; u64sDup(s, u1bData(b));
    for (u32 i = 0; i < N; i++) u1sSet(s, i);
    testeqv((long long)u1sCount(u1sConst(s)), (long long)N, "%lld");
    for (u32 clr = 0; clr < N; clr++) {
        u1sClr(s, clr);
        testeqv((long long)u1At(u1sConst(s), clr), 0LL, "%lld");
        for (u32 j = clr + 1; j < N; j++)
            testeqv((long long)u1At(u1sConst(s), j), 1LL, "%lld");
        u1sSet(s, clr);  // restore for the next round
    }
    call(u1bUnMap, b);
    done;
}

//  ---- unused tail bits stay zero: set only low bits, count is exact ----
ok64 BITtail_test() {
    sane(1);
    Bu1 b = {};
    call(u1bMap, b, 10);  // rounds up to one 64-bit word
    u64s s; u64sDup(s, u1bData(b));
    testeqv((long long)u1sLen(u1sConst(s)), 64LL, "%lld");
    for (u32 i = 0; i < 10; i++) u1sSet(s, i);
    testeqv((long long)u1sCount(u1sConst(s)), 10LL, "%lld");
    testeqv((long long)u1sAny(u1sConst(s)), 1LL, "%lld");
    testeqv((long long)u1sNext(u1sConst(s), 0), 0LL, "%lld");
    testeqv((long long)u1sNext(u1sConst(s), 9), 9LL, "%lld");
    testeqv((long long)u1sNext(u1sConst(s), 10), 64LL, "%lld");  // none past 9
    call(u1bUnMap, b);
    done;
}

//  ---- algebra vs reference model --------------------------------------
ok64 BITalgebra_test() {
    sane(1);
    enum { N = 192 };  // 3 whole words
    Bu1 ba = {}, bb = {};
    call(u1bMap, ba, N);
    call(u1bMap, bb, N);
    u64s a; u64sDup(a, u1bData(ba));
    u64s b; u64sDup(b, u1bData(bb));
    for (u32 i = 0; i < N; i++) {
        if (i % 3 == 0) u1sSet(a, i);
        if (i % 5 == 0) u1sSet(b, i);
    }

    ref ra = {}, rb = {};
    refLoad(&ra, u1sConst(a));
    refLoad(&rb, u1sConst(b));

#define ALG(opfn, expr)                                                   \
    do {                                                                  \
        Bu1 bd = {};                                                      \
        call(u1bMap, bd, N);                                              \
        u64s d; u64sDup(d, u1bData(bd));                                  \
        call(u1sOr, d, u1sConst(a));                                      \
        call(opfn, d, u1sConst(b));                                       \
        ref rd;                                                           \
        refClr(&rd, N);                                                   \
        for (u32 i = 0; i < N; i++) rd.bit[i] = (u8)(expr);              \
        want(refEq(&rd, u1sConst(d)));                                    \
        call(u1bUnMap, bd);                                               \
    } while (0)
    ALG(u1sOr,     ra.bit[i] | rb.bit[i]);
    ALG(u1sAnd,    ra.bit[i] & rb.bit[i]);
    ALG(u1sAndNot, ra.bit[i] & ~rb.bit[i] & 1);
    ALG(u1sXor,    ra.bit[i] ^ rb.bit[i]);
#undef ALG

    //  Count + Next iteration over `a` matches the reference.
    u32 rc = 0;
    for (u32 i = 0; i < N; i++) rc += ra.bit[i];
    testeqv((long long)u1sCount(u1sConst(a)), (long long)rc, "%lld");
    u32 seen = 0;
    u1$for (i, u1sConst(a)) { want(ra.bit[i]); seen++; }
    testeqv((long long)seen, (long long)rc, "%lld");

    //  Word-length mismatch guard: 3-word dst vs 2-word src -> BITLEN.
    Bu1 bs = {};
    call(u1bMap, bs, 128);  // 2 words
    u64s shorter; u64sDup(shorter, u1bData(bs));
    testeqv((long long)u1sOr(a, u1sConst(shorter)), (long long)BITLEN, "%lld");
    want(!u1sEq(u1sConst(a), u1sConst(shorter)));
    call(u1bUnMap, bs);

    call(u1bUnMap, ba);
    call(u1bUnMap, bb);
    done;
}

//  ---- buffer family: Map presents a zeroed map; Reset re-zeros --------
ok64 BITbuf_test() {
    sane(1);
    Bu1 b = {};
    call(u1bMap, b, 128);  // 2 words, all zero
    u64s s; u64sDup(s, u1bData(b));
    testeqv((long long)u1sCount(u1sConst(s)), 0LL, "%lld");
    for (u32 i = 0; i < 128; i++) if (i & 1) u1sSet(s, i);
    u64cs d; u64csDup(d, u1bDataC(b));
    for (u32 i = 0; i < 128; i++)
        testeqv((long long)u1At(d, i), (long long)(i & 1), "%lld");
    //  Reset clears every bit, keeps the map size.
    u1bReset(b);
    testeqv((long long)u1sLen(u1sConst(s)), 128LL, "%lld");
    testeqv((long long)u1sCount(u1sConst(s)), 0LL, "%lld");
    call(u1bUnMap, b);
    done;
}

//  ---- arena Acquire yields a zeroed, addressable map ------------------
ok64 BITacquire_test() {
    sane(1);
    Bu1 b = {};
    call(u1bAcquire, ABC_BASS, b, 130);  // -> 3 words
    u64s s; u64sDup(s, u1bData(b));
    testeqv((long long)u1sLen(u1sConst(s)), 192LL, "%lld");
    testeqv((long long)u1sCount(u1sConst(s)), 0LL, "%lld");
    u1sSet(s, 129);
    testeqv((long long)u1At(u1sConst(s), 129), 1LL, "%lld");
    testeqv((long long)u1sCount(u1sConst(s)), 1LL, "%lld");
    done;  // BASS-acquired: released by the call() frame
}

ok64 BITtest() {
    sane(1);
    call(BITelem_test);
    call(BITclr_regression_test);
    call(BITtail_test);
    call(BITalgebra_test);
    call(BITbuf_test);
    call(BITacquire_test);
    done;
}

TEST(BITtest)
