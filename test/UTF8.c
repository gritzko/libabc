#include "UTF8.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "S.h"
#include "INT.h"
#include "TEST.h"

ok64 UTF8test1() {
    sane(1);
    u8cs abc = $u8str("abc");
    u32 a, b, c;
    call(utf8sDrain32, &a, abc);
    call(utf8sDrain32, &b, abc);
    call(utf8sDrain32, &c, abc);
    same(a, 'a');
    same(b, 'b');
    same(c, 'c');
    done;
}

// ABC-003: UTF8_LEN byte-length table must report the exact lead-byte length.
// `utf8sDrain1utf8` copies UTF8_LEN[lead>>4] bytes, so an off-by-one table
// over-counts multibyte leads (and drops the 4-byte lead entirely).
struct utf8len_case {
    char const *name;
    u8 bytes[5];
    size_t len;  // expected UTF-8 byte length
    u32 cp;      // expected decoded codepoint
};

static struct utf8len_case const utf8len_cases[] = {
    {"ASCII A", {0x41}, 1, 0x41},
    {"Cyrillic Ya U+042F", {0xD0, 0xAF}, 2, 0x042F},
    {"Cyrillic Zhe U+0416", {0xD0, 0x96}, 2, 0x0416},
    {"em-dash U+2014", {0xE2, 0x80, 0x94}, 3, 0x2014},
    {"Katakana Ka U+30AB", {0xE3, 0x82, 0xAB}, 3, 0x30AB},
    {"Han Kan U+6F22", {0xE6, 0xBC, 0xA2}, 3, 0x6F22},
    {"emoji grin U+1F600", {0xF0, 0x9F, 0x98, 0x80}, 4, 0x1F600},
    {"emoji rocket U+1F680", {0xF0, 0x9F, 0x9A, 0x80}, 4, 0x1F680},
};

ok64 UTF8test2() {
    sane(1);
    for (size_t i = 0; i < sizeof(utf8len_cases) / sizeof(*utf8len_cases);
         i++) {
        struct utf8len_case const *c = &utf8len_cases[i];
        // Source slice holds exactly the multibyte sequence.
        u8cs from = {c->bytes, c->bytes + c->len};
        // Drain one whole UTF-8 character; this consults UTF8_LEN.
        a_pad(u8, into, 8);
        call(utf8sDrain1utf8, into_idle, from);
        // Exactly `len` bytes must be consumed from source ...
        same($len(from), 0);
        // ... and exactly `len` bytes must be written to the sink.
        same($len(into_data), (i64)c->len);
        // The drained bytes must decode back to the expected codepoint.
        u32 cp = 0;
        u8cs decode = {into_data[0], into_data[1]};
        call(utf8sDrain32, &cp, decode);
        same(cp, c->cp);
        same($len(decode), 0);
    }
    done;
}

// STD-26B068SZeb: overlong and out-of-range boundaries per RFC 3629 §3
struct utf8bad_case {
    u8 bytes[4];
    u8 len;
    ok64 o;
};

static struct utf8bad_case const utf8bad_cases[] = {
    {{0xC0, 0x80}, 2, UTFBAD},             {{0xC1, 0xBF}, 2, UTFBAD},
    {{0xC2, 0x80}, 2, OK},                 {{0xE0, 0x80, 0x80}, 3, UTFBAD},
    {{0xE0, 0x9F, 0xBF}, 3, UTFBAD},       {{0xE0, 0xA0, 0x80}, 3, OK},
    {{0xED, 0x9F, 0xBF}, 3, OK},           {{0xED, 0xA0, 0x80}, 3, UTFBAD},
    {{0xED, 0xBF, 0xBF}, 3, UTFBAD},       {{0xEE, 0x80, 0x80}, 3, OK},
    {{0xF0, 0x80, 0x80, 0x80}, 4, UTFBAD}, {{0xF0, 0x8F, 0xBF, 0xBF}, 4, UTFBAD},
    {{0xF0, 0x90, 0x80, 0x80}, 4, OK},     {{0xF4, 0x8F, 0xBF, 0xBF}, 4, OK},
    {{0xF4, 0x90, 0x80, 0x80}, 4, UTFBAD},
};

// STD-26B068SZeb: every 3- and 4-byte lead x continuation combination
// decodes iff its value is the shortest form of a scalar value.
ok64 UTF8test3() {
    sane(1);
    for (size_t i = 0; i < sizeof(utf8bad_cases) / sizeof(*utf8bad_cases);
         i++) {
        u32 cp = 0;
        u8cs d = {utf8bad_cases[i].bytes,
                  utf8bad_cases[i].bytes + utf8bad_cases[i].len};
        same(utf8sDrain32(&cp, d), utf8bad_cases[i].o);
    }
    for (u32 b0 = 0xE0; b0 < 0xF8; b0++)
        for (u32 b1 = 0x80; b1 < 0xC0; b1++)
            for (u32 b2 = 0x80; b2 < 0xC0; b2++)
                for (u32 b3 = 0x80; b3 < (b0 < 0xF0 ? 0x81 : 0xC0); b3++) {
                    u8 buf[4] = {(u8)b0, (u8)b1, (u8)b2, (u8)b3};
                    u32 v, cp = 0;
                    b8 ok;
                    if (b0 < 0xF0) {
                        v = (b0 & 15) << 12 | (b1 & 63) << 6 | (b2 & 63);
                        ok = v >= 0x800 && (v < 0xD800 || v > 0xDFFF);
                    } else {
                        v = (b0 & 7) << 18 | (b1 & 63) << 12 |
                            (b2 & 63) << 6 | (b3 & 63);
                        ok = v >= 0x10000 && v <= 0x10FFFF;
                    }
                    u8cs d = {buf, buf + (b0 < 0xF0 ? 3 : 4)};
                    same(utf8sDrain32(&cp, d), ok ? OK : UTFBAD);
                    if (ok) same(cp, v);
                }
    done;
}

// STD-26B068PXdv: the encoder refuses surrogates and values past U+10FFFF
// (RFC 3629 §3) writing nothing; every scalar value round-trips.
ok64 UTF8test4() {
    sane(1);
    for (u32 cp = 0; cp <= 0x110000; cp++) {
        a_pad(u8, buf, 4);
        b8 bad = (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF;
        same(utf8sFeed32(buf_idle, cp), bad ? UTFBAD : OK);
        if (bad) {
            same($len(buf_data), 0);
            continue;
        }
        u32 back = 0;
        u8cs d = {buf_data[0], buf_data[1]};
        call(utf8sDrain32, &back, d);
        same(back, cp);
        same($len(d), 0);
    }
    a_pad(u8, big, 4);
    same(utf8sFeed32(big_idle, 0xFFFFFFFF), UTFBAD);
    done;
}

ok64 UTF8test() {
    sane(1);
    call(UTF8test1);
    call(UTF8test2);
    call(UTF8test3);
    call(UTF8test4);
    done;
}

TEST(UTF8test);
