#include "abc/ANSI.h"

#include <string.h>

#include "abc/PRO.h"
#include "abc/TEST.h"

typedef struct {
    const char *input;     // raw escape sequence
    int   ilen;            // input length (for NUL-bearing inputs)
    ok64  expect;          // OK or ANSIBAD
    int   consumed;        // bytes parsed (only for OK)
    u8    private;
    u8    final;
    u8    nparams;
    u32   params[CSI_MAX_PARAMS];
} CSICase;

#define IL(s) (int)(sizeof(s) - 1)

static const CSICase CASES[] = {
    // --- Bare final ---
    {"\x1b[H",         IL("\x1b[H"),         OK, 3, 0,   'H', 0, {0}},
    {"\x1b[J",         IL("\x1b[J"),         OK, 3, 0,   'J', 0, {0}},

    // --- Single param ---
    {"\x1b[5n",        IL("\x1b[5n"),        OK, 4, 0,   'n', 1, {5}},
    {"\x1b[2J",        IL("\x1b[2J"),        OK, 4, 0,   'J', 1, {2}},
    {"\x1b[31m",       IL("\x1b[31m"),       OK, 5, 0,   'm', 1, {31}},

    // --- Multi param ---
    {"\x1b[12;40H",    IL("\x1b[12;40H"),    OK, 8, 0,   'H', 2, {12,40}},
    {"\x1b[1;31;47m",  IL("\x1b[1;31;47m"),  OK, 10, 0,  'm', 3, {1,31,47}},
    {"\x1b[38;5;200m", IL("\x1b[38;5;200m"), OK, 11, 0,  'm', 3, {38,5,200}},

    // --- Private prefix (SGR mouse) ---
    {"\x1b[<0;5;10M",   IL("\x1b[<0;5;10M"),   OK, 10, '<', 'M', 3, {0,5,10}},
    {"\x1b[<2;80;24m",  IL("\x1b[<2;80;24m"),  OK, 11, '<', 'm', 3, {2,80,24}},
    {"\x1b[<64;5;10M",  IL("\x1b[<64;5;10M"),  OK, 11, '<', 'M', 3, {64,5,10}},

    // --- Private '?' (DEC modes) ---
    {"\x1b[?25h",       IL("\x1b[?25h"),       OK, 6, '?', 'h', 1, {25}},
    {"\x1b[?1049l",     IL("\x1b[?1049l"),     OK, 8, '?', 'l', 1, {1049}},

    // --- Multi-digit params ---
    {"\x1b[120;200H",   IL("\x1b[120;200H"),   OK, 10, 0, 'H', 2, {120, 200}},

    // --- Bad: incomplete ---
    {"\x1b[",           IL("\x1b["),           ANSIBAD, 0, 0, 0, 0, {0}},
    {"\x1b[<",          IL("\x1b[<"),          ANSIBAD, 0, 0, 0, 0, {0}},
    {"\x1b[5;",         IL("\x1b[5;"),         ANSIBAD, 0, 0, 0, 0, {0}},
    {"\x1b[5;10",       IL("\x1b[5;10"),       ANSIBAD, 0, 0, 0, 0, {0}},

    // --- Bad: wrong prefix ---
    {"[5H",             IL("[5H"),             ANSIBAD, 0, 0, 0, 0, {0}},
    {"\x1b]5H",         IL("\x1b]5H"),         ANSIBAD, 0, 0, 0, 0, {0}},
    {"\x1b[a",          IL("\x1b[a"),          OK, 3, 0, 'a', 0, {0}},  // 'a' is in 0x40-0x7E, valid

    // --- Trailing data after sequence (consumed = sequence length only) ---
    {"\x1b[Hxxx",       IL("\x1b[Hxxx"),       OK, 3, 0, 'H', 0, {0}},
    {"\x1b[<0;5;10MAB", IL("\x1b[<0;5;10MAB"), OK, 10, '<', 'M', 3, {0,5,10}},
};

#define NCASES (sizeof(CASES) / sizeof(CASES[0]))

ok64 ANSITestCSI() {
    sane(1);
    for (size_t i = 0; i < NCASES; i++) {
        const CSICase *tc = &CASES[i];
        u8c *base = (u8c *)tc->input;
        u8cs input = {base, base + tc->ilen};
        csi out = {};
        ok64 o = ANSIu8sDrainCSI(input, &out);

        if (o != tc->expect) {
            fprintf(stderr,
                    "FAIL [%zu]: got %s want %s\n",
                    i, ok64str(o), ok64str(tc->expect));
            fail(TESTFAIL);
        }
        if (o != OK) continue;

        int consumed = (int)(input[0] - base);
        if (consumed != tc->consumed) {
            fprintf(stderr, "FAIL [%zu]: consumed got %d want %d\n",
                    i, consumed, tc->consumed);
            fail(TESTFAIL);
        }
        if (out.private != tc->private) {
            fprintf(stderr, "FAIL [%zu]: private got 0x%02x want 0x%02x\n",
                    i, out.private, tc->private);
            fail(TESTFAIL);
        }
        if (out.final != tc->final) {
            fprintf(stderr, "FAIL [%zu]: final got 0x%02x want 0x%02x\n",
                    i, out.final, tc->final);
            fail(TESTFAIL);
        }
        if (out.nparams != tc->nparams) {
            fprintf(stderr, "FAIL [%zu]: nparams got %u want %u\n",
                    i, out.nparams, tc->nparams);
            fail(TESTFAIL);
        }
        for (u32 j = 0; j < out.nparams; j++) {
            if (out.params[j] != tc->params[j]) {
                fprintf(stderr,
                        "FAIL [%zu]: params[%u] got %u want %u\n",
                        i, j, out.params[j], tc->params[j]);
                fail(TESTFAIL);
            }
        }
    }
    done;
}

ok64 ANSItest() {
    sane(1);
    call(ANSITestCSI);
    done;
}

TEST(ANSItest);
