#include "INT.h"
#include "PRO.h"
#include "TEST.h"

// Instantiate the HEAP template for u32
#define X(M, name) M##u32##name
#include "HEAPx.h"
#undef X

ok64 HEAPtest1() {
    sane(1);
    // Make a buffer on the stack
    aBpad(u32, pad, 32);
    // Pushes one entry into the heap buffer
    call(HEAPu32Push1, pad, 3);
    call(HEAPu32Push1, pad, 2);
    call(HEAPu32Push1, pad, 1);
    u32 one, two, three;
    // Retrieves the least entry.
    // May also use **pad to read one.
    call(HEAPu32Pop, &one, pad);
    call(HEAPu32Pop, &two, pad);
    call(HEAPu32Pop, &three, pad);
    testeqv((long long)(one), (long long)(1), "%lld");
    testeqv((long long)(two), (long long)(2), "%lld");
    testeqv((long long)(three), (long long)(3), "%lld");
    done;
}
ok64 HEAPtest() {
    sane(1);
    call(HEAPtest1);
    done;
}

TEST(HEAPtest)
