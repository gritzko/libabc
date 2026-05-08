#include "SORT.h"

#include <unistd.h>

#include "PRO.h"
#include "B.h"
#include "TEST.h"

#define LEN1 917

ok64 SORT1() {
    sane(1);
    aBpad2(u64, ints, LEN1);
    aBpad2(u64, ints2, LEN1);
    aBpad2(u64, ints3, LEN1);
    u64 r = 57;
    for (u64 i = 0; i < LEN1; ++i) {
        u64sFeed1(intsidle, i ^ r);
        u64sFeed1(ints3idle, i ^ r);
    }
    $sort(ints3data, u64cmp);
    call(SORTu64, ints2idle, intsdata);
    testeqv((long long)(LEN1), (long long)($len(ints2data)), "%lld");
    for (u64 i = 0; i < LEN1; ++i) {
        testeqv((long long)(u64sAt(ints3data, i)), (long long)(u64sAt(ints2data, i)), "%lld");
    }
    done;
}

ok64 SORTtest() {
    sane(1);
    call(SORT1);
    done;
}

TEST(SORTtest);
