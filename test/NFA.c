//
// NFA tests — Thompson NFA regex matching
//

#include "NFA.h"
#include "PRO.h"
#include "TEST.h"

// Helper: compile + match (anchored)
ok64 nfa_match(u8cs pat, u8cs text, b8 *result) {
    sane(1);
    nfau8 buf[256];
    nfau8g prog = {buf, buf + 256, buf};
    u32 pbuf[512];
    u32 *pws[2] = {pbuf, pbuf + 512};

    ok64 o = NFAu8Compile(prog, pat, pws);
    if (o != OK) return o;

    nfau8cs nfa = {prog[2], prog[0]};
    u32 wbuf[768];
    u32 *ws[2] = {wbuf, wbuf + 768};
    *result = NFAu8Match(nfa, text, ws);
    done;
}

// Helper: compile + search (unanchored)
ok64 nfa_search(u8cs pat, u8cs text, b8 *result) {
    sane(1);
    nfau8 buf[256];
    nfau8g prog = {buf, buf + 256, buf};
    u32 pbuf[512];
    u32 *pws[2] = {pbuf, pbuf + 512};

    ok64 o = NFAu8Compile(prog, pat, pws);
    if (o != OK) return o;

    nfau8cs nfa = {prog[2], prog[0]};
    u32 wbuf[768];
    u32 *ws[2] = {wbuf, wbuf + 768};
    *result = NFAu8Search(nfa, text, ws);
    done;
}

// --- Literals ---

ok64 test_literal_match() {
    sane(1);
    b8 m;
    u8cs pat = {(u8 *)"abc", (u8 *)"abc" + 3};
    u8cs yes = {(u8 *)"abc", (u8 *)"abc" + 3};
    u8cs no = {(u8 *)"abd", (u8 *)"abd" + 3};
    u8cs short_ = {(u8 *)"ab", (u8 *)"ab" + 2};

    call(nfa_match, pat, yes, &m);
    want(m == YES);
    call(nfa_match, pat, no, &m);
    want(m == NO);
    call(nfa_match, pat, short_, &m);
    want(m == NO);
    done;
}

// --- Dot (any) ---

ok64 test_dot() {
    sane(1);
    b8 m;
    u8cs pat = {(u8 *)"a.c", (u8 *)"a.c" + 3};
    u8cs yes = {(u8 *)"abc", (u8 *)"abc" + 3};
    u8cs yes2 = {(u8 *)"axc", (u8 *)"axc" + 3};
    u8cs no = {(u8 *)"ac", (u8 *)"ac" + 2};

    call(nfa_match, pat, yes, &m);
    want(m == YES);
    call(nfa_match, pat, yes2, &m);
    want(m == YES);
    call(nfa_match, pat, no, &m);
    want(m == NO);
    done;
}

// --- Star ---

ok64 test_star() {
    sane(1);
    b8 m;
    u8cs pat = {(u8 *)"ab*c", (u8 *)"ab*c" + 4};
    u8cs t0 = {(u8 *)"ac", (u8 *)"ac" + 2};
    u8cs t1 = {(u8 *)"abc", (u8 *)"abc" + 3};
    u8cs t3 = {(u8 *)"abbbc", (u8 *)"abbbc" + 5};
    u8cs no = {(u8 *)"adc", (u8 *)"adc" + 3};

    call(nfa_match, pat, t0, &m);
    want(m == YES);
    call(nfa_match, pat, t1, &m);
    want(m == YES);
    call(nfa_match, pat, t3, &m);
    want(m == YES);
    call(nfa_match, pat, no, &m);
    want(m == NO);
    done;
}

// --- Plus ---

ok64 test_plus() {
    sane(1);
    b8 m;
    u8cs pat = {(u8 *)"ab+c", (u8 *)"ab+c" + 4};
    u8cs t0 = {(u8 *)"ac", (u8 *)"ac" + 2};
    u8cs t1 = {(u8 *)"abc", (u8 *)"abc" + 3};
    u8cs t3 = {(u8 *)"abbbc", (u8 *)"abbbc" + 5};

    call(nfa_match, pat, t0, &m);
    want(m == NO);
    call(nfa_match, pat, t1, &m);
    want(m == YES);
    call(nfa_match, pat, t3, &m);
    want(m == YES);
    done;
}

// --- Question ---

ok64 test_question() {
    sane(1);
    b8 m;
    u8cs pat = {(u8 *)"ab?c", (u8 *)"ab?c" + 4};
    u8cs t0 = {(u8 *)"ac", (u8 *)"ac" + 2};
    u8cs t1 = {(u8 *)"abc", (u8 *)"abc" + 3};
    u8cs t2 = {(u8 *)"abbc", (u8 *)"abbc" + 4};

    call(nfa_match, pat, t0, &m);
    want(m == YES);
    call(nfa_match, pat, t1, &m);
    want(m == YES);
    call(nfa_match, pat, t2, &m);
    want(m == NO);
    done;
}

// --- Alternation ---

ok64 test_alt() {
    sane(1);
    b8 m;
    u8cs pat = {(u8 *)"cat|dog", (u8 *)"cat|dog" + 7};
    u8cs t1 = {(u8 *)"cat", (u8 *)"cat" + 3};
    u8cs t2 = {(u8 *)"dog", (u8 *)"dog" + 3};
    u8cs t3 = {(u8 *)"bat", (u8 *)"bat" + 3};

    call(nfa_match, pat, t1, &m);
    want(m == YES);
    call(nfa_match, pat, t2, &m);
    want(m == YES);
    call(nfa_match, pat, t3, &m);
    want(m == NO);
    done;
}

// --- Parenthesized groups ---

ok64 test_group() {
    sane(1);
    b8 m;
    u8cs pat = {(u8 *)"(ab)+", (u8 *)"(ab)+" + 5};
    u8cs t1 = {(u8 *)"ab", (u8 *)"ab" + 2};
    u8cs t2 = {(u8 *)"abab", (u8 *)"abab" + 4};
    u8cs t3 = {(u8 *)"ababab", (u8 *)"ababab" + 6};
    u8cs t0 = {(u8 *)"", (u8 *)""};
    u8cs no = {(u8 *)"abc", (u8 *)"abc" + 3};

    call(nfa_match, pat, t1, &m);
    want(m == YES);
    call(nfa_match, pat, t2, &m);
    want(m == YES);
    call(nfa_match, pat, t3, &m);
    want(m == YES);
    call(nfa_match, pat, t0, &m);
    want(m == NO);
    call(nfa_match, pat, no, &m);
    want(m == NO);
    done;
}

// --- Escape ---

ok64 test_escape() {
    sane(1);
    b8 m;
    u8cs pat = {(u8 *)"a\\.b", (u8 *)"a\\.b" + 4};
    u8cs yes = {(u8 *)"a.b", (u8 *)"a.b" + 3};
    u8cs no = {(u8 *)"axb", (u8 *)"axb" + 3};

    call(nfa_match, pat, yes, &m);
    want(m == YES);
    call(nfa_match, pat, no, &m);
    want(m == NO);
    done;
}

// --- STD-26B068DyLt: C escapes decode the same outside and inside [] ---

ok64 test_ctrl_escapes() {
    sane(1);
    b8 m;
    // pattern, text, expected
    char const *rows[][3] = {
        {"\\n", "\n", "y"},   {"\\n", "n", ""},     {"[\\n]", "\n", "y"},
        {"\\t", "\t", "y"},   {"\\t", "t", ""},     {"\\r", "\r", "y"},
        {"\\f", "\f", "y"},   {"\\v", "\v", "y"},   {"[\\v]", "\v", "y"},
        {"[\\v]", "v", ""},   {"\\e", "\x1b", "y"}, {"\\x41", "A", "y"},
        {"\\x41", "x41", ""}, {"[\\x41]", "A", "y"}, {"\\x4a", "J", "y"},
        {"\\x4", "\x04", "y"}, {"\\101", "A", "y"}, {"[\\x00-\\x1f]", "\x1b", "y"},
        {"a\\.b", "a.b", "y"},
    };
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        u8cs pat = {(u8c *)rows[i][0], (u8c *)rows[i][0] + strlen(rows[i][0])};
        u8cs txt = {(u8c *)rows[i][1], (u8c *)rows[i][1] + strlen(rows[i][1])};
        call(nfa_match, pat, txt, &m);
        want(m == (rows[i][2][0] == 'y'));
    }
    done;
}

// --- Unanchored search ---

ok64 test_search() {
    sane(1);
    b8 m;
    u8cs pat = {(u8 *)"fox", (u8 *)"fox" + 3};
    u8cs hay = {(u8 *)"the quick brown fox jumps",
                (u8 *)"the quick brown fox jumps" + 25};
    u8cs no = {(u8 *)"the quick brown dog", (u8 *)"the quick brown dog" + 19};

    call(nfa_search, pat, hay, &m);
    want(m == YES);
    call(nfa_search, pat, no, &m);
    want(m == NO);
    done;
}

ok64 test_search_pattern() {
    sane(1);
    b8 m;
    u8cs pat = {(u8 *)"a+b", (u8 *)"a+b" + 3};
    u8cs yes = {(u8 *)"xxxaaabyyy", (u8 *)"xxxaaabyyy" + 10};
    u8cs no = {(u8 *)"xxxbyyy", (u8 *)"xxxbyyy" + 7};

    call(nfa_search, pat, yes, &m);
    want(m == YES);
    call(nfa_search, pat, no, &m);
    want(m == NO);
    done;
}

// --- Cox's pathological case: a?^n a^n vs a^n ---

ok64 test_pathological() {
    sane(1);
    // a?{20}a{20} vs a{20}
    // Backtracking: 2^20 steps. Thompson: O(20*20).
    u8 patbuf[64], textbuf[32];
    u16 pi = 0, ti = 0;
    for (int i = 0; i < 20; i++) {
        patbuf[pi++] = 'a';
        patbuf[pi++] = '?';
    }
    for (int i = 0; i < 20; i++) {
        patbuf[pi++] = 'a';
        textbuf[ti++] = 'a';
    }
    u8cs pat = {patbuf, patbuf + pi};
    u8cs text = {textbuf, textbuf + ti};

    b8 m;
    call(nfa_match, pat, text, &m);
    want(m == YES);

    // should NOT match a shorter string
    u8cs short_ = {textbuf, textbuf + 10};
    call(nfa_match, pat, short_, &m);
    want(m == NO);
    done;
}

// --- Compile errors ---

ok64 test_bad_patterns() {
    sane(1);
    nfau8 buf[64];
    u32 pbuf[128];
    u32 *pws[2] = {pbuf, pbuf + 128};

    // unclosed paren
    nfau8g p1 = {buf, buf + 64, buf};
    u8cs bad1 = {(u8 *)"(abc", (u8 *)"(abc" + 4};
    want(NFAu8Compile(p1, bad1, pws) != OK);

    // empty pattern
    nfau8g p2 = {buf, buf + 64, buf};
    u8cs bad2 = {(u8 *)"", (u8 *)""};
    want(NFAu8Compile(p2, bad2, pws) != OK);

    // leading quantifier
    nfau8g p3 = {buf, buf + 64, buf};
    u8cs bad3 = {(u8 *)"*abc", (u8 *)"*abc" + 4};
    want(NFAu8Compile(p3, bad3, pws) != OK);

    done;
}

// --- Oversized programs: refuse or compile right, never miswire ---

// Compile + match with mapped buffers sized for the 64K state cap
ok64 nfa_big(u8cs pat, u8cs text, b8 *result) {
    sane(1);
    Bu8 pb = {}, wb = {};
    Bu32 patch = {};
    call(u8bMap, pb, 0x10000 * sizeof(nfau8));
    call(u32bMap, patch, 1 << 20);
    call(u8bMap, wb, 3 * 0x10000 * sizeof(u32));
    nfau8 *buf = (nfau8 *)pb[0];
    nfau8g prog = {buf, buf + 0xFFFF, buf};
    u32 *pws[2] = {patch[0], patch[0] + (1 << 20)};
    ok64 o = NFAu8Compile(prog, pat, pws);
    if (o == OK) {
        nfau8cs nfa = {prog[2], prog[0]};
        u32 *ws[2] = {(u32 *)wb[0], (u32 *)wb[0] + 3 * 0x10000};
        *result = NFAu8Match(nfa, text, ws);
    }
    u8bUnMap(pb);
    u32bUnMap(patch);
    u8bUnMap(wb);
    return o;
}

ok64 test_big_counts() {
    sane(1);
    b8 m = NO;
    // STD-26B068h31R: a u16 count used to wrap, a{65536} == a{0}
    char const *bad[] = {"a{65536}", "a{65537}", "a{70000}", "a{1,65535}",
                         "a{,65536}", "a{99999999999}"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        u8cs pat = {(u8c *)bad[i], (u8c *)bad[i] + strlen(bad[i])};
        u8cs txt = {(u8c *)"a", (u8c *)"a" + 1};
        want(nfa_big(pat, txt, &m) == NFABADSYN);
    }
    // STD-26B068h31R: {n,} stays unbounded, {n,m} bounded
    u8cs p1 = $u8str("a{2,}");
    u8cs t1 = $u8str("aaaaaaa");
    call(nfa_big, p1, t1, &m);
    want(m == YES);
    u8cs p2 = $u8str("a{2,3}");
    call(nfa_big, p2, t1, &m);
    want(m == NO);
    // STD-26B0680CDX: >64K patch entries used to wrap the u16 offsets
    u8cs p3 = $u8str("(a?){16400}");
    u8cs t3 = $u8str("aaa");
    m = NO;
    call(nfa_big, p3, t3, &m);
    want(m == YES);
    done;
}

// --- Dotstar ---

ok64 test_dotstar() {
    sane(1);
    b8 m;
    u8cs pat = {(u8 *)"a.*b", (u8 *)"a.*b" + 4};
    u8cs t1 = {(u8 *)"ab", (u8 *)"ab" + 2};
    u8cs t2 = {(u8 *)"aXXXb", (u8 *)"aXXXb" + 5};
    u8cs no = {(u8 *)"aXXX", (u8 *)"aXXX" + 4};

    call(nfa_match, pat, t1, &m);
    want(m == YES);
    call(nfa_match, pat, t2, &m);
    want(m == YES);
    call(nfa_match, pat, no, &m);
    want(m == NO);
    done;
}

ok64 all_tests() {
    sane(1);
    call(test_literal_match);
    call(test_dot);
    call(test_star);
    call(test_plus);
    call(test_question);
    call(test_alt);
    call(test_group);
    call(test_escape);
    call(test_ctrl_escapes);
    call(test_big_counts);
    call(test_search);
    call(test_search_pattern);
    call(test_pathological);
    call(test_bad_patterns);
    call(test_dotstar);
    done;
}

TEST(all_tests)
