// JSON-26B065OmZ3: differential fuzz of JSONutf8Parse against yyjson
// (strict RFC 8259 by default, 100% JSONTestSuite, raw numbers on request).
// Both render one canonical event string: the event byte, then unescaped
// key/string bytes or the raw number text, then 0xFF (never in UTF-8).
// Oracles: same accept/reject, same strings; JSONLexer accepts a superset.
// Excluded: nesting past JSON_MAX_NEST; a lone-surrogate escape (i_) only
// checks that yyjson refuses too, as JSONUnEscapeAll does.
#include <yyjson.h>

#include "JSON.h"
#include "PRO.h"
#include "TEST.h"

con ok64 JSONFZSKIP = 0x4dc6173e3714499;

static ok64 JSONfzOurs(u8s out, u8cs json) {
    aBpad(u8, stack, JSON_MAX_NEST + 1);
    u8 e = 0;
    u8cs tok = {};
    ok64 o = OK;
    while ((o = JSONutf8Parse(&e, tok, json, stack)) == OK) {
        if (u8sFeed1(out, e) != OK) return NOROOM;
        if (e == '1' && u8sFeed(out, tok) != OK) return NOROOM;
        if (e == '"' || e == 'k') {
            u8cs in = {tok[0] + 1, tok[1] - 1};
            if (JSONUnEscapeAll(out, in) != OK) return JSONFZSKIP;
        }
        if ((e == '1' || e == '"' || e == 'k') && u8sFeed1(out, 0xff) != OK)
            return NOROOM;
    }
    return o == NODATA ? OK : o;
}

static ok64 JSONfzBytes(u8s out, u8 e, char const *s, size_t len) {
    u8cs b = {(u8c *)s, (u8c *)s + len};
    if (u8sFeed1(out, e) != OK || u8sFeed(out, b) != OK) return NOROOM;
    return u8sFeed1(out, 0xff);
}

static ok64 JSONfzRef(u8s out, yyjson_val *v, int depth) {
    ok64 o = OK;
    if (yyjson_is_ctn(v) && depth > JSON_MAX_NEST) return JSONFZSKIP;
    switch (yyjson_get_type(v)) {
        case YYJSON_TYPE_OBJ: {
            if (u8sFeed1(out, '{') != OK) return NOROOM;
            yyjson_obj_iter it = yyjson_obj_iter_with(v);
            yyjson_val *k = NULL;
            while (o == OK && (k = yyjson_obj_iter_next(&it)) != NULL) {
                o = JSONfzBytes(out, 'k', yyjson_get_str(k), yyjson_get_len(k));
                if (o == OK)
                    o = JSONfzRef(out, yyjson_obj_iter_get_val(k), depth + 1);
            }
            return o == OK ? u8sFeed1(out, '}') : o;
        }
        case YYJSON_TYPE_ARR: {
            if (u8sFeed1(out, '[') != OK) return NOROOM;
            yyjson_arr_iter it = yyjson_arr_iter_with(v);
            yyjson_val *x = NULL;
            while (o == OK && (x = yyjson_arr_iter_next(&it)) != NULL)
                o = JSONfzRef(out, x, depth + 1);
            return o == OK ? u8sFeed1(out, ']') : o;
        }
        case YYJSON_TYPE_STR:
            return JSONfzBytes(out, '"', yyjson_get_str(v), yyjson_get_len(v));
        case YYJSON_TYPE_RAW:
            return JSONfzBytes(out, '1', yyjson_get_raw(v), yyjson_get_len(v));
        case YYJSON_TYPE_BOOL:
            return u8sFeed1(out, yyjson_get_bool(v) ? 't' : 'f');
        case YYJSON_TYPE_NULL:
            return u8sFeed1(out, 'n');
    }
    return FAILSANITY;
}

FUZZ(u8, JSONfuzz) {
    sane(1);
    if ($len(input) > 1 << 16) done;
    JSONstate lex = {.data = {input[0], input[1]}};
    ok64 lo = JSONLexer(&lex);
    test(lo == OK || lo == JSONBAD, FAILSANITY);

    size_t room = 8 * $len(input) + 64;
    a_carve(u8, ours, room);
    a_carve(u8, theirs, room);
    a_dup(u8c, json, input);
    ok64 oo = JSONfzOurs(u8bIdle(ours), json);
    test(oo == OK || oo == JSONFZSKIP || oo == JSONSYNTAX || oo == JSONBAD,
         FAILSANITY);
    test(oo != OK || lo == OK, FAILSANITY);  // the lexer takes a superset
    test(oo == OK || (json[0] >= input[0] && json[0] <= input[1]), FAILSANITY);

    yyjson_read_err err = {};
    yyjson_doc *doc = yyjson_read_opts((char *)input[0], $len(input),
                                       YYJSON_READ_NUMBER_AS_RAW, NULL, &err);
    ok64 ro = doc ? JSONfzRef(u8bIdle(theirs), yyjson_doc_get_root(doc), 1)
                  : JSONBAD;
    yyjson_doc_free(doc);
    if (ro == JSONFZSKIP) done;
    if (oo == JSONFZSKIP) {  // a lone surrogate escape: both must refuse
        test(ro != OK, FAILEQ);
        done;
    }
    b8 agree = (oo == OK) == (ro == OK);
    if (agree && oo == OK) {
        u8cs a = {ours[1], ours[2]};
        u8cs b = {theirs[1], theirs[2]};
        agree = u8csEq(a, b);
    }
    if (!agree)
        fprintf(stderr, "JSONfuzz: ours %s, yyjson %s (%s at %zu)\n",
                ok64str(oo), ok64str(ro), doc ? "ok" : err.msg, err.pos);
    test(agree, FAILEQ);
    done;
}
