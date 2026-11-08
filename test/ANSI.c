#include "ANSI.h"

#include <string.h>

#include "PRO.h"
#include "TEST.h"

typedef struct {
    const char *input;     // raw escape sequence
    int   ilen;            // input length (for NUL-bearing inputs)
    ok64  expect;          // OK or ANSIBAD
    int   consumed;        // bytes parsed (only for OK)
    u8    private;
    u8    final;
    u8    nparams;
    u32   params[CSI_MAX_PARAMS];
    u16   subs;            // sub-param bitmask
    const char *inter;     // intermediate bytes, NULL = none
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

    // --- STD-26B068qhvl: intermediates, sub-params, empty params ---
    {"\x1b[2 q",   IL("\x1b[2 q"),   OK, 5, 0,   'q', 1, {2}, 0, " "},
    {"\x1b[?1$p",  IL("\x1b[?1$p"),  OK, 6, '?', 'p', 1, {1}, 0, "$"},
    {"\x1b[38:2::255:0:0m", IL("\x1b[38:2::255:0:0m"), OK, 16, 0, 'm', 6,
     {38,2,0,255,0,0}, 0x3e},
    {"\x1b[4:3m",  IL("\x1b[4:3m"),  OK, 6, 0,   'm', 2, {4,3}, 0x2},
    {"\x1b[;5H",   IL("\x1b[;5H"),   OK, 5, 0,   'H', 2, {0,5}},
    {"\x1b[5;H",   IL("\x1b[5;H"),   OK, 5, 0,   'H', 2, {5,0}},
    {"\x1b[;m",    IL("\x1b[;m"),    OK, 4, 0,   'm', 2, {0,0}},
    {"\x1b[1 !q",  IL("\x1b[1 !q"),  OK, 6, 0,   'q', 1, {1}, 0, " !"},
    {"\x1b[1 !\"q", IL("\x1b[1 !\"q"), ANSIBAD, 0, 0, 0, 0, {0}},
    {"\x1b[ 1q",   IL("\x1b[ 1q"),   ANSIBAD, 0, 0, 0, 0, {0}},
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
        if (out.subs != tc->subs) {
            fprintf(stderr, "FAIL [%zu]: subs got 0x%x want 0x%x\n",
                    i, out.subs, tc->subs);
            fail(TESTFAIL);
        }
        size_t ni = tc->inter ? strlen(tc->inter) : 0;
        if (out.ninter != ni || memcmp(out.inter, tc->inter ?: "", ni) != 0) {
            fprintf(stderr, "FAIL [%zu]: inter mismatch (%u)\n", i, out.ninter);
            fail(TESTFAIL);
        }
    }
    done;
}

// STD-26B068qhvl: thousands of separators stay bounded at CSI_MAX_PARAMS
ok64 ANSITestCSILong() {
    sane(1);
    static u8 buf[4096];
    for (int k = 0; k < 2; k++) {
        buf[0] = 0x1b;
        buf[1] = '[';
        memset(buf + 2, k ? ':' : ';', sizeof(buf) - 3);
        buf[sizeof(buf) - 1] = 'm';
        u8cs input = {buf, buf + sizeof(buf)};
        csi out = {};
        call(ANSIu8sDrainCSI, input, &out);
        want(input[0] == buf + sizeof(buf));
        want(out.nparams == CSI_MAX_PARAMS && out.final == 'm');
        want(out.subs == (k ? 0xfffe : 0));
    }
    done;
}

ok64 ANSItest() {
    sane(1);
    call(ANSITestCSI);
    call(ANSITestCSILong);
    done;
}

TEST(ANSItest);
