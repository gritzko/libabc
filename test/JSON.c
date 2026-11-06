#include "JSON.h"

#include "B.h"
#include "TEST.h"

typedef struct {
    const char *input;
    const char *expected;
} JSONCase;

// Compact roundtrip: parse → format with no indent → compare
ok64 JSONCompactTest() {
    sane(1);

    JSONCase cases[] = {
        {"42", "42"},
        {"\"hello\"", "\"hello\""},
        {"true", "true"},
        {"false", "false"},
        {"null", "null"},
        {"{}", "{}"},
        {"[]", "[]"},
        {"[1,2,3]", "[1,2,3]"},
        {"{\"a\":1}", "{\"a\":1}"},
        {"{\"a\":1,\"b\":2}", "{\"a\":1,\"b\":2}"},
        {"{\"a\":[1,2]}", "{\"a\":[1,2]}"},
        {"[{\"x\":1},{\"y\":2}]", "[{\"x\":1},{\"y\":2}]"},
        {"{\"a\":{\"b\":{\"c\":3}}}", "{\"a\":{\"b\":{\"c\":3}}}"},
        {"\"hello\\nworld\"", "\"hello\\nworld\""},
        {"\"tab\\there\"", "\"tab\\there\""},
        {"\"quote\\\"here\"", "\"quote\\\"here\""},
        {" { \"a\" : 1 , \"b\" : 2 } ", "{\"a\":1,\"b\":2}"},
        {"[true,false,null]", "[true,false,null]"},
        {"-42", "-42"},
        {"3.14", "3.14"},
        {"1e10", "1e10"},
        {"\"\\\\\"", "\"\\\\\""},
        {"\"\"", "\"\""},
        {"[[[]]]", "[[[]]]"},
        {"{\"a\":[{\"b\":1},2]}", "{\"a\":[{\"b\":1},2]}"},
        {" [ 1 , 2 ] ", "[1,2]"},
        {"\t{\n\"k\"\t:\r1\n}", "{\"k\":1}"},
        {"{\"a\":[]}", "{\"a\":[]}"},
        {"-0", "-0"},
        {"0.5", "0.5"},
        {"-1.23e+4", "-1.23e+4"},
        {"1E10", "1E10"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    u8cs no_indent = {(u8c *)"", (u8c *)""};

    for (int i = 0; i < ncases; ++i) {
        a_pad(u8, out, 1 << 16);
        u8cs json = u8scstr(cases[i].input);
        call(JSONFmt, out_idle, json, no_indent);

        u8cs result = {out[1], out_idle[0]};
        u8cs expect = u8scstr(cases[i].expected);

        if (!u8csEq(result, expect)) {
            fprintf(stderr, "FAIL case %d: '%s'\n", i, cases[i].input);
            fprintf(stderr, "  expected: '%.*s'\n", (int)u8csLen(expect),
                    expect[0]);
            fprintf(stderr, "  got:      '%.*s'\n", (int)u8csLen(result),
                    result[0]);
            fail(TESTFAIL);
        }
    }
    done;
}

// Pretty-print: parse → format with indent → compare
ok64 JSONPrettyTest() {
    sane(1);

    JSONCase cases[] = {
        {"42", "42"},
        {"\"hi\"", "\"hi\""},
        {"true", "true"},
        {"{}", "{}"},
        {"[]", "[]"},
        {"{\"a\":1}", "{\n  \"a\": 1\n}"},
        {"[1,2]", "[\n  1,\n  2\n]"},
        {"{\"a\":1,\"b\":2}", "{\n  \"a\": 1,\n  \"b\": 2\n}"},
        {"{\"a\":{\"b\":1}}", "{\n  \"a\": {\n    \"b\": 1\n  }\n}"},
        {"[1,[2,3]]", "[\n  1,\n  [\n    2,\n    3\n  ]\n]"},
        {"{\"a\":[]}", "{\n  \"a\": []\n}"},
        {"{\"a\":{}}", "{\n  \"a\": {}\n}"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    u8c indent_str[] = "  ";
    u8cs indent = {indent_str, indent_str + 2};

    for (int i = 0; i < ncases; ++i) {
        a_pad(u8, out, 1 << 16);
        u8cs json = u8scstr(cases[i].input);
        call(JSONFmt, out_idle, json, indent);

        u8cs result = {out[1], out_idle[0]};
        u8cs expect = u8scstr(cases[i].expected);

        if (!u8csEq(result, expect)) {
            fprintf(stderr, "FAIL pretty case %d: '%s'\n", i, cases[i].input);
            fprintf(stderr, "  expected: '%.*s'\n", (int)u8csLen(expect),
                    expect[0]);
            fprintf(stderr, "  got:      '%.*s'\n", (int)u8csLen(result),
                    result[0]);
            fail(TESTFAIL);
        }
    }
    done;
}

ok64 JSONEscapeTest() {
    sane(1);

    typedef struct {
        const char *unescaped;
        const char *escaped;
    } EscCase;

    EscCase cases[] = {
        {"hello", "hello"},
        {"a\tb", "a\\tb"},
        {"a\nb", "a\\nb"},
        {"a\"b", "a\\\"b"},
        {"a\\b", "a\\\\b"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);

    for (int i = 0; i < ncases; ++i) {
        a_pad(u8, buf, 1024);
        u8cs from = u8scstr(cases[i].unescaped);
        call(JSONEscapeAll, buf_idle, from);
        u8cs result = {buf[1], buf_idle[0]};
        u8cs expect = u8scstr(cases[i].escaped);
        want(u8csEq(result, expect));
    }
    done;
}

// JSON-26B065PDML: JSONfmt nests up to JSON_MAX_NEST, refuses an unopened close
ok64 JSONNestTest() {
    sane(1);
    u8 deep[2 * JSON_MAX_NEST + 2];
    for (int i = 0; i <= JSON_MAX_NEST; ++i) {
        deep[i] = '[';
        deep[2 * JSON_MAX_NEST + 1 - i] = ']';
    }
    u8c ind[] = " ";
    u8cs indent = {ind, ind + 1};
    a_pad(u8, out, 1 << 18);
    u8cs ok = {deep + 1, deep + 2 * JSON_MAX_NEST + 1};
    call(JSONFmt, out_idle, ok, indent);
    a_pad(u8, out2, 1 << 18);
    u8cs over = {deep, deep + 2 * JSON_MAX_NEST + 2};
    want(JSONFULL == JSONFmt(out2_idle, over, indent));
    u8cs close = u8scstr("]");
    want(JSONBAD == JSONFmt(out_idle, close, indent));
    done;
}

// JSON-26B065isnm: surrogate pairs decode to one 4-byte code point,
// lone or mis-ordered surrogates and the non-JSON `\0` are refused
ok64 JSONUnEscapeTest() {
    sane(1);
    typedef struct {
        const char *escaped;
        const char *unescaped;
        ok64 ret;
    } UnEscCase;
    UnEscCase cases[] = {
        {"a\\tb", "a\tb", OK},
        {"\\u00e9", "\xc3\xa9", OK},
        {"\\u0000", "", OK},
        {"\\uD83D\\uDE00", "\xf0\x9f\x98\x80", OK},
        {"x\\udbff\\udfffy", "x\xf4\x8f\xbf\xbfy", OK},
        {"\\uD83D", "", JSONBAD},
        {"\\uDEAD", "", JSONBAD},
        {"\\uDE00\\uD83D", "", JSONBAD},
        {"\\uD83D\\u0041", "", JSONBAD},
        {"\\uD83D\\uD83D", "", JSONBAD},
        {"\\uD83Dx", "", JSONBAD},
        {"\\uD83D\\uDE0", "", JSONBAD},
        {"\\0", "", JSONBAD},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < ncases; ++i) {
        a_pad(u8, buf, 64);
        u8cs from = u8scstr(cases[i].escaped);
        ok64 o = JSONUnEscapeAll(buf_idle, from);
        want(o == cases[i].ret);
        if (o != OK) continue;
        u8cs result = {buf[1], buf_idle[0]};
        u8cs expect = u8scstr(cases[i].unescaped);
        if (i == 2) expect[1] = expect[0] + 1;  // the NUL byte
        want(u8csEq(result, expect));
    }
    done;
}

ok64 JSONtest() {
    sane(1);
    call(JSONEscapeTest);
    call(JSONUnEscapeTest);
    call(JSONCompactTest);
    call(JSONPrettyTest);
    call(JSONNestTest);
    done;
}

TEST(JSONtest);
