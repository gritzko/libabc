#include "OK.h"
#include "POL.h"
#include "TEST.h"

ok64 RONTestFromTm() {
    sane(1);
    struct tm t = {
        .tm_year = 126,  // 2026
        .tm_mon = 0,     // January (0-based)
        .tm_mday = 9,
        .tm_hour = 12,
        .tm_min = 0,
        .tm_sec = 0,
    };
    ron60 r = 0;
    call(RONOfTime, &r, &t, 456);
    // Verify round-trip
    struct tm t2 = {};
    u32 ms2 = 0;
    call(RONToTime, r, &t2, &ms2);
    testeqv((long long)(t.tm_year), (long long)(t2.tm_year), "%lld");
    testeqv((long long)(t.tm_mon), (long long)(t2.tm_mon), "%lld");
    testeqv((long long)(t.tm_mday), (long long)(t2.tm_mday), "%lld");
    testeqv((long long)(t.tm_hour), (long long)(t2.tm_hour), "%lld");
    testeqv((long long)(t.tm_min), (long long)(t2.tm_min), "%lld");
    testeqv((long long)(t.tm_sec), (long long)(t2.tm_sec), "%lld");
    testeqv((long long)(456), (long long)((u32)ms2), "%lld");
    done;
}

//  RON-001: RONToTime is decode-tolerant; out-of-range calendar slots clamp
//  to the nearest valid value and return OK (was BADARG).
ok64 RONTestClamp() {
    sane(1);
    struct tm t = {};

    // Overflowed month (14) clamps to 12, hour (25) to 23, minute (60) to 59.
    ron60 over = 0;
    over |= ((u64)2 << (9 * 6));
    over |= ((u64)6 << (8 * 6));
    over |= ((u64)14 << (7 * 6));  // month 14 -> 12
    over |= ((u64)0 << (6 * 6));
    over |= ((u64)1 << (5 * 6));   // day 1
    over |= ((u64)25 << (4 * 6));  // hour 25 -> 23
    over |= ((u64)60 << (3 * 6));  // min 60 -> 59
    want(RONToTime(over, &t, NULL) == OK);
    testeqv((long long)t.tm_mon, (long long)11, "%lld");
    testeqv((long long)t.tm_hour, (long long)23, "%lld");
    testeqv((long long)t.tm_min, (long long)59, "%lld");

    // ms overflow: a valid ron60 (ms=0) + 1000 sets the ms field to 1000
    // (< 4096, so no carry into seconds); clamps to 999, time is unchanged.
    struct tm base = {
        .tm_year = 126,  // 2026
        .tm_mon = 0,
        .tm_mday = 9,
        .tm_hour = 10,
        .tm_min = 0,
        .tm_sec = 0,
    };
    ron60 valid = 0;
    call(RONOfTime, &valid, &base, 0);
    ron60 bad_ms = valid + 1000;
    struct tm tm2 = {};
    u32 ms2 = 0;
    want(RONToTime(bad_ms, &tm2, &ms2) == OK);
    testeqv((long long)tm2.tm_hour, (long long)10, "%lld");
    testeqv((long long)tm2.tm_min, (long long)0, "%lld");
    testeqv((long long)tm2.tm_sec, (long long)0, "%lld");
    testeqv((long long)ms2, (long long)999, "%lld");

    // seconds overflow: sec field 60 (60 << 12) clamps to 59, returns OK.
    ron60 bad_sec = valid + (60ull << 12);
    struct tm tm3 = {};
    want(RONToTime(bad_sec, &tm3, NULL) == OK);
    testeqv((long long)tm3.tm_sec, (long long)59, "%lld");
    testeqv((long long)tm3.tm_hour, (long long)10, "%lld");

    done;
}

ok64 RONTestRoundTrip() {
    sane(1);
    u8 buf[16];
    for (u64 i = 0; i <= 4096; ++i) {
        u8* p = buf;
        u8* end = buf + sizeof(buf);
        u8* into[2] = {p, end};
        call(RONutf8sFeed, into, i);
        u8c* from[2] = {buf, into[0]};
        ok64 back = 0;
        call(RONutf8sDrain, &back, from);
        testeqv((long long)(i), (long long)(back), "%lld");
    }
    done;
}

fun ron60 _r60(const char* s) {
    u8c* p = (u8c*)s;
    u8c* sl[2] = {p, p + strlen(s)};
    ron60 r = 0;
    RONutf8sDrain(&r, sl);
    return r;
}

ok64 RONTestNormInc() {
    sane(1);
    con char* cases[][2] = {
        {"1000000000", "1000000001"},
        {"7000000000", "7000000001"},
        {"8000000000", "8000000010"},
        {"A000000000", "A000000010"},
        {"G000000000", "G000000100"},
        {"O000000000", "O000001000"},
        {"a000000000", "a000010000"},
        {"i000000000", "i000100000"},
        {"q000000000", "q001000000"},
        {"~000000000", "~010000000"},
        // trailing digits don't affect octant
        {"1a00000000", "1a00000001"},
        {"a100000000", "a100010000"},
    };
    u8 count = sizeof(cases) / sizeof(cases[0]);
    for (u8 i = 0; i < count; i++) {
        ron60 got = ron60NormInc(_r60(cases[i][0]));
        testeqv((long long)(got), (long long)(_r60(cases[i][1])), "%lld");
    }
    done;
}

ok64 RONTestInc() {
    sane(1);
    con char* cases[][2] = {
        {"1", "1000000001"},
        {"7", "7000000001"},
        {"8", "800000001"},
        {"A", "A00000001"},
        {"G", "G0000001"},
        {"O", "O000001"},
        {"a", "a00001"},
        {"i", "i0001"},
        {"q", "q001"},
        {"~", "~01"},
        {"a1", "a10001"},
        {"11", "1100000001"},
    };
    u8 count = sizeof(cases) / sizeof(cases[0]);
    for (u8 i = 0; i < count; i++) {
        ron60 got = ron60Inc(_r60(cases[i][0]));
        testeqv((long long)(got), (long long)(_r60(cases[i][1])), "%lld");
    }
    done;
}

ok64 RONTestNormInk() {
    sane(1);
    con char* cases[][2] = {
        {"1000000000", "2000000000"},
        {"7000000000", "8000000000"},
        {"8000000000", "8100000000"},
        {"A000000000", "A100000000"},
        {"G000000000", "G010000000"},
        {"O000000000", "O001000000"},
        {"a000000000", "a000100000"},
        {"i000000000", "i000010000"},
        {"q000000000", "q000001000"},
        {"~000000000", "~000000100"},
        // trailing digits don't affect octant
        {"1a00000000", "2a00000000"},
        {"a100000000", "a100100000"},
    };
    u8 count = sizeof(cases) / sizeof(cases[0]);
    for (u8 i = 0; i < count; i++) {
        ron60 got = ron60NormInk(_r60(cases[i][0]));
        testeqv((long long)(got), (long long)(_r60(cases[i][1])), "%lld");
    }
    done;
}

ok64 RONTestInk() {
    sane(1);
    con char* cases[][2] = {
        {"1", "2"},
        {"7", "8"},
        {"8", "81"},
        {"A", "A1"},
        {"G", "G01"},
        {"O", "O001"},
        {"a", "a0001"},
        {"i", "i00001"},
        {"q", "q000001"},
        {"~", "~0000001"},
        {"a1", "a1001"},
        {"11", "21"},
    };
    u8 count = sizeof(cases) / sizeof(cases[0]);
    for (u8 i = 0; i < count; i++) {
        ron60 got = ron60Ink(_r60(cases[i][0]));
        testeqv((long long)(got), (long long)(_r60(cases[i][1])), "%lld");
    }
    done;
}

ok64 RONTestNowMonotone() {
    sane(1);
    enum { N = 1000 };
    ron60 ts[N];
    ts[0] = RONNow();
    for (int i = 1; i < N; i++) {
        ts[i] = RONNow();
    }
    for (int i = 1; i < N; i++) {
        test(ts[i] >= ts[i - 1], FAIL);
    }
    // RONNow has ms resolution; 1000 tight iterations may finish within
    // a single tick on fast machines, so sleep to guarantee advancement.
    POLSleep(2 * POLNanosPerMSec);
    ron60 after = RONNow();
    test(after > ts[0], FAIL);
    done;
}

//  Recover the Unix-epoch seconds a ron60 stamp encodes the same way
//  sniff's at_ts_of_ron60 does (localtime-written ron60 → mktime).  Used
//  only by the SOURCE_DATE_EPOCH test to prove the override is tz-stable.
static long long _epoch_of_ron(ron60 r) {
    struct tm tm = {};
    u32 ms = 0;
    if (RONToTime(r, &tm, &ms) != OK) return -1;
    tm.tm_isdst = -1;
    return (long long)mktime(&tm);
}

//  DIS-051: RONNow honours SOURCE_DATE_EPOCH (reproducible-build clock).
//  When set, two calls yield the SAME ron60 whose recovered epoch equals
//  the env value, tz-independently; unset restores the wall clock.
ok64 RONTestSourceDateEpoch() {
    sane(1);
    char *saved = getenv("SOURCE_DATE_EPOCH");
    char saved_buf[64] = {};
    if (saved) { strncpy(saved_buf, saved, sizeof(saved_buf) - 1); saved = saved_buf; }
    char *saved_tz = getenv("TZ");
    char tz_buf[64] = {};
    if (saved_tz) { strncpy(tz_buf, saved_tz, sizeof(tz_buf) - 1); saved_tz = tz_buf; }

    //  A few epochs across DST boundaries; each tested under several zones
    //  to prove the localtime→mktime round-trip cancels the tz offset.
    con long long epochs[] = {
        946771200LL,   // 2000-01-02 00:00:00 UTC (inside the domain in ALL zones)
        1262304000LL,  // 2010-01-01 (winter)
        1467331200LL,  // 2016-07-01 (summer / DST)
        1700000000LL,  // 2023-11-14
    };
    con char *zones[] = { "UTC", "America/New_York", "Asia/Kolkata" };
    u8 ne = sizeof(epochs) / sizeof(epochs[0]);
    u8 nz = sizeof(zones) / sizeof(zones[0]);
    for (u8 z = 0; z < nz; z++) {
        setenv("TZ", zones[z], 1);
        tzset();
        for (u8 i = 0; i < ne; i++) {
            char ebuf[32] = {};
            snprintf(ebuf, sizeof(ebuf), "%lld", epochs[i]);
            setenv("SOURCE_DATE_EPOCH", ebuf, 1);
            ron60 a = RONNow();
            POLSleep(2 * POLNanosPerMSec);   // ensure not just "same ms"
            ron60 b = RONNow();
            //  Deterministic: pinned, so two calls agree exactly.
            testeqv((long long)a, (long long)b, "%lld");
            //  tz-stable: recovered epoch equals the env value in every zone.
            testeqv(_epoch_of_ron(a), epochs[i], "%lld");
        }
    }

    //  Unset → wall clock again (not pinned to the last epoch).
    unsetenv("SOURCE_DATE_EPOCH");
    setenv("TZ", "UTC", 1); tzset();
    ron60 wall = RONNow();
    want(_epoch_of_ron(wall) != epochs[0]);

    //  Restore the inherited environment.
    if (saved) setenv("SOURCE_DATE_EPOCH", saved, 1);
    else unsetenv("SOURCE_DATE_EPOCH");
    if (saved_tz) setenv("TZ", saved_tz, 1); else unsetenv("TZ");
    tzset();
    done;
}

ok64 RONTestFeedPad() {
    sane(1);
    con char* cases[][3] = {
        {"0", "1", "0"},
        {"0", "2", "00"},
        {"0", "3", "000"},
        {"1", "2", "01"},
        {"~", "1", "~"},
        {"10", "2", "10"},
        {"~~", "2", "~~"},
        {"100", "3", "100"},
    };
    u8 count = sizeof(cases) / sizeof(cases[0]);
    for (u8 i = 0; i < count; i++) {
        ok64 val = _r60(cases[i][0]);
        u8 width = (u8)atoi(cases[i][1]);
        const char* expected = cases[i][2];
        u8 buf[16] = {};
        u8s into = {buf, buf + sizeof(buf)};
        call(RONu8sFeedPad, into, val, width);
        u8c* exp_s = (u8c*)expected;
        u8c* exp_sl[2] = {exp_s, exp_s + strlen(expected)};
        u8c* got_sl[2] = {buf, buf + width};
        want($cmp(got_sl, exp_sl) == 0);
    }
    // overflow: val=64 ("10"), width=1 should fail
    u8 buf2[16];
    u8s into2 = {buf2, buf2 + sizeof(buf2)};
    want(RONu8sFeedPad(into2, 64, 1) != OK);
    done;
}

ok64 RONTestSpliceBase() {
    sane(1);
    ok64 base = 0;
    u8 width = 0;
    // prob=1000, n=100: need=200000, 64^3=262144 -> width >= 3
    call(RONSpliceBase, &base, &width, 12345, 1000, 100);
    want(width >= 3);
    u64 space = 1;
    for (u8 i = 0; i < width; i++) space *= 64;
    want(base + 100 <= space);
    // prob=64, n=1: need=128, 64^2=4096 -> width >= 2
    call(RONSpliceBase, &base, &width, 99999, 64, 1);
    want(width >= 2);
    // different rand -> different base
    ok64 base1 = 0, base2 = 0;
    u8 w1 = 0, w2 = 0;
    call(RONSpliceBase, &base1, &w1, 111, 1000, 10);
    call(RONSpliceBase, &base2, &w2, 999, 1000, 10);
    want(base1 != base2);
    // n=0 -> error
    want(RONSpliceBase(&base, &width, 0, 1000, 0) != OK);
    // prob=0 -> error
    want(RONSpliceBase(&base, &width, 0, 0, 10) != OK);
    done;
}

ok64 RONTestSpliceKeyOrder() {
    sane(1);
    con ok64 n = 50;
    ok64 base = 0;
    u8 width = 0;
    call(RONSpliceBase, &base, &width, 42, 1000, n);
    u8 keys[50][12] = {};
    for (ok64 i = 0; i < n; i++) {
        u8s into = {keys[i], keys[i] + sizeof(keys[i])};
        call(RONu8sFeedPad, into, base + i, width);
    }
    for (ok64 i = 0; i + 1 < n; i++) {
        u8c* a[2] = {keys[i], keys[i] + width};
        u8c* b[2] = {keys[i + 1], keys[i + 1] + width};
        want($cmp(a, b) < 0);
    }
    done;
}

ok64 RONTestSpliceIsolation() {
    sane(1);
    con ok64 n = 10;
    con u64 prob = 1000;
    enum { trials = 10000 };
    ok64 bases[trials];
    u8 width = 0;
    for (int i = 0; i < trials; i++) {
        // simple LCG for deterministic pseudo-random
        u64 rand = (u64)i * 6364136223846793005UL + 1442695040888963407UL;
        call(RONSpliceBase, &bases[i], &width, rand, prob, n);
    }
    int overlaps = 0;
    // check first 1000 pairs for overlaps
    for (int i = 0; i < 1000; i++) {
        for (int j = i + 1; j < 1000; j++) {
            ok64 lo1 = bases[i], hi1 = bases[i] + n;
            ok64 lo2 = bases[j], hi2 = bases[j] + n;
            if (lo1 < hi2 && lo2 < hi1) overlaps++;
        }
    }
    // expect overlap rate < 2/prob ~ 0.2% of pairs
    // 1000*999/2 = 499500 pairs, 2/prob = 0.002, so expect < ~999 overlaps
    want(overlaps < (int)(2 * 499500 / prob));
    done;
}

// ABC-008: repro: chained pads must advance the cursor and be
// all-or-nothing (no partial garbage on SBADARG).
ok64 RONTestFeedPadChained() {
    sane(1);
    u8 buf[16] = {};
    u8s into = {buf, buf + sizeof(buf)};
    call(RONu8sFeedPad, into, _r60("1"), 2);
    call(RONu8sFeedPad, into, _r60("2"), 2);
    a$str(exp, "0102");
    u8c* got[2] = {buf, buf + 4};
    want($cmp(got, exp) == 0);
    want(into[0] == buf + 4);
    u8 buf2[4] = {'#', '#', '#', '#'};
    u8s into2 = {buf2, buf2 + sizeof(buf2)};
    want(RONu8sFeedPad(into2, 64, 1) != OK);
    want(into2[0] == buf2 && buf2[0] == '#');
    done;
}

// ABC-008: repro: drain must cap input like the feed side caps
// output; 64-bit values must still round-trip.
ok64 RONTestDrainCap() {
    sane(1);
    ok64 v = 0;
    a$str(long12, "010000000000");
    want(RONutf8sDrain(&v, long12) != OK);
    a$str(over11, "G0000000000");
    want(RONutf8sDrain(&v, over11) != OK);
    u8 buf[16];
    u8s into = {buf, buf + sizeof(buf)};
    call(RONutf8sFeed, into, ron60Max);
    u8c* from[2] = {buf, into[0]};
    call(RONutf8sDrain, &v, from);
    want(v == ron60Max);
    done;
}

// ABC-008: repro: splice sizing must not wrap 2*prob*n or divide
// by zero when the space is exhausted.
ok64 RONTestSpliceBaseOverflow() {
    sane(1);
    ok64 base = 0;
    u8 width = 0;
    call(RONSpliceBase, &base, &width, 5, 1UL << 57, 64);
    want(width == 10);
    want(RONSpliceBase(&base, &width, 5, 1, 1UL << 60) != OK);
    done;
}

ok64 RONtest() {
    sane(1);
    call(RONTestFromTm);
    call(RONTestClamp);
    call(RONTestRoundTrip);
    call(RONTestNormInc);
    call(RONTestInc);
    call(RONTestNormInk);
    call(RONTestInk);
    call(RONTestNowMonotone);
    call(RONTestSourceDateEpoch);
    call(RONTestFeedPad);
    call(RONTestFeedPadChained);
    call(RONTestDrainCap);
    call(RONTestSpliceBase);
    call(RONTestSpliceBaseOverflow);
    call(RONTestSpliceKeyOrder);
    call(RONTestSpliceIsolation);
    done;
}

TEST(RONtest);
