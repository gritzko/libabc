#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "01.h"
#include "BUF.h"
#include "INT.h"
#include "PRO.h"
#include "TEST.h"

// ABC-006: instantiate Bx.h with mremap masked out, so the portable
// bReMap fallback path is compiled and tested on every platform.
typedef u8 x8;
fun b8 x8Z(u8c *a, u8c *b) { return *a < *b; }
#undef MREMAP_MAYMOVE
#define X(M, name) M##x8##name
#include "Bx.h"
#undef X

ok64 fail_test() {
    sane(1);
    fail(BADARG);
    done;
}

ok64 Bmap_test() {
    sane(1);
    Bu8 buf = {};
    call(u8bMap, buf, 1024);
    Bat(buf, 100) = 100;
    testeqv((long long)(Bat(buf, 100)), (long long)(100), "%lld");
    call(u8bUnMap, buf);
    done;
}

ok64 B$_test() {
    sane(1);
    aBpad(u8cp, slices, 8);
    u8cs hello = $u8str("Hello");
    u8cpbFeed2(slices, hello[0], hello[1]);
    done;
}

ok64 Bndx_test() {
    sane(YES);
    Bu64 buf = {};
    u64bAllocate(buf, 1024);
    for (u64 i = 0; i < 1000; ++i) {
        call(u64bFeed1, buf, i);
        sane(Blast(buf) == i);
    }
    u64bFree(buf);
    done;
}

ok64 Breserve_test() {
    sane(1);
    Bu8 buf = {};
    call(u8bAllocate, buf, 1024);
    for (int i = 0; i < (1 << 18); i++) {
        otry(u8bFeed2, buf, '1', '2');
        ofix(BNOROOM) call(u8bReserve, buf, 1024);
        ocry();
    }
    call(u8bFree, buf);
    done;
}

ok64 B$test() {
    sane(1);
    a$$pad(pad, 128, 8);
    $$call(u8sFeedCStr, pad, "one");
    $$call(utf8sFeed10, pad, 2);
    $$call(u8sFeedCStr, pad, "three");
    a$str(templ, "First $1, then $2, then $3!");
    a$str(correct, "First one, then 2, then three!");
    aBpad2(u8, res, 128);
    $$feedf(residle, templ, pad$data);
    $testeq(correct, resdata);
    done;
}

ok64 BBtest() {
    sane(1);
    aBpad(u8b, buff, 4);
    testeqv((long long)(sizeof(Bat(buff, 0))), (long long)(sizeof(Bvoid)), "%lld");
    done;
}

ok64 u8sPrintf_test() {
    sane(1);

    // basic formatting
    a_pad(u8, buf, 128);
    call(u8sPrintf, buf_idle, "hello %d", 42);
    a$str(expect, "hello 42");
    $testeq(expect, buf_datac);

    // append more
    call(u8sPrintf, buf_idle, " %s!", "world");
    a$str(expect2, "hello 42 world!");
    $testeq(expect2, buf_datac);

    // empty format
    u8p before = buf[2];
    call(u8sPrintf, buf_idle, "%s", "");
    testeqv((long long)(buf[2]), (long long)(before), "%lld");

    // SNOROOM on overflow
    a_pad(u8, tiny, 4);
    ok64 o = u8sPrintf(tiny_idle, "toolong");
    testeqv((long long)(o), (long long)(SNOROOM), "%lld");

    // "abc" = 3 chars fits in 4 bytes (3 + null)
    a_pad(u8, fit, 4);
    o = u8sPrintf(fit_idle, "abc");
    testeqv((long long)(o), (long long)(OK), "%lld");
    testeqv((long long)(u8bDataLen(fit)), (long long)(3u), "%lld");

    // "abcd" = 4 chars does NOT fit in 4 bytes (needs 5)
    a_pad(u8, fit2, 4);
    o = u8sPrintf(fit2_idle, "abcd");
    testeqv((long long)(o), (long long)(SNOROOM), "%lld");

    // u8gPrintf: write into a gauge
    a_pad(u8, gbuf, 64);
    b_lign(u8, gg, gbuf);
    call(u8gPrintf, gg, "x=%d", 99);
    b_cq(u8, gleft, gbuf);
    a$str(gexp, "x=99");
    $testeq(gexp, gleft);

    // u8bPrintf: write into a buffer
    a_pad(u8, bbuf, 64);
    call(u8bPrintf, bbuf, "%s=%d", "val", 7);
    a$str(bexp, "val=7");
    $testeq(bexp, bbuf_datac);

    // u8bPrintf: append
    call(u8bPrintf, bbuf, "!");
    a$str(bexp2, "val=7!");
    $testeq(bexp2, bbuf_datac);

    done;
}

ok64 u8sPop_test() {
    sane(1);
    // ABC-005 repro: Pop must return the TAIL bytes and shed them;
    // it used to copy from the head while shedding the tail
    u8 src[6] = {'a', 'b', 'c', 'd', 'e', 'f'};
    u8cs s = {src, src + 6};
    u8 out[3];
    u8s into = {out, out + 3};
    call(u8sPop, s, into);
    a$str(exp, "def");
    u8cs got = {out, out + 3};
    want($eq(got, exp));
    a$str(rem, "abc");
    want($eq(s, rem));
    // round-trip: pop the rest in two goes, mimic u8sPop1/u8sPop32 order
    u8 out2[2];
    u8s into2 = {out2, out2 + 2};
    call(u8sPop, s, into2);
    a$str(exp2, "bc");
    u8cs got2 = {out2, out2 + 2};
    want($eq(got2, exp2));
    u8 last = 0;
    call(u8sPop1, s, &last);
    want(last == 'a');
    want($empty(s));
    // underflow must stay SNODATA
    u8 out3[4];
    u8s into3 = {out3, out3 + 4};
    want(SNODATA == u8sPop(s, into3));
    done;
}

ok64 $$feedf_trunc_test() {
    sane(1);
    // ABC-005 repro: a failed feed of the LAST template substitution
    // used to leave the cursor advanced, so truncation returned OK
    u8cs argv[1] = {u8slit("bcd")};
    u8css args = {argv, argv + 1};
    a$str(t1, "a$1");
    u8 buf[2];
    u8s into = {buf, buf + 2};
    want(BNOROOM == $$feedf(into, t1, args));

    // ABC-005 repro: same for the last literal template byte
    a$str(t2, "ab");
    u8css noargs = {argv, argv};
    u8 buf2[1];
    u8s into2 = {buf2, buf2 + 1};
    want(BNOROOM == $$feedf(into2, t2, noargs));

    // exact fit must still succeed
    a$str(t3, "a$1");
    u8 buf3[4];
    u8s into3 = {buf3, buf3 + 4};
    want(OK == $$feedf(into3, t3, args));
    a$str(exp, "abcd");
    u8cs got = {buf3, into3[0]};
    want($eq(got, exp));
    done;
}

ok64 gFed_test() {
    sane(1);
    // ABC-005 repro: huge len used to wrap the rest pointer (UB) and
    // report OK; must be a plain NOROOM with the gauge untouched
    u8 pad[8];
    u8g g = {pad, pad, pad + 8};
    want(NOROOM == u8gFed(g, (size_t)-1));
    want(g[1] == pad);
    call(u8gFed, g, 8);
    want(g[1] == pad + 8);
    want(NOROOM == u8gFed1(g));
    done;
}

// ABC-006 repro: bSplice offsets/lengths are ELEMENT counts; a 4-byte
// T mid-buffer splice must move whole elements, not bytes.
ok64 Bsplice_test() {
    sane(1);
    // u8 baseline: replace "cd" with "XYZ" (behavior-preserving check)
    a_pad(u8, sb, 16);
    u8cs six = $u8str("abcdef");
    call(u8bFeed, sb, six);
    u8cs xyz = $u8str("XYZ");
    call(u8bSplice, sb, 2, 2, xyz);
    a$str(exp, "abXYZef");
    $testeq(exp, sb_datac);
    // u32: same splice, element-wise
    aBpad(u32, wb, 16);
    for (u32 i = 0; i < 6; ++i) call(u32bFeed1, wb, 100 + i);
    u32c pastev[] = {7, 8, 9};
    a_u32cs(paste, pastev);
    call(u32bSplice, wb, 2, 2, paste);
    u32c expv[] = {100, 101, 7, 8, 9, 104, 105};
    testeqv((long long)(u32bDataLen(wb)), (long long)(7), "%lld");
    for (size_t i = 0; i < 7; ++i)
        testeqv((long long)(u32bAt(wb, i)), (long long)(expv[i]), "%lld");
    done;
}

// ABC-006 repro: non-mremap bReMap fallback must clamp the copy to
// min(old,new) — a shrink used to memmove old_size into a new_size map.
ok64 BReMapShrink_test() {
    sane(1);
    Bx8 buf = {};
    call(x8bMap, buf, 64 * 4096);
    for (u8 i = 0; i < 16; ++i) call(x8bFeed1, buf, i);
    call(x8bReMap, buf, 4096);
    testeqv((long long)(x8bLen(buf)), (long long)(4096), "%lld");
    testeqv((long long)(x8bDataLen(buf)), (long long)(16), "%lld");
    for (size_t i = 0; i < 16; ++i)
        testeqv((long long)(x8bAt(buf, i)), (long long)(i), "%lld");
    // grow through the same fallback path keeps data too
    call(x8bReMap, buf, 8 * 4096);
    testeqv((long long)(x8bLen(buf)), (long long)(8 * 4096), "%lld");
    for (size_t i = 0; i < 16; ++i)
        testeqv((long long)(x8bAt(buf, i)), (long long)(i), "%lld");
    call(x8bUnMap, buf);
    done;
}

// ABC-006 repro: cap*sizeof(T) used to wrap, so a huge cap/len passed
// the room checks and claimed cap elements over a few bytes.
ok64 Boverflow_test() {
    sane(1);
    size_t huge = SIZE_MAX / sizeof(u64) + 2;  // *8 wraps to 8 bytes
    Bu8 arena = {};
    call(u8bMap, arena, 4096);
    Bu64 child = {};
    ok64 o = u64bAcquire(arena, child, huge);
    testeqv((long long)(o), (long long)(BNOROOM), "%llx");
    call(u8bUnMap, arena);
    Bu64 hbuf = {};
    o = u64bAllocate(hbuf, huge);
    testeqv((long long)(o), (long long)(BALLOCFAIL), "%llx");
    Bu64 mbuf = {};
    o = u64bMap(mbuf, huge);
    testeqv((long long)(o), (long long)(MMAPFAIL), "%llx");
    done;
}

ok64 Btest() {
    sane(1);
    call(Bmap_test);
    call(Bsplice_test);
    call(BReMapShrink_test);
    call(Boverflow_test);
    call(B$_test);
    call(Bndx_test);
    call(Breserve_test);
    call(B$test);
    call(BBtest);
    call(u8sPrintf_test);
    call(u8sPop_test);
    call($$feedf_trunc_test);
    call(gFed_test);
    done;
}

TEST(Btest)
