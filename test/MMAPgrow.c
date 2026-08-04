// MEM-021 repro: the mmap grow sizers must never turn an oversize
// request into a silent SHRINK.  `round_power_of_2` did `1UL << 64`
// (UB) above 2^63 and returned 1 on x86; `MMAPmayresize` wrapped
// `Bsize + idle_size - has_idle` before rounding.  Either way the
// caller asked to grow and got a smaller mapping back, with OK.

#include "MMAP.h"

#include "01.h"
#include "PRO.h"
#include "TEST.h"

ok64 MMAPgrowRound() {
    sane(1);
    // the defined range is unchanged
    same(round_power_of_2(0), 0);
    same(round_power_of_2(1), 1);
    same(round_power_of_2(17), 32);
    same(round_power_of_2((u64)1 << 63), (u64)1 << 63);
    same(round_power_of_2(((u64)1 << 63) - 1), (u64)1 << 63);
    // above 2^63 no power of two fits a u64: say so, never shrink
    same(round_power_of_2(((u64)1 << 63) + 1), 0);
    same(round_power_of_2(u64max), 0);
    done;
}

ok64 MMAPgrowWrap() {
    sane(1);
    u8b buf = {};
    call(u8bMap, buf, 4096);
    a_cstr(txt, "0123456789abcdef");
    call(u8bFeed, buf, txt);
    i64 was = Bsize(buf);

    // DATA is 16 bytes, so has_idle < Bsize and the old
    // `Bsize + idle_size - has_idle` wrapped to 15, rounded to 16 and
    // remapped the buffer down to 16 bytes, returning OK.
    same(Bmayremap(buf, u64max), MMAPBADARG);
    same(Bsize(buf), was);
    same(Bmayremap(buf, u64max - 16), MMAPBADARG);
    same(Bsize(buf), was);

    // an honest grow still works
    call(Bmayremap, buf, 8192);
    want(Bsize(buf) >= 8192 + 16);
    call(u8bUnMap, buf);
    done;
}

ok64 MMAPgrowTest() {
    sane(1);
    call(MMAPgrowRound);
    call(MMAPgrowWrap);
    done;
}

TEST(MMAPgrowTest);
