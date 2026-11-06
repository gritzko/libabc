#include "JSON.h"

#include "OK.h"
#include "PRO.h"

// --- SAX dispatcher callbacks ---

ok64 JSONonString(u8cs tok, JSONstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->on_string) return state->on_string(tok, state->ctx);
    done;
}

ok64 JSONonNumber(u8cs tok, JSONstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->on_number) return state->on_number(tok, state->ctx);
    done;
}

ok64 JSONonLiteral(u8cs tok, JSONstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->on_literal) return state->on_literal(tok, state->ctx);
    done;
}

ok64 JSONonOpenObject(u8cs tok, JSONstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->on_open_object) return state->on_open_object(tok, state->ctx);
    done;
}

ok64 JSONonCloseObject(u8cs tok, JSONstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->on_close_object)
        return state->on_close_object(tok, state->ctx);
    done;
}

ok64 JSONonOpenArray(u8cs tok, JSONstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->on_open_array) return state->on_open_array(tok, state->ctx);
    done;
}

ok64 JSONonCloseArray(u8cs tok, JSONstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->on_close_array) return state->on_close_array(tok, state->ctx);
    done;
}

ok64 JSONonColon(u8cs tok, JSONstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->on_colon) return state->on_colon(tok, state->ctx);
    done;
}

ok64 JSONonComma(u8cs tok, JSONstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->on_comma) return state->on_comma(tok, state->ctx);
    done;
}

ok64 JSONonJSON(u8cs tok, JSONstate *state) { return OK; }

ok64 JSONonRoot(u8cs tok, JSONstate *state) { return OK; }

// --- Formatter callbacks ---

static ok64 JSONfmtNL(JSONfmt *f) {
    sane(f != NULL);
    if ($empty(f->indent)) done;
    call(u8sFeed1, f->out, '\n');
    for (u8 i = 0; i < f->depth; ++i) {
        call(u8sFeed, f->out, f->indent);
    }
    done;
}

static ok64 JSONfmtPre(JSONfmt *f) {
    sane(f != NULL);
    if (f->need_nl) {
        call(JSONfmtNL, f);
        f->need_nl = NO;
    }
    done;
}

static ok64 JSONfmtString(u8cs tok, void *ctx) {
    sane($ok(tok) && ctx != NULL);
    JSONfmt *f = (JSONfmt *)ctx;
    call(JSONfmtPre, f);
    call(u8sFeed, f->out, tok);
    done;
}

static ok64 JSONfmtNumber(u8cs tok, void *ctx) {
    sane($ok(tok) && ctx != NULL);
    JSONfmt *f = (JSONfmt *)ctx;
    call(JSONfmtPre, f);
    call(u8sFeed, f->out, tok);
    done;
}

static ok64 JSONfmtLiteral(u8cs tok, void *ctx) {
    sane($ok(tok) && ctx != NULL);
    JSONfmt *f = (JSONfmt *)ctx;
    call(JSONfmtPre, f);
    call(u8sFeed, f->out, tok);
    done;
}

static ok64 JSONfmtOpenObject(u8cs tok, void *ctx) {
    sane($ok(tok) && ctx != NULL);
    JSONfmt *f = (JSONfmt *)ctx;
    test(f->depth < JSON_MAX_NEST, JSONFULL);
    call(JSONfmtPre, f);
    call(u8sFeed1, f->out, '{');
    ++f->depth;
    f->need_nl = YES;
    done;
}

static ok64 JSONfmtCloseObject(u8cs tok, void *ctx) {
    sane($ok(tok) && ctx != NULL);
    JSONfmt *f = (JSONfmt *)ctx;
    test(f->depth > 0, JSONBAD);
    --f->depth;
    if (!f->need_nl) {
        call(JSONfmtNL, f);
    }
    f->need_nl = NO;
    call(u8sFeed1, f->out, '}');
    done;
}

static ok64 JSONfmtOpenArray(u8cs tok, void *ctx) {
    sane($ok(tok) && ctx != NULL);
    JSONfmt *f = (JSONfmt *)ctx;
    test(f->depth < JSON_MAX_NEST, JSONFULL);
    call(JSONfmtPre, f);
    call(u8sFeed1, f->out, '[');
    ++f->depth;
    f->need_nl = YES;
    done;
}

static ok64 JSONfmtCloseArray(u8cs tok, void *ctx) {
    sane($ok(tok) && ctx != NULL);
    JSONfmt *f = (JSONfmt *)ctx;
    test(f->depth > 0, JSONBAD);
    --f->depth;
    if (!f->need_nl) {
        call(JSONfmtNL, f);
    }
    f->need_nl = NO;
    call(u8sFeed1, f->out, ']');
    done;
}

static ok64 JSONfmtColon(u8cs tok, void *ctx) {
    sane($ok(tok) && ctx != NULL);
    JSONfmt *f = (JSONfmt *)ctx;
    call(u8sFeed1, f->out, ':');
    if (!$empty(f->indent)) {
        call(u8sFeed1, f->out, ' ');
    }
    done;
}

static ok64 JSONfmtComma(u8cs tok, void *ctx) {
    sane($ok(tok) && ctx != NULL);
    JSONfmt *f = (JSONfmt *)ctx;
    call(u8sFeed1, f->out, ',');
    f->need_nl = YES;
    done;
}

ok64 JSONfmtInit(JSONstate *state, JSONfmt *fmt) {
    sane(state != NULL && fmt != NULL);
    state->ctx = fmt;
    state->on_string = JSONfmtString;
    state->on_number = JSONfmtNumber;
    state->on_literal = JSONfmtLiteral;
    state->on_open_object = JSONfmtOpenObject;
    state->on_close_object = JSONfmtCloseObject;
    state->on_open_array = JSONfmtOpenArray;
    state->on_close_array = JSONfmtCloseArray;
    state->on_colon = JSONfmtColon;
    state->on_comma = JSONfmtComma;
    done;
}

ok64 JSONFmt(u8s out, u8cs json, u8cs indent) {
    sane(u8sOK(out) && $ok(json));
    JSONfmt fmt = {.out = {out[0], out[1]}, .indent = {indent[0], indent[1]}};
    JSONstate state = {.data = {json[0], json[1]}};
    call(JSONfmtInit, &state, &fmt);
    call(JSONLexer, &state);
    out[0] = fmt.out[0];
    done;
}

// --- Escape functions ---

ok64 JSONEscape(u8s txt, u8cs val) {
    sane(u8sOK(txt) && u8csOK(val));
    switch (**val) {
        case '\t':
            call(u8sFeed2, txt, '\\', 't');
            break;
        case '\r':
            call(u8sFeed2, txt, '\\', 'r');
            break;
        case '\n':
            call(u8sFeed2, txt, '\\', 'n');
            break;
        case '\b':
            call(u8sFeed2, txt, '\\', 'b');
            break;
        case '\f':
            call(u8sFeed2, txt, '\\', 'f');
            break;
        case '\\':
            call(u8sFeed2, txt, '\\', '\\');
            break;
        case '"':
            call(u8sFeed2, txt, '\\', '"');
            break;
        default:
            if (**val < 0x20) {
                static const char hex[] = "0123456789abcdef";
                u8 c = **val;
                call(u8sFeed2, txt, '\\', 'u');
                call(u8sFeed2, txt, '0', '0');
                call(u8sFeed2, txt, hex[c >> 4], hex[c & 0xf]);
            } else {
                call(u8sFeed1, txt, **val);
            }
    }
    ++*val;
    done;
}

// One \uXXXX quad; JSONBAD unless the 4 bytes are hex
fun ok64 JSONu16Drain(u64 *cp, u8cs txt) {
    if ($len(txt) < 6 || txt[0][0] != '\\' || txt[0][1] != 'u')
        return JSONBAD;
    u8cs hex = {*txt + 2, *txt + 6};
    if (u64hexdrain(cp, hex) != OK) return JSONBAD;
    *txt += 6;
    return OK;
}

// RFC 8259 §7: a surrogate pair is one 12-byte escape of one code point;
// lone or mis-ordered surrogates are refused (JSON-26B065isnm)
ok64 JSONUnEscape(u8s tlv, u8cs txt) {
    if ($empty(tlv)) return JSONNOROOM;
    if ($empty(txt)) return JSONBAD;
    if (**txt != '\\') {
        **tlv = **txt;
        ++*tlv;
        ++*txt;
        return OK;
    }
    if ($len(txt) < 2) return JSONBAD;
    u8 c = 0;
    switch ((*txt)[1]) {
        case 't': c = '\t'; break;
        case 'r': c = '\r'; break;
        case 'n': c = '\n'; break;
        case 'b': c = '\b'; break;
        case 'f': c = '\f'; break;
        case '\\': c = '\\'; break;
        case '/': c = '/'; break;
        case '"': c = '"'; break;
        case 'u': {
            u8cs t = {txt[0], txt[1]};
            u64 cp = 0, lo = 0;
            if (JSONu16Drain(&cp, t) != OK) return JSONBAD;
            if (cp >= 0xdc00 && cp <= 0xdfff) return JSONBAD;
            if (cp >= 0xd800 && cp <= 0xdbff) {
                if (JSONu16Drain(&lo, t) != OK) return JSONBAD;
                if (lo < 0xdc00 || lo > 0xdfff) return JSONBAD;
                cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
            }
            ok64 o = utf8sFeed32(tlv, cp);
            if (o != OK) return o;
            *txt = t[0];
            return OK;
        }
        default:
            return JSONBAD;
    }
    **tlv = c;
    ++*tlv;
    *txt += 2;
    return OK;
}

ok64 JSONEscapeAll(u8s into, u8cs from) {
    sane(u8sOK(into) && u8csOK(from));
    while (!u8csEmpty(from)) {
        call(JSONEscape, into, from);
    }
    done;
}

ok64 JSONUnEscapeAll(u8s tlv, u8cs txt) {
    sane(u8sOK(tlv) && u8csOK(txt));
    while (!$empty(txt) && !$empty(tlv)) {
        if (**txt != '\\') {
            **tlv = **txt;
            ++*tlv;
            ++*txt;
            continue;
        }
        ok64 o = JSONUnEscape(tlv, txt);
        if (o != OK) return o;
    }
    if (!$empty(txt)) return JSONNOROOM;
    return OK;
}
