#include "BIN.h"

#include "PRO.h"
#include "TEST.h"

ok64 BINtest1() {
    sane(1);
    testeqv((long long)(YES), (long long)(bin64contains(bin64of(2, 0), bin64of(0, 2))), "%lld");
    testeqv((long long)(bin64daughter(11)), (long long)(9), "%lld");
    testeqv((long long)(bin64son(5)), (long long)(6), "%lld");
    testeqv((long long)(bin64size(7)), (long long)(8), "%lld");
    testeqv((long long)(bin64level(11)), (long long)(2), "%lld");
    done;
}

ok64 BINtest() {
    sane(1);
    call(BINtest1);
    done;
}

TEST(BINtest);
