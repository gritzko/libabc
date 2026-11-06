//
// Created by gritzko on 5/11/24.
//
#include "INT.h"

#include <assert.h>
#include <stdio.h>

#include "FILE.h"
#include "OK.h"
#include "PRO.h"
#include "S.h"
#include "TEST.h"

ok64 print(int c) {
    sane(1);
    printf("%c", c);
    done;
}

/** A simple true/false checker function */
ok64 check(int a, int b) {
    sane(1);
    if (a != b) fail(FAIL);
    // nedo(fprintf(stderr, "res: %lx\n", __));
    done;
}

ok64 Utest1() {
    sane(1);
    u8 array[] = {1, 2, 3, 4};
    a$(u8, slice, array);
    $eat(slice) { printf("%i\n", **slice); }
    Bu8 buf = {};
    u8bAllocate(buf, 32);
    u8 i = 0;
    $eat(u8bIdle(buf)) { **u8bIdle(buf) = i++; }
    assert(*buf[1] == 0);
    u8sReverse(u8bData(buf));
    assert(*buf[1] == 31);
    u8bSort(buf);
    assert(*buf[1] == 0);
    check(*buf[1], 0);
    check(*buf[1], 31);
    u8 **data = u8bData(buf);
    $eat(data) printf("%c", (int)(**data + 'A'));
    printf("\n");
    u8bFree(buf);
    done;
}

ok64 Utest2() {
    sane(1);
    a$str(dec, "-123");
    i64 i;
    call(i64decdrain, &i, dec);
    testeqv((long long)(i), (long long)(-123), "%lld");
    done;
}

ok64 OKdec() {
    sane(YES);
    aBpad(u8, into, 64);
    call(utf8sFeed10, u8bIdle(into), 12345UL);
    $print(u8bDataC(into));
    a$str(str, "12345");
    testeqv((long long)(YES), (long long)($eq(u8bData(into), str)), "%lld");
    done;
}

// MEM-002: i64decdrain must propagate the inner u64decdrain failure and
// must NOT read/produce a value from uninitialized stack memory. A lone
// sign (or sign followed by a non-digit) leaves the inner decode with no
// digits: u64decdrain returns BADARG without writing x. i64decdrain must
// return that error and leave the out-parameter untouched.
ok64 BADdec() {
    sane(1);
    // Each input has a leading sign so sane(!$empty) passes, then the inner
    // u64decdrain sees no digit and returns BADARG without writing x. Also
    // a bare non-digit "x" reaches u64decdrain directly with the same result.
    char const *bad[] = {"-", "+", "+a", "-z", "x"};
    for (int n = 0; n < (int)(sizeof(bad) / sizeof(bad[0])); ++n) {
        a$str(dec, bad[n]);
        i64 const sentinel = 0x7eadbeef7eadbeefLL;
        i64 i = sentinel;
        // not call(): we expect a non-OK code, assert on it rather than return
        otry(i64decdrain, &i, dec);
        // the inner u64decdrain failure must propagate verbatim
        testeqv((long long)(__), (long long)(INTBAD), "%llx");
        // out-parameter must be untouched on the error path (no garbage)
        testeqv((long long)(i), (long long)(sentinel), "%llx");
        __ = OK;
    }
    done;
}

// u64decdrain stops at the first non-digit without scaling by it:
// "7 limit" is 7 and "30\n" is 30, the cursor left on the stopper.
ok64 STOPdec() {
    sane(1);
    char const *in[] = {"7 limit", "30\n", "123"};
    u64 const want[] = {7, 30, 123};
    size_t const used[] = {1, 2, 3};
    for (int n = 0; n < 3; ++n) {
        a$str(dec, in[n]);
        u8c *from = dec[0];
        u64 x = 0;
        call(u64decdrain, &x, dec);
        testeqv((long long)(x), (long long)(want[n]), "%lld");
        testeqv((long long)(dec[0] - from), (long long)(used[n]), "%lld");
    }
    done;
}

//  u128 puts the SECOND u64 first, n128 the FIRST: same bits, two orders.
ok64 ORDER128() {
    sane(1);
    u128 a = {._64 = {1, 9}}, b = {._64 = {2, 1}};
    want(u128Z(&b, &a));    //  second u64: 1 < 9
    want(!u128Z(&a, &b));
    n128 c = {._64 = {1, 9}}, d = {._64 = {2, 1}};
    want(n128Z(&c, &d));    //  first u64: 1 < 2
    want(!n128Z(&d, &c));
    n128 e = {._64 = {1, 10}};
    want(n128Z(&c, &e));    //  equal first u64: the second breaks the tie
    want(!n128Z(&c, &c) && n128KeyEq(&c, &c) && !n128KeyEq(&c, &e));
    done;
}

ok64 Utest() {
    sane(1);
    call(ORDER128);
    call(Utest1);
    call(Utest2);
    call(OKdec);
    call(BADdec);
    call(STOPdec);
    done;
}

TEST(Utest);
