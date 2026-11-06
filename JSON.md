#   JSON rework

Objective: make a "binary JSON" format, nested TLKV (see TLV.h) with
SLOG (see SLOG.md, SLOG.h).

The iterator structure and the API:
````
typedef struct {
    u8bp  buf;
    u64bp stack;

    u8cs key;
    u8cs val;

    u8 lit;  // value type
    u8 plit; // parent O A N S B
} slit;

ok64 JSONNext(slit *it);
ok64 JSONInto(slit *it);
ok64 JSONOuto(slit *it);

ok64 JSONWriteNext(slit *it);
ok64 JSONWriteInto(slit *it);
ok64 JSONWriteOuto(slit *it);
````

The Ragel lexer should use *Write* calls to build a doc in a buffer.
Read should use Next/Into/Outo API. Note that Into() with `key` and
`lit` set should position on an element having >= the provided key
or return NONE.

API entry points:
````
ok64 JSONParse(u8bp buf, u8cs json);
ok64 JSONExport(u8s json, u8bp buf);
````
Keys and values are always text. Strings are unescaped (escaped back
on export). Record types (lits): Object, Array, Number, String,
Boolean, Null.
For array contents, the key is the index in RON base 64 (see RON.h).

##  Pull parser: JSONutf8Parse

`JSONLexer` is a token-level lexer: it accepts any token stream, so `[1,]`, `{}{}` or `]` lex OK. `ok64 JSONutf8Parse(u8p event, utf8csp token, utf8cs input, u8bp stack)` returns one token per call, no callbacks, and enforces RFC 8259 structure (JSON-26B065OmZ3).

 -  `*event`: `{` `}` `[` `]`, `"` string value, `k` object key, `1` number, `t` `f` `n`; `:` and `,` are checked, not reported.
 -  `token` is a zero-copy slice of `input` as lexed: strings keep their quotes and escapes (JSONUnEscape* decodes); `input` advances past it.
 -  `stack` is all the state: start with an empty buffer, e.g. `aBpad(u8, st, JSON_MAX_NEST + 1)`; it keeps a root byte plus one per open container, so N bytes nest N-1 deep.
 -  Returns OK per token, NODATA once the single root value is closed and only whitespace is left, JSONSYNTAX for structure (empty, truncated, second root, too deep), JSONBAD for lexical errors and invalid UTF-8 in strings (JSON-26B065wahm); on error `input` is at the offending byte.
 -  `JSONFmt` validates with it first, so it refuses invalid input and never emits unbalanced output.
 -  fuzz/JSON.c diffs it against yyjson (optional, found by CMake): accept/reject, unescaped strings, raw numbers; JSONLexer must accept a superset.
