//
// Created by gritzko on 5/11/24.
//
#include "01.h"

#include <assert.h>
#include <stdio.h>

#include "OK.h"
#include "PRO.h"
#include "TEST.h"

ok64 BITStest1() {
    sane(1);
    u64 a = 0xaabbccdd11223344;
    u64 b = flip64(a);
    testeqv((long long)(b), (long long)(0x44332211ddccbbaa), "%lld");
    u32 c = 0xaabbccdd;
    testeqv((long long)(flip32(c)), (long long)(0xddccbbaa), "%lld");

    u64 t7 = 128;
    testeqv((long long)(ctz64(t7)), (long long)(7), "%lld");
    testeqv((long long)(clz64(t7)), (long long)(56), "%lld");
    done;
}

ok64 BITSbytelen() {
    sane(YES);
    testeqv((long long)(2), (long long)(u32bytelen(0x111)), "%lld");
    testeqv((long long)(2), (long long)(u32bytelen(0x1111)), "%lld");
    testeqv((long long)(2), (long long)(u64bytelen(0x111)), "%lld");
    testeqv((long long)(2), (long long)(u64bytelen(0x1111)), "%lld");
    testeqv((long long)(5), (long long)(u64bytelen(0x1122334455)), "%lld");
    testeqv((long long)(6), (long long)(u64bytelen(0x12233445566)), "%lld");
    testeqv((long long)(0), (long long)(u64bytelen(0x0)), "%lld");

    testeqv((long long)(4096), (long long)(sizeof(bl0C)), "%lld");
    testeqv((long long)(65536), (long long)(sizeof(bl10)), "%lld");
    done;
}

ok64 BITSstr() {
    sane(YES);
    const char* sbadarg = okstr(SBADARG);
    testeqv((long long)(0), (long long)(strcmp(sbadarg, "SBADARG")), "%lld");
    const char* ok = okstr(OK);
    testeqv((long long)(0), (long long)(strcmp(ok, "OK")), "%lld");
    done;
}

ok64 BITStest() {
    sane(1);
    call(BITStest1);
    call(BITSbytelen);
    call(BITSstr);
    done;
}

TEST(BITStest);
