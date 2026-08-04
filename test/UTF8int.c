// MEM-021 repro: utf8sFeedInt did `u = -*i`, which is signed-overflow
// UB for INT64_MIN (a UBSan build aborts on it; a plain build gets the
// right digits only by accident).  utf8sDrainInt already guards the
// magnitude, so the round trip is the natural pin.

#include "UTF8.h"

#include "01.h"
#include "PRO.h"
#include "TEST.h"

static ok64 UTF8intOne(i64 v, char const *txt) {
    sane(1);
    a_pad(u8, out, 32);
    call(utf8sFeedInt, out_idle, &v);
    a_cstr(want_txt, txt);
    want(u8csEq(out_datac, want_txt));
    i64 back = 0;
    a_dup(u8c, src, out_datac);
    call(utf8sDrainInt, src, &back);
    same(back, v);
    done;
}

ok64 UTF8intTest() {
    sane(1);
    call(UTF8intOne, 0, "0");
    call(UTF8intOne, 42, "42");
    call(UTF8intOne, -42, "-42");
    call(UTF8intOne, i64MaxValue, "9223372036854775807");
    call(UTF8intOne, INT64_MIN, "-9223372036854775808");
    done;
}

TEST(UTF8intTest);
