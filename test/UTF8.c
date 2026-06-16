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
        same($len(into_data), c->len);
        // The drained bytes must decode back to the expected codepoint.
        u32 cp = 0;
        u8cs decode = {into_data[0], into_data[1]};
        call(utf8sDrain32, &cp, decode);
        same(cp, c->cp);
        same($len(decode), 0);
    }
    done;
}

ok64 UTF8test() {
    sane(1);
    call(UTF8test1);
    call(UTF8test2);
    done;
}

TEST(UTF8test);
