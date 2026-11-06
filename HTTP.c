#include "HTTP.h"

ok64 HTTPonMethod(u8cs tok, HTTPstate* state) {
    u8csMv(state->method, tok);
    return OK;
}
ok64 HTTPonRequestURI(u8cs tok, HTTPstate* state) {
    u8csMv(state->uri, tok);
    return OK;
}
ok64 HTTPonHTTPVersion(u8cs tok, HTTPstate* state) {
    u8csMv(state->version, tok);
    return OK;
}
ok64 HTTPonRequestLine(u8cs tok, HTTPstate* state) { return OK; }
ok64 HTTPonFieldName(u8cs tok, HTTPstate* state) {
    return u8cssFeed1(state->headers, tok);
}
// STD-26B068T_oD: RFC 9112 §5 field-value excludes trailing OWS
ok64 HTTPonFieldValue(u8cs tok, HTTPstate* state) {
    while (tok[1] > tok[0] && (tok[1][-1] == ' ' || tok[1][-1] == '\t'))
        --tok[1];
    return u8cssFeed1(state->headers, tok);
}
ok64 HTTPonMessageHeader(u8cs tok, HTTPstate* state) { return OK; }
// STD-26B068gn1w: END stops the lexer at the head's end, so the body
// and any pipelined bytes stay unparsed
ok64 HTTPonRequestHead(u8cs tok, HTTPstate* state) { return END; }
ok64 HTTPonResponseHead(u8cs tok, HTTPstate* state) { return END; }
ok64 HTTPonBody(u8cs tok, HTTPstate* state) { return OK; }
ok64 HTTPonRequest(u8cs tok, HTTPstate* state) { return OK; }
ok64 HTTPonStatusCode(u8cs tok, HTTPstate* state) {
    u8csMv(state->status_code, tok);
    return OK;
}
ok64 HTTPonReasonPhrase(u8cs tok, HTTPstate* state) {
    u8csMv(state->reason, tok);
    return OK;
}
ok64 HTTPonStatusLine(u8cs tok, HTTPstate* state) { return OK; }
ok64 HTTPonResponse(u8cs tok, HTTPstate* state) { return OK; }
ok64 HTTPonMessage(u8cs tok, HTTPstate* state) { return OK; }
ok64 HTTPonRoot(u8cs tok, HTTPstate* state) { return OK; }

ok64 HTTPutf8Drain(u8cs from, HTTPstate* http) {
    u8csMv(http->data, from);
    ok64 o = HTTPLexer(http);
    if (o == END) o = OK;
    // ABC-011: consume the parsed head; STD-26B068gn1w: only the head
    if (o == OK) u8csMv(from, http->data);
    return o;
}

ok64 HTTPutf8Feed(u8s into, HTTPstate const* http) {
    ok64 o = OK;
    if (!u8csEmpty(http->method)) {
        // Request line
        o = u8sFeed(into, http->method);
        if (o != OK) return o;
        o = u8sFeed1(into, ' ');
        if (o != OK) return o;
        o = u8sFeed(into, http->uri);
        if (o != OK) return o;
        o = u8sFeed1(into, ' ');
        if (o != OK) return o;
        o = u8sFeed(into, http->version);
        if (o != OK) return o;
        o = u8sFeed2(into, '\r', '\n');
        if (o != OK) return o;
    } else if (!u8csEmpty(http->status_code)) {
        // Status line
        o = u8sFeed(into, http->version);
        if (o != OK) return o;
        o = u8sFeed1(into, ' ');
        if (o != OK) return o;
        o = u8sFeed(into, http->status_code);
        if (o != OK) return o;
        o = u8sFeed1(into, ' ');
        if (o != OK) return o;
        o = u8sFeed(into, http->reason);
        if (o != OK) return o;
        o = u8sFeed2(into, '\r', '\n');
        if (o != OK) return o;
    }
    // Headers; ABC-011: {headers[0], *headers} was {p, p}, a dead loop
    a_dup(u8cs, hdrs, http->headers);
    while (u8cssLen(hdrs) >= 2) {
        o = u8sFeed(into, $at(hdrs, 0));
        if (o != OK) return o;
        o = u8sFeed2(into, ':', ' ');
        if (o != OK) return o;
        o = u8sFeed(into, $at(hdrs, 1));
        if (o != OK) return o;
        o = u8sFeed2(into, '\r', '\n');
        if (o != OK) return o;
        o = u8cssUsed(hdrs, 2);
        if (o != OK) return o;
    }
    // End of headers
    o = u8sFeed2(into, '\r', '\n');
    return o;
}

// STD-26B068WrsM: RFC 9110 §5.1 names are ASCII case-insensitive
static b8 HTTPnameEq(u8csc a, u8csc b) {
    if ($len(a) != $len(b)) return NO;
    for (u8c *p = a[0], *q = b[0]; p < a[1]; ++p, ++q) {
        u8 x = *p, y = *q;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return NO;
    }
    return YES;
}

ok64 HTTPfind(u8cs *value, u8csc key, u8css headers) {
    for (i64 i = 0; i + 1 < $len(headers); i += 2) {
        if (HTTPnameEq($at(headers, i), key)) {
            u8csMv(*value, $at(headers, i + 1));
            return OK;
        }
    }
    return HTTPNONE;
}
