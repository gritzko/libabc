#include "HEX.h"

#include "PRO.h"
#include "TEST.h"

// ABC-008: repro: bad low nibble ("0g") must be rejected, not
// silently decoded to 0xff.
ok64 HEXTestDrainBadLowNibble() {
    sane(1);
    a$str(hex, "0g");
    a_pad(u8, bin, 4);
    testeqv((long long)(HEXu8sDrainSome(bin_idle, hex)),
            (long long)(HEXBAD), "%lld");
    a$str(hex2, "g0");
    a_pad(u8, bin2, 4);
    testeqv((long long)(HEXu8sDrainSome(bin2_idle, hex2)),
            (long long)(HEXBAD), "%lld");
    done;
}

ok64 HEXTestRoundTrip() {
    sane(1);
    u8 raw[] = {0x00, 0x1a, 0xf0, 0xff};
    a$(u8c, bin, raw);
    a_pad(u8, hex, 8);
    call(HEXu8sFeedSome, hex_idle, bin);
    a$str(exp, "001af0ff");
    want($eq(hex_data, exp));
    a_pad(u8, back, 4);
    a_dup(u8c, hexc, hex_datac);
    call(HEXu8sDrainSome, back_idle, hexc);
    a$(u8c, orig, raw);
    want($eq(back_data, orig));
    done;
}

ok64 HEXtest() {
    sane(1);
    call(HEXTestDrainBadLowNibble);
    call(HEXTestRoundTrip);
    done;
}

TEST(HEXtest);
