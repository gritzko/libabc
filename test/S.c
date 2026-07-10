#include "S.h"

#include <assert.h>
#include <unistd.h>

#include "INT.h"
#include "TEST.h"

ok64 $test1() {
    sane(1);
    a_pad(i32, pad, 4);
    i32 a1 = 1;
    i32 a2 = 0;
    i32$ into = i32bIdle(pad);
    i32c$ data = i32bDataC(pad);

    call(i32sFeed1, into, a2);
    call(i32sFeedP, into, &a1);
    want($len(data) == 2);
    call($i32feed, into, data);
    want($len(data) == 4);
    want($at(data, 0) == 0);
    want($at(data, 1) == 1);
    want($at(data, 2) == 0);
    want($at(data, 3) == 1);
    want(SNOROOM == i32sFeed1(into, a1));

    aBpad(i32, padi2, 4);
    i32$ into2 = i32bIdle(padi2);
    i32sCopy(into2, data);  // FIXME
    want($eq(into2, data));

    i32Swap(&a1, &a2);
    want(a1 == 0 && a2 == 1);
    i32mv(&a1, &a2);
    want(a1 == a2 && a1 == 1);
    done;
}

ok64 $test2() {
    sane(1);
    aBpad(i32, pad, 8);
    i32$ into = i32bIdle(pad);
    i32$ data = i32bData(pad);
    call(i32sFeed1, into, 4);
    call(i32sFeed1, into, 7);
    call(i32sFeed1, into, 2);
    call(i32sFeed1, into, 5);
    call(i32sFeed1, into, 0);
    call(i32sFeed1, into, 1);
    call(i32sFeed1, into, 3);
    call(i32sFeed1, into, 6);

    i32sSort(data);

    a_dup(i32, d, data);
    int j = 0;
    $eat(d) want(**d == j++);

    for (i32 i = 0; i < $len(data); i++) {
        want($at(data, i) == i);
        i32* p = i32sBinSearch(&i, data);
        want(p - *data == i);
    }

    done;
}

ok64 findtest() {
    sane(1);
    u8 data[] = "hello:world";
    u8cs slice = {data, data + 11};

    // Find existing character
    a_dup(u8c,s1,slice);
    want(u8csFind(s1, ':') == OK);
    want(*s1 == data + 5);
    want(**s1 == ':');

    // Find first character
    a_dup(u8c,s2,slice);
    want(u8csFind(s2, 'h') == OK);
    want(*s2 == data);

    // Find last character
    a_dup(u8c,s3,slice);
    want(u8csFind(s3, 'd') == OK);
    want(*s3 == data + 10);

    // Character not found
    a_dup(u8c,s4,slice);
    want(u8csFind(s4, 'z') == NONE);

    // Test sFind (mutable slice)
    u8s mslice = {data, data + 11};
    want(u8sFind(mslice, ':') == OK);
    want(*mslice == data + 5);

    // Test repeated search
    a_dup(u8c,s5,slice);
    int count = 0;
    while (u8csFind(s5, 'l') == OK) {
        count++;
        ++*s5;
    }
    want(count == 3);  // 'l' appears 3 times in "hello:world"

    done;
}

ok64 findStest() {
    sane(1);
    u8 data[] = "hello world, hello universe";
    u8cs haystack = {data, data + 27};

    // Find "world"
    u8 needle1[] = "world";
    u8cs n1 = {needle1, needle1 + 5};
    a_dup(u8c,h1,haystack);
    want(u8csFindS(h1, n1) == OK);
    want(*h1 == data + 6);

    // Find "hello" (first occurrence)
    u8 needle2[] = "hello";
    u8cs n2 = {needle2, needle2 + 5};
    a_dup(u8c,h2,haystack);
    want(u8csFindS(h2, n2) == OK);
    want(*h2 == data);

    // Find "universe" (at end)
    u8 needle3[] = "universe";
    u8cs n3 = {needle3, needle3 + 8};
    a_dup(u8c,h3,haystack);
    want(u8csFindS(h3, n3) == OK);
    want(*h3 == data + 19);

    // Not found
    u8 needle4[] = "foo";
    u8cs n4 = {needle4, needle4 + 3};
    a_dup(u8c,h4,haystack);
    want(u8csFindS(h4, n4) == NONE);

    // Needle longer than haystack
    u8 short_data[] = "hi";
    u8cs short_hay = {short_data, short_data + 2};
    u8 long_needle[] = "hello";
    u8cs ln = {long_needle, long_needle + 5};
    a_dup(u8c,h5,short_hay);
    want(u8csFindS(h5, ln) == NONE);

    // Regression (fuzz crash-d402460f via spot grep): the needle's first
    // byte appears within nlen-1 of the term, so csFind advances the
    // cursor there and the memcmp must NOT read past the haystack end.
    // Exact-sized stack array so ASAN traps any over-read.
    u8 tail_hay[5] = {'a', 'a', 'a', 'a', 'A'};
    u8cs th = {tail_hay, tail_hay + 5};
    u8 tail_ndl[] = "ABCDE";              // 5-byte needle, first byte at hay[4]
    u8cs tn = {tail_ndl, tail_ndl + 5};
    a_dup(u8c, hT, th);
    want(u8csFindS(hT, tn) == NONE);

    // Empty needle
    u8cs empty = {needle1, needle1};
    a_dup(u8c,h6,haystack);
    want(u8csFindS(h6, empty) == NONE);

    // Find with repeated first char: "llo" in "hello world, hello universe"
    u8 needle5[] = "llo";
    u8cs n5 = {needle5, needle5 + 3};
    a_dup(u8c,h7,haystack);
    want(u8csFindS(h7, n5) == OK);
    want(*h7 == data + 2);  // "llo" starts at index 2

    // Test skipping: "ld" in "hello world" - 'l' at 2,3,9 but "ld" at 9
    u8 data2[] = "hello world";
    u8cs hay2 = {data2, data2 + 11};
    u8 needle6[] = "ld";
    u8cs n6 = {needle6, needle6 + 2};
    a_dup(u8c,h8,hay2);
    want(u8csFindS(h8, n6) == OK);
    want(*h8 == data2 + 9);  // "ld" at index 9

    // Test repeated search: find all "hello" occurrences
    a_dup(u8c,h9,haystack);
    int count = 0;
    while (u8csFindS(h9, n2) == OK) {
        count++;
        ++*h9;
    }
    want(count == 2);  // "hello" appears twice

    done;
}

// ABC-005 repro: purge predicate for the table-driven purge test
fun b8 i32is2(i32 const* p) { return *p == 2; }

ok64 purgetest() {
    sane(1);
    // ABC-005 repro: a matching element swapped in from the tail
    // must be re-tested, not skipped (it used to survive the purge)
    struct {
        i32 in[8];
        size_t n;
        i32 out[8];  // expected survivors, sorted
        size_t m;
    } tt[] = {
        {{2, 5, 2}, 3, {5}, 1},
        {{2, 2, 2}, 3, {0}, 0},
        {{1, 2, 3, 2, 2, 4}, 6, {1, 3, 4}, 3},
        {{1, 3}, 2, {1, 3}, 2},
        {{2}, 1, {0}, 0},
        {{5, 2}, 2, {5}, 1},
    };
    for (size_t t = 0; t < sizeof(tt) / sizeof(tt[0]); ++t) {
        i32 pad[8];
        memcpy(pad, tt[t].in, sizeof(pad));
        i32s s = {pad, pad + tt[t].n};
        i32s_purge(s, &i32is2);
        want($len(s) == (long)tt[t].m);
        i32sSort(s);
        i32cs exp = {tt[t].out, tt[t].out + tt[t].m};
        want($eq(s, exp));
    }
    done;
}

ok64 rmtest() {
    sane(1);
    // ABC-005 repro: $rm must move the whole tail, not just len elements
    i32 pad[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    i32s s = {pad, pad + 8};
    $rm(s, 1, 2);
    i32c exp[6] = {0, 3, 4, 5, 6, 7};
    i32cs e = {exp, exp + 6};
    want($len(s) == 6);
    want($eq(s, e));

    // ABC-005 repro: off+2*len > $len used to read past term (ASAN)
    i32 pad2[6] = {0, 1, 2, 3, 4, 5};
    i32s s2 = {pad2, pad2 + 6};
    $rm(s2, 3, 3);
    i32c exp2[3] = {0, 1, 2};
    i32cs e2 = {exp2, exp2 + 3};
    want($len(s2) == 3);
    want($eq(s2, e2));

    // ABC-005 repro: $rm1 must move the whole tail, not one element
    i32 pad3[4] = {0, 1, 2, 3};
    i32s s3 = {pad3, pad3 + 4};
    $rm1(s3, 1);
    i32c exp3[3] = {0, 2, 3};
    i32cs e3 = {exp3, exp3 + 3};
    want($len(s3) == 3);
    want($eq(s3, e3));

    // ABC-005: $tailshift is the reference tail memmove; it did not even
    // compile before (must() arity) — exercise it the same way as $rm
    i32 pad4[5] = {0, 1, 2, 3, 4};
    i32s s4 = {pad4, pad4 + 5};
    $tailshift(s4, 1, 2);
    s4[1] -= 2;
    i32c exp4[3] = {0, 3, 4};
    i32cs e4 = {exp4, exp4 + 3};
    want($eq(s4, e4));
    done;
}

ok64 draintest() {
    sane(1);
    // ABC-005 repro: Drain contract — write what fits, advance BOTH
    // sides; the old code was all-or-nothing and never advanced `from`
    i32 src[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    i32cs from = {src, src + 8};
    i32 got[8];
    i32s out = {got, got + 8};
    int rounds = 0;
    while (!$empty(from) && rounds < 16) {
        i32 chunk[3];
        i32s into = {chunk, chunk + 3};
        call(i32sDrain, into, from);
        i32cs part = {chunk, into[0]};
        want(!$empty(part));
        call($i32feed, out, part);
        ++rounds;
    }
    want(rounds == 3);
    want($empty(from));
    want(out[0] == got + 8);
    i32cs all = {src, src + 8};
    i32cs copied = {got, got + 8};
    want($eq(copied, all));
    done;
}

ok64 feedftest() {
    sane(1);
    // ABC-005 repro: truncated feed of the LAST template item used to
    // advance the cursor and return OK on truncated output
    a$str(world, "world");
    u8 buf[3];
    u8s into = {buf, buf + 3};
    a$str(t1, "x$s");
    want(SNOROOM == $feedf(into, t1, world));

    // ABC-005 repro: $u truncation counted the snprintf NUL as payload
    u8 buf2[4];
    u8s into2 = {buf2, buf2 + 4};
    a$str(t2, "$u");
    want(SNOROOM == $feedf(into2, t2, (u64)12345));
    want(into2[0] - buf2 <= 3);

    // exact-fitting template must still succeed
    u8 buf3[16];
    u8s into3 = {buf3, buf3 + 16};
    a$str(t3, "n=$u;$s");
    want(OK == $feedf(into3, t3, (u64)42, world));
    u8cs got = {buf3, into3[0]};
    a$str(exp, "n=42;world");
    want($eq(got, exp));
    done;
}

ok64 $test() {
    sane(1);
    call($test1);
    call($test2);
    call(findtest);
    call(findStest);
    call(purgetest);
    call(rmtest);
    call(draintest);
    call(feedftest);
    done;
}

TEST($test);
