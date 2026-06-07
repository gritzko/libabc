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
    $eat(u8bIdle(buf)) { Bi(buf) = i++; }
    assert(Bd(buf) == 0);
    $reverse(u8bData(buf));
    assert(Bd(buf) == 31);
    u8bSort(buf);
    assert(Bd(buf) == 0);
    check(Bd(buf), 0);
    check(Bd(buf), 31);
    u8 **data = u8bData(buf);
    $eat(data) printf("%c", (int)(**data + 'A'));
    printf("\n");
    $u8 abc = $cut(u8bPast(buf), 0, 3);
    assert($len(abc) == 3);
    u8 three = 3;
    u8c *c = $u8find(u8bPastC(buf), &three);
    assert(c - buf[0] == 3);
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
// digits: u64decdrain returns SBADARG without writing x. i64decdrain must
// return that error and leave the out-parameter untouched.
ok64 BADdec() {
    sane(1);
    // Each input has a leading sign so sane(!$empty) passes, then the inner
    // u64decdrain sees no digit and returns SBADARG without writing x. Also
    // a bare non-digit "x" reaches u64decdrain directly with the same result.
    char const *bad[] = {"-", "+", "+a", "-z", "x"};
    for (int n = 0; n < (int)(sizeof(bad) / sizeof(bad[0])); ++n) {
        a$str(dec, bad[n]);
        i64 const sentinel = 0x7eadbeef7eadbeefLL;
        i64 i = sentinel;
        // not call(): we expect a non-OK code, assert on it rather than return
        otry(i64decdrain, &i, dec);
        // the inner u64decdrain failure must propagate verbatim
        testeqv((long long)(__), (long long)(SBADARG), "%llx");
        // out-parameter must be untouched on the error path (no garbage)
        testeqv((long long)(i), (long long)(sentinel), "%llx");
        __ = OK;
    }
    done;
}

ok64 Utest() {
    sane(1);
    call(Utest1);
    call(Utest2);
    call(OKdec);
    call(BADdec);
    done;
}

TEST(Utest);
