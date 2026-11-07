#include "PRO.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "INT.h"
#include "TEST.h"

ok64 fail_test() {
    sane(1);
    fail(BADARG);
    done;
}

con ok64 ABBADARG = 0x28b2ca34a6d0;
con ok64 ABCBADARG = 0xa2cc2ca34a6d0;
con ok64 ABCDBADARG = 0x28b30d2ca34a6d0;

ok64 PROis() {
    sane(1);
    test(ok64is(ABCDBADARG, BADARG), BADARG);
    test(ok64is(ABCBADARG, BADARG), BADARG);
    test(ok64is(ABBADARG, BADARG), BADARG);
    test(ok64is(BADARG, BADARG), BADARG);
    done;
}

// Regression: argv parsing used to write into a fixed 64-slot global
// (_STD_ARGS).  A `be put` glob expanding past 64 tokens tripped a
// global-buffer-overflow.  _parse_args now acquires STD_ARGS storage
// from ABC_BASS sized to argn — feed it 256 tokens and check every
// slot lands correctly.
#define BIG_ARGN 256
ok64 parse_args_big() {
    sane(1);
    static char buf[BIG_ARGN][16];
    char *argv[BIG_ARGN];
    for (int i = 0; i < BIG_ARGN; ++i) {
        snprintf(buf[i], sizeof(buf[i]), "arg%d", i);
        argv[i] = buf[i];
    }
    extern ok64 _parse_args(int, char **);
    test(_parse_args(BIG_ARGN, argv), 0);
    if ($arglen != BIG_ARGN) fail(TESTFAIL);
    for (int i = 0; i < BIG_ARGN; ++i) {
        a$rg(a, i);
        size_t want = strlen(buf[i]);
        if ($len(a) != (i64)want) fail(TESTFAIL);
        if (memcmp(a[0], buf[i], want) != 0) fail(TESTFAIL);
    }
    done;
}

// ABC-26B07MlxmQ: a callee carving on BASS keeps its carve under
// ocall, loses it under call, and both propagate failures.
ok64 carve_into(u8 **out, b8 bad) {
    sane(out);
    a_carve(u8, c, 64);
    u8bFeed1(c, 'x');
    *out = c[1];
    test(!bad, BADARG);
    done;
}

ok64 ocall_fail() {
    sane(1);
    u8 *p = NULL;
    ocall(carve_into, &p, YES);
    done;
}

ok64 ocall_test() {
    sane(1);
    u8 *top = ABC_BASS[2], *p = NULL;
    call(carve_into, &p, NO);
    test(ABC_BASS[2] == top, TESTFAIL);
    ocall(carve_into, &p, NO);
    test(ABC_BASS[2] > top && *p == 'x', TESTFAIL);
    test(ocall_fail() == BADARG, TESTFAIL);
    ((u8 **)ABC_BASS)[1] = ((u8 **)ABC_BASS)[2] = top;
    done;
}

ok64 pro_test() {
    sane(1);
    mute(fail_test(), BADARG);
    call(PROis);
    call(parse_args_big);
    call(ocall_test);
    done;
}

TEST(pro_test);
