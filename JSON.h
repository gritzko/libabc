#ifndef ABC_JSON_H
#define ABC_JSON_H

#include "INT.h"
#include "UTF8.h"
#include "HEX.h"

con ok64 JSONBAD = 0x137185cb28d;
con ok64 JSONFULL = 0x4dc6173de555;
con ok64 JSONNOROOM = 0x4dc6175d86d8616;
con ok64 JSONSYNTAX = 0x4dc6177225dd2a1;

// Container nesting cap: JSONFmt, a `JSON_MAX_NEST + 1` byte parse stack
#define JSON_MAX_NEST 255

// SAX event handler type: receives token slice and user context
typedef ok64 (*JSONsax_fn)(u8cs tok, void *ctx);

// Parse state for the lexer callbacks
typedef struct {
    u8cs       data;            // source JSON text ('data' for lex compat)
    void      *ctx;             // user context passed to handlers
    JSONsax_fn on_string;
    JSONsax_fn on_number;
    JSONsax_fn on_literal;
    JSONsax_fn on_open_object;
    JSONsax_fn on_close_object;
    JSONsax_fn on_open_array;
    JSONsax_fn on_close_array;
    JSONsax_fn on_colon;
    JSONsax_fn on_comma;
} JSONstate;

// Lexer (generated from JSON.c.rl)
ok64 JSONLexer(JSONstate *state);

// RFC 8259 pull parser: one token per call, no callbacks (JSON-26B065OmZ3).
// `*event` is { } [ ] " (string value) k (key) 1 (number) t f n; `token`
// is its slice in `input` as lexed (strings with quotes, escaped).
// `stack`, empty at start, holds all the state; N bytes nest N-1 deep.
ok64 JSONutf8Parse(u8p event, utf8csp token, utf8cs input, u8bp stack);

// Escape functions
ok64 JSONEscape(u8s txt, u8cs val);
ok64 JSONUnEscape(u8s tlv, u8cs txt);
ok64 JSONEscapeAll(u8s into, u8cs from);
ok64 JSONUnEscapeAll(u8s tlv, u8cs txt);

// JSON formatter context (SAX consumer that writes formatted JSON)
typedef struct {
    u8s  out;      // output buffer
    u8cs indent;   // indent per level ("" for compact)
    u8   depth;    // current nesting depth
    b8   need_nl;  // pending newline before next content
} JSONfmt;

// Wire formatter callbacks into a JSONstate
ok64 JSONfmtInit(JSONstate *state, JSONfmt *fmt);

// Convenience: validate (JSONutf8Parse), lex JSON, format into output buffer
ok64 JSONFmt(u8s out, u8cs json, u8cs indent);

#endif
