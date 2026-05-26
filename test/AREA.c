#include "BUF.h"
#include "INT.h"
#include "PRO.h"
#include "TEST.h"

// Test 1: bAcquire — fixed-cap child buffers acquired from a u8 arena
ok64 bAcquireTest() {
    sane(1);

    Bu8 arena = {};
    call(u8bMap, arena, MB);

    Bu8 bytes = {};
    call(u8bAcquire, arena, bytes, 1000);

    Bu32 words = {};
    call(u32bAcquire, arena, words, 500);

    Bu64 quads = {};
    call(u64bAcquire, arena, quads, 200);

    testeqv((long long)(u8bDataLen(bytes)), (long long)(0), "%lld");
    testeqv((long long)(u8bLen(bytes)), (long long)(1000), "%lld");
    testeqv((long long)(u32bDataLen(words)), (long long)(0), "%lld");
    testeqv((long long)(u32bLen(words)), (long long)(500), "%lld");
    testeqv((long long)(u64bDataLen(quads)), (long long)(0), "%lld");
    testeqv((long long)(u64bLen(quads)), (long long)(200), "%lld");

    test((u8p)words[0] >= (u8p)bytes[3], FAIL);
    test((u8p)quads[0] >= (u8p)words[3], FAIL);

    test(((size_t)words[0] & 3) == 0, FAIL);
    test(((size_t)quads[0] & 7) == 0, FAIL);

    test(u8bContains(arena, bytes[0]), FAIL);
    test(u8bContains(arena, words[0]), FAIL);
    test(u8bContains(arena, quads[0]), FAIL);
    test((u8cp)bytes[3] <= (u8cp)arena[3], FAIL);
    test((u8cp)words[3] <= (u8cp)arena[3], FAIL);
    test((u8cp)quads[3] <= (u8cp)arena[3], FAIL);

    for (u8 i = 0; i < 100; ++i) call(u8bFeed1, bytes, i);
    testeqv((long long)(u8bDataLen(bytes)), (long long)(100), "%lld");
    testeqv((long long)(Bat(bytes, 50)), (long long)(50), "%lld");

    for (u32 i = 0; i < 100; ++i) call(u32bFeed1, words, i * 7);
    testeqv((long long)(u32bDataLen(words)), (long long)(100), "%lld");
    testeqv((long long)(Bat(words, 10)), (long long)(70), "%lld");

    for (u64 i = 0; i < 100; ++i) call(u64bFeed1, quads, i * 13);
    testeqv((long long)(u64bDataLen(quads)), (long long)(100), "%lld");
    testeqv((long long)(Bat(quads, 5)), (long long)(65), "%lld");

    u8 stack_var = 0;
    test(!u8bContains(arena, &stack_var), FAIL);

    call(u8bUnMap, arena);
    done;
}

// Test 2: bAlign/bAcq — u8 source, no alignment
ok64 bAlignU8Test() {
    sane(1);

    Bu8 arena = {};
    call(u8bAllocate, arena, 4096);

    b_lign(u8, g, arena);
    testeqv((long long)(u8gRestLen(g)), (long long)(4096), "%lld");
    testeqv((long long)(u8gLeftLen(g)), (long long)(0), "%lld");

    for (u8 i = 0; i < 100; ++i) call(u8gFeed1, g, i);
    testeqv((long long)(u8gLeftLen(g)), (long long)(100), "%lld");

    b_cq(u8, s1, arena);
    testeqv((long long)(u8csLen(s1)), (long long)(100), "%lld");
    testeqv((long long)(u8bPastLen(arena)), (long long)(100), "%lld");

    b_lign(u8, g2, arena);
    testeqv((long long)(u8gRestLen(g2)), (long long)(4096 - 100), "%lld");
    for (u8 i = 0; i < 50; ++i) call(u8gFeed1, g2, i);
    b_cq(u8, s1b, arena);
    testeqv((long long)(u8csLen(s1b)), (long long)(50), "%lld");
    testeqv((long long)(u8bPastLen(arena)), (long long)(150), "%lld");

    // Earlier slice still valid (same backing memory).
    testeqv((long long)(u8csLen(s1)), (long long)(100), "%lld");
    testeqv((long long)(*s1[0]), (long long)(0), "%lld");
    testeqv((long long)(*(s1[1] - 1)), (long long)(99), "%lld");

    call(u8bFree, arena);
    done;
}

// Test 3: bAlign/bAcq — typed gauge, alignment padding folds into PAST
ok64 bAlignU32Test() {
    sane(1);

    Bu8 arena = {};
    call(u8bAllocate, arena, 4096);

    // Misalign with a 1-byte u8 rental
    b_lign(u8, g0, arena);
    call(u8gFeed1, g0, 0xFF);
    b_cq(u8, pad, arena);
    testeqv((long long)(u8csLen(pad)), (long long)(1), "%lld");
    testeqv((long long)(u8bPastLen(arena)), (long long)(1), "%lld");

    // u32 align: bAlign rounds up to 4
    b_lign(u32, g, arena);
    test(((uintptr_t)g[1] & 3) == 0, FAIL);
    test(u8bPastLen(arena) >= 4, FAIL);

    for (u32 i = 0; i < 50; ++i) call(u32gFeed1, g, i * 7);
    testeqv((long long)(u32gLeftLen(g)), (long long)(50), "%lld");

    b_cq(u32, left, arena);
    testeqv((long long)(u32csLen(left)), (long long)(50), "%lld");
    testeqv((long long)(*left[0]), (long long)(0), "%lld");
    testeqv((long long)(*(left[0] + 10)), (long long)(70), "%lld");

    // 4 (aligned) + 50*4 = 204 bytes in PAST
    testeqv((long long)(u8bPastLen(arena)), (long long)(4 + 50 * 4), "%lld");

    testeqv((long long)(*left[0]), (long long)(0), "%lld");
    testeqv((long long)(*(left[0] + 49)), (long long)(49 * 7), "%lld");

    call(u8bFree, arena);
    done;
}

// Test 4: Mixed u8 + u32 — typical interleaved workload
ok64 MixedArenaTest() {
    sane(1);

    Bu8 arena = {};
    call(u8bAllocate, arena, 8192);

    u8cs hello = $u8str("hello");
    b_lign(u8, g8, arena);
    call(u8gFeed, g8, hello);
    b_cq(u8, s_hello, arena);
    testeqv((long long)(u8bPastLen(arena)), (long long)(5), "%lld");

    b_lign(u32, g32, arena);
    test(((uintptr_t)g32[1] & 3) == 0, FAIL);
    for (u32 i = 0; i < 10; ++i) call(u32gFeed1, g32, i + 100);
    b_cq(u32, toks, arena);
    testeqv((long long)(u32csLen(toks)), (long long)(10), "%lld");

    u8cs world = $u8str("world");
    b_lign(u8, g8b, arena);
    call(u8gFeed, g8b, world);
    b_cq(u8, s_world, arena);

    test(u8bPastLen(arena) > 0, FAIL);
    test(u8bPastLen(arena) <= 8192, FAIL);

    $testeq(s_hello, hello);
    testeqv((long long)(u32csLen(toks)), (long long)(10), "%lld");
    testeqv((long long)(*toks[0]), (long long)(100), "%lld");
    testeqv((long long)(*(toks[0] + 9)), (long long)(109), "%lld");
    $testeq(s_world, world);

    call(u8bFree, arena);
    done;
}

// Test 5: Many rentals then u8bReset reuses the whole arena
ok64 ArenaCycleTest() {
    sane(1);

    Bu8 arena = {};
    call(u8bAllocate, arena, 4096);

    for (int cycle = 0; cycle < 10; ++cycle) {
        b_lign(u32, g, arena);
        for (u32 i = 0; i < 20; ++i) call(u32gFeed1, g, i);
        testeqv((long long)(u32gLeftLen(g)), (long long)(20), "%lld");
        b_cq(u32, cyc, arena);
        (void)cyc;
    }

    test(u8bPastLen(arena) >= 800, FAIL);
    test(u8bPastLen(arena) <= 800 + 10 * 4, FAIL);

    u8bReset(arena);
    testeqv((long long)(u8bPastLen(arena)), (long long)(0), "%lld");
    testeqv((long long)(u8bIdleLen(arena)), (long long)(4096), "%lld");

    b_lign(u8, g8, arena);
    for (u8 i = 0; i < 200; ++i) call(u8gFeed1, g8, i);
    b_cq(u8, refill, arena);
    testeqv((long long)(u8csLen(refill)), (long long)(200), "%lld");
    testeqv((long long)(u8bPastLen(arena)), (long long)(200), "%lld");

    call(u8bFree, arena);
    done;
}

// Test 6: All earlier rentals stay valid as later ones land
ok64 ArenaSliceValidTest() {
    sane(1);

    Bu8 arena = {};
    call(u8bAllocate, arena, 4096);

    u8cs pat1 = $u8str("AAAA");
    b_lign(u8, g1, arena);
    call(u8gFeed, g1, pat1);
    b_cq(u8, s1, arena);

    b_lign(u32, g2, arena);
    call(u32gFeed1, g2, 0xDEADBEEF);
    b_cq(u32, s2, arena);

    u8cs pat3 = $u8str("ZZZZ");
    b_lign(u8, g3, arena);
    call(u8gFeed, g3, pat3);
    b_cq(u8, s3, arena);

    $testeq(s1, pat1);
    testeqv((long long)(u32csLen(s2)), (long long)(1), "%lld");
    testeqv((long long)(*s2[0]), (long long)((u32)0xDEADBEEF), "%lld");
    $testeq(s3, pat3);

    test((u8cp)s1[1] <= (u8cp)s2[0], FAIL);
    test((u8cp)s2[1] <= (u8cp)s3[0], FAIL);

    call(u8bFree, arena);
    done;
}

// Test 7: b_ren / b_rent — one-shot rental over a known source
ok64 ArenaRenTest() {
    sane(1);

    Bu8 arena = {};
    call(u8bAllocate, arena, 4096);

    u8cs hello = $u8str("hello");
    b_ren(stored_hello, arena, hello);
    $testeq(stored_hello, hello);
    test(u8bContains(arena, stored_hello[0]), FAIL);
    testeqv((long long)u8bPastLen(arena), (long long)5, "%lld");

    u32 raw[4] = {10, 20, 30, 40};
    u32cs src = {raw, raw + 4};
    b_rent(u32, stored_toks, arena, src);
    testeqv((long long)u32csLen(stored_toks), (long long)4, "%lld");
    test(((uintptr_t)stored_toks[0] & 3) == 0, FAIL);
    testeqv((long long)*stored_toks[0], (long long)10, "%lld");
    testeqv((long long)*(stored_toks[0] + 3), (long long)40, "%lld");
    // 5 bytes hello + 3 align pad + 16 bytes u32s = 24 in PAST
    testeqv((long long)u8bPastLen(arena), (long long)24, "%lld");

    $testeq(stored_hello, hello);

    // BNOROOM path
    Bu8 small = {};
    call(u8bAllocate, small, 4);
    u8cs too_big = $u8str("overflow");
    u8cs out = {};
    ok64 o = u8bAren(small, out, too_big);
    test(o == BNOROOM, FAIL);
    call(u8bFree, small);

    call(u8bFree, arena);
    done;
}

ok64 AREAtest() {
    sane(1);
    call(bAcquireTest);
    call(bAlignU8Test);
    call(bAlignU32Test);
    call(MixedArenaTest);
    call(ArenaCycleTest);
    call(ArenaSliceValidTest);
    call(ArenaRenTest);
    done;
}

TEST(AREAtest)
