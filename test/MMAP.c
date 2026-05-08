#include "MMAP.h"

#include "INT.h"
#include "PRO.h"
#include "TEST.h"

ok64 MMAPtest1() {
    sane(1);
    u8b buf8 = {};
    call(u8bMap, buf8, 4096);
    aB(u32, word);
    call(u32bMap, wordbuf, 1024);
    testeqv((long long)(Bsize(buf8)), (long long)(Bsize(wordbuf)), "%lld");

    Bat(buf8, 0) = 0xaa;
    Bat(buf8, 1) = 0xbb;
    Bat(buf8, 2) = 0xcc;
    Bat(buf8, 3) = 0xdd;
    $copy(wordidle, u8bIdle(buf8));
    testeqv((long long)(Bat(wordbuf, 0)), (long long)(0xddccbbaa), "%lld");

    call(Bremap2, buf8);
    call(Bmayremap, wordbuf, 2048);
    testeqv((long long)(Bsize(buf8)), (long long)(Bsize(wordbuf)), "%lld");

    Bat(buf8, 8188) = 0xaa;
    Bat(buf8, 8189) = 0xbb;
    Bat(buf8, 8190) = 0xcc;
    Bat(buf8, 8191) = 0xee;
    $copy(u32bIdle(wordbuf), u8bIdle(buf8));
    testeqv((long long)(Bat(wordbuf, 2047)), (long long)(0xeeccbbaa), "%lld");
    testeqv((long long)(Bat(wordbuf, 0)), (long long)(0xddccbbaa), "%lld");

    call(u8bUnMap, buf8);
    call(u32bUnMap, wordbuf);
    done;
}

ok64 MMAPtest() {
    sane(1);
    call(MMAPtest1);
    done;
}

TEST(MMAPtest);
