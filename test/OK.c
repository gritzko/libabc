#include "OK.h"

#include "PRO.h"
#include "TEST.h"

// ABC-008: repro: OKscan("OK") must yield 0 (success), and the
// print->scan round trip of OK must be the identity.
ok64 OKTestScanOK() {
    sane(1);
    a$str(oks, "OK");
    ok64 v = 99;
    call(OKscan, &v, oks);
    testeqv((long long)(v), (long long)(0), "%lld");
    // "KO" is NOT the success token; it must stay a plain ron60
    a$str(kos, "KO");
    ok64 k = 0;
    call(OKscan, &k, kos);
    want(k != 0);
    done;
}

ok64 OKTestPrintScanRoundTrip() {
    sane(1);
    con ok64 codes[] = {OK, NOROOM, NODATA, BADARG, FAILSANITY};
    for (u32 i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
        u8 buf[16];
        u8s into = {buf, buf + sizeof(buf)};
        call(OKprint, codes[i], into);
        u8c* from[2] = {buf, into[0]};
        ok64 back = FAIL;
        call(OKscan, &back, from);
        testeqv((long long)(back), (long long)(codes[i]), "%lld");
    }
    done;
}

ok64 OKtest() {
    sane(1);
    call(OKTestScanOK);
    call(OKTestPrintScanRoundTrip);
    done;
}

TEST(OKtest);
