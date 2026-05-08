#include "NUM.h"
#include "PRO.h"
#include "TEST.h"

ok64 NUMTest1() {
    sane(1);
    a_pad(u8, buf, 512);

    // Zero
    call(NUMu8sFeed, buf_idle, 0);
    a_cstr(zero, "zero");
    testeqv((long long)(u8bDataLen(buf)), (long long)(u8csLen(zero)), "%lld");
    u8bReset(buf);

    // Single digits
    call(NUMu8sFeed, buf_idle, 1);
    a_cstr(one, "one");
    testeqv((long long)(u8bDataLen(buf)), (long long)(u8csLen(one)), "%lld");
    u8bReset(buf);

    call(NUMu8sFeed, buf_idle, 9);
    a_cstr(nine, "nine");
    testeqv((long long)(u8bDataLen(buf)), (long long)(u8csLen(nine)), "%lld");

    done;
}

ok64 NUMTest2() {
    sane(1);
    a_pad(u8, buf, 512);

    // Teens
    call(NUMu8sFeed, buf_idle, 13);
    a_cstr(thirteen, "thirteen");
    testeqv((long long)(u8bDataLen(buf)), (long long)(u8csLen(thirteen)), "%lld");
    u8bReset(buf);

    call(NUMu8sFeed, buf_idle, 19);
    a_cstr(nineteen, "nineteen");
    testeqv((long long)(u8bDataLen(buf)), (long long)(u8csLen(nineteen)), "%lld");
    u8bReset(buf);

    // Tens
    call(NUMu8sFeed, buf_idle, 42);
    a_cstr(fortytwo, "forty two");
    testeqv((long long)(u8bDataLen(buf)), (long long)(u8csLen(fortytwo)), "%lld");
    u8bReset(buf);

    call(NUMu8sFeed, buf_idle, 99);
    a_cstr(ninetynine, "ninety nine");
    testeqv((long long)(u8bDataLen(buf)), (long long)(u8csLen(ninetynine)), "%lld");

    done;
}

ok64 NUMTest3() {
    sane(1);
    a_pad(u8, buf, 512);

    // Hundreds
    call(NUMu8sFeed, buf_idle, 100);
    a_cstr(hundred, "one hundred");
    testeqv((long long)(u8bDataLen(buf)), (long long)(u8csLen(hundred)), "%lld");
    u8bReset(buf);

    call(NUMu8sFeed, buf_idle, 123);
    a_cstr(oneTwoThree, "one hundred twenty three");
    testeqv((long long)(u8bDataLen(buf)), (long long)(u8csLen(oneTwoThree)), "%lld");
    u8bReset(buf);

    // Thousands
    call(NUMu8sFeed, buf_idle, 1000);
    a_cstr(thousand, "one thousand");
    testeqv((long long)(u8bDataLen(buf)), (long long)(u8csLen(thousand)), "%lld");
    u8bReset(buf);

    call(NUMu8sFeed, buf_idle, 12345);
    a_cstr(n12345, "twelve thousand three hundred forty five");
    testeqv((long long)(u8bDataLen(buf)), (long long)(u8csLen(n12345)), "%lld");

    done;
}

ok64 NUMTest4() {
    sane(1);
    a_pad(u8, buf, 512);

    // Million
    call(NUMu8sFeed, buf_idle, 1000000);
    a_cstr(million, "one million");
    testeqv((long long)(u8bDataLen(buf)), (long long)(u8csLen(million)), "%lld");
    u8bReset(buf);

    // Billion
    call(NUMu8sFeed, buf_idle, 1000000000ULL);
    a_cstr(billion, "one billion");
    testeqv((long long)(u8bDataLen(buf)), (long long)(u8csLen(billion)), "%lld");
    u8bReset(buf);

    // Trillion
    call(NUMu8sFeed, buf_idle, 1000000000000ULL);
    a_cstr(trillion, "one trillion");
    testeqv((long long)(u8bDataLen(buf)), (long long)(u8csLen(trillion)), "%lld");
    u8bReset(buf);

    // Complex large number
    call(NUMu8sFeed, buf_idle, 1234567890ULL);
    want(u8bDataLen(buf) > 50);

    done;
}

ok64 NUMTest5() {
    sane(1);
    a_pad(u8, buf, 512);

    // Very large numbers (quadrillion, quintillion)
    call(NUMu8sFeed, buf_idle, 1000000000000000ULL);
    a_cstr(quadrillion, "one quadrillion");
    testeqv((long long)(u8bDataLen(buf)), (long long)(u8csLen(quadrillion)), "%lld");
    u8bReset(buf);

    call(NUMu8sFeed, buf_idle, 1000000000000000000ULL);
    a_cstr(quintillion, "one quintillion");
    testeqv((long long)(u8bDataLen(buf)), (long long)(u8csLen(quintillion)), "%lld");
    u8bReset(buf);

    // Near UINT64_MAX
    call(NUMu8sFeed, buf_idle, UINT64_MAX);
    want(u8bDataLen(buf) > 100);

    done;
}

ok64 NUMTests() {
    sane(1);
    call(NUMTest1);
    call(NUMTest2);
    call(NUMTest3);
    call(NUMTest4);
    call(NUMTest5);
    done;
}

TEST(NUMTests);
