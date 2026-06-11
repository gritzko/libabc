#include "URI.h"

#include <stdio.h>
#include <stdlib.h>

#include "B.h"
#include "FILE.h"
#include "PATH.h"
#include "PRO.h"
#include "TEST.h"

ok64 URItest1() {
    sane(1);
#define LEN1 5
    u8cs inputs[LEN1] = {
        $u8str("http://mit.edu"),
        $u8str("git+ssh://git@github.com/gritzko/librdx"),
        $u8str("ftp://1.2.3.4/some/path"),
        $u8str("//1.2.3.4/some/path"),
        $u8str("http://myserver:123/path?query#fragment"),
    };

    for (int i = 0; i < LEN1; ++i) {
        URIstate state = {};
        $mv(state.data, inputs[i]);
        call(URILexer, &state);
    }
    done;
}

ok64 URItest2() {
    sane(1);
    a$str(uri, "http://myserver:123/path?query#fragment");
    a$str(scheme, "http");
    a$str(host, "myserver");
    a$str(port, "123");
    a$str(path, "/path");
    a$str(query, "query");
    a$str(fragment, "fragment");
    URIstate state = {};
    $mv(state.data, uri);
    call(URILexer, &state);
    $println(state.scheme);
    $testeq(state.scheme, scheme);
    $testeq(state.host, host);
    $testeq(state.port, port);
    $testeq(state.path, path);
    $testeq(state.query, query);
    $testeq(state.fragment, fragment);
    done;
}

ok64 URItest3() {
    sane(1);
    a$str(uri, "http://user@myserver:123/path?query#fragment");
    URIstate state = {};
    call(URIutf8Drain, uri, &state);
    a_pad(u8, out, 256);
    call(URIutf8Feed, out_idle, &state);
    u8cs result = {out[1], *out_idle};
    $testeq(result, uri);
    done;
}

ok64 URItest4() {
    sane(1);
    u8cs inputs[] = {
        $u8str("http://mit.edu"),
        $u8str("git+ssh://git@github.com/gritzko/librdx"),
        $u8str("ftp://1.2.3.4/some/path"),
        $u8str("http://myserver:123/path?query#fragment"),
        //  Present-but-empty query: a bare `?` means "the trunk" in
        //  dog/DOG's K/V convention.  Round-trip must keep the `?`.
        $u8str("?#0123456789abcdef0123456789abcdef01234567"),
        //  Present-but-empty fragment: a trailing `#` with nothing
        //  after signals a deletion row (`?branch#`).  Round-trip
        //  must keep the `#`.
        $u8str("?feature/fix1#"),
        //  Both present-but-empty on a remote observation row.
        $u8str("//peer/repo?#cafebabecafebabecafebabecafebabecafebabe"),
    };
    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        URIstate state = {};
        call(URIutf8Drain, inputs[i], &state);
        a_pad(u8, out, 256);
        call(URIutf8Feed, out_idle, &state);
        u8cs result = {out[1], *out_idle};
        $testeq(result, inputs[i]);
    }
    done;
}

ok64 URItest5() {
    sane(1);
    a$str(fail, " -> FAIL\n");
    a$str(schemelab, " -> scheme=");
    a$str(pathlab, " path=");
    a$str(reconlab, "  reconstructed: ");
    u8cs urns[] = {
        $u8str("urn:isbn:0451450523"),
        $u8str("urn:ietf:rfc:2648"),
        $u8str("urn:uuid:6e8bc430-9c3a-11d9-9669-0800200c9a66"),
        $u8str("mailto:user@example.com"),
        $u8str("tel:+1-816-555-1212"),
        $u8str("file:///etc/passwd"),
        $u8str("data:text/plain;base64,SGVsbG8="),
    };
    for (size_t i = 0; i < sizeof(urns) / sizeof(urns[0]); ++i) {
        URIstate state = {};
        ok64 o = URIutf8Drain(urns[i], &state);
        $print(urns[i]);
        if (o != OK) {
            $print(fail);
            continue;
        }
        $print(schemelab);
        $print(state.scheme);
        $print(pathlab);
        $println(state.path);
        a_pad(u8, out, 256);
        call(URIutf8Feed, out_idle, &state);
        u8cs result = {out[1], *out_idle};
        $print(reconlab);
        $println(result);
    }
    done;
}

// URI-004: the lexer no longer materializes a path-`segments` buffer.
// Readers walk the parsed `path` component on demand via abc/PATH.
// This table-driven test parses each URI, then asserts that an
// abc/PATH segment walk of `u.path` yields exactly the expected
// non-empty segments — the same result the old `segments` buffer
// produced (PATHu8sDrainNE skips empty runs: leading '/', '//',
// trailing '/').  Cases cover the basic path, a trailing slash, a
// path with NO segments (bare '/'), and a URI with no path at all.
ok64 URITestSegments() {
    sane(1);

    static struct {
        char const *uri;
        char const *segs[8];   // NULL-terminated expected segment list
    } cases[] = {
        { "http://example.com/a/b/c", { "a", "b", "c", NULL } },
        { "http://example.com/a/b/",  { "a", "b", NULL } },     // trailing '/'
        { "http://example.com/",      { NULL } },               // zero segments
        { "http://example.com",       { NULL } },               // no path slot
        { "http://h/single",          { "single", NULL } },     // one segment
        { "/a/b/c",                   { "a", "b", "c", NULL } }, // path-only ref
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        a_cstr(uri_str, cases[i].uri);
        uri u = {};
        call(URIutf8Drain, uri_str, &u);

        size_t n = 0;
        $eachseg(seg, u.path) {
            test(cases[i].segs[n] != NULL, URIFAIL);   // not too many
            a_cstr(expect, cases[i].segs[n]);
            $testeq(seg, expect);
            n++;
        }
        // Walked exactly the expected count (no missing tail).
        test(cases[i].segs[n] == NULL, URIFAIL);
    }

    done;
}

// Table-driven test for URIRelative and URIAbsolute
// Each triplet: {base, specific, expected_relative}
// Verifies: URIRelative(base, specific) == relative
//           URIAbsolute(base, relative) == specific (round-trip)
ok64 URITestTable() {
    sane(1);

    // Test triplets: base, specific, expected relative
    // NULL relative means "same as specific" (for scheme-differs case)
    static char const* triplets[][3] = {
        // Identical URIs -> empty relative
        {"http://example.com/path", "http://example.com/path", ""},

        // Different scheme -> full URI (scheme included)
        {"http://example.com/path", "https://example.com/path", "https://example.com/path"},
        {"ftp://server/file", "http://server/file", "http://server/file"},

        // Same scheme, different host -> authority + path
        {"http://example.com/path", "http://other.com/newpath", "//other.com/newpath"},
        {"http://a.com/x", "http://b.com/y", "//b.com/y"},

        // Same scheme+host, different port -> authority + path
        {"http://example.com/path", "http://example.com:8080/path", "//example.com:8080/path"},

        // Same scheme+host, different user -> authority + path
        {"http://example.com/path", "http://user@example.com/path", "//user@example.com/path"},

        // Same authority, different path - sibling files (same directory)
        {"http://example.com/path1", "http://example.com/path2", "path2"},
        {"http://example.com/dir/file1", "http://example.com/dir/file2", "file2"},

        // Same authority, different path - go up directories
        {"http://example.com/a/b", "http://example.com/c/d", "../c/d"},
        {"http://example.com/a/b/c", "http://example.com/x/y", "../../x/y"},
        {"http://example.com/a/b/c", "http://example.com/a/x", "../x"},

        // Same authority, go down directories
        {"http://example.com/a", "http://example.com/a/b/c", "a/b/c"},
        {"http://example.com/dir/file", "http://example.com/dir/sub/file", "sub/file"},

        // Same path, different query -> query only
        {"http://example.com/path?q1", "http://example.com/path?q2", "?q2"},
        {"http://example.com/path", "http://example.com/path?newq", "?newq"},

        // Same path, base has query, specific has none -> emit bare
        // `?` to override base's query to empty (per RFC 3986).
        {"http://example.com/path?oldq", "http://example.com/path", "?"},

        // Same query, different fragment -> fragment only
        {"http://example.com/path#f1", "http://example.com/path#f2", "#f2"},
        {"http://example.com/path", "http://example.com/path#frag", "#frag"},

        // Same fragment in base, none in specific -> emit bare `#`
        // to override base's fragment to empty.
        {"http://example.com/path#old", "http://example.com/path", "#"},

        // Complex cases with query and fragment
        {"http://example.com/path?q#f", "http://example.com/path?q#f2", "#f2"},
        {"http://example.com/path?q1#f", "http://example.com/path?q2#f", "?q2#f"},
        {"http://example.com/a", "http://example.com/b?q#f", "b?q#f"},
        {"http://a.com/x", "http://b.com/y?q#f", "//b.com/y?q#f"},

        // User, port combinations
        {"http://user@example.com:80/path", "http://user@example.com:80/other", "other"},
        {"http://example.com/path", "http://user@other.com:8080/new?q#f",
         "//user@other.com:8080/new?q#f"},

        // Deep relative paths
        {"http://example.com/a/b/c/d", "http://example.com/a/b/x/y", "../x/y"},
        {"http://example.com/a/b/c/d", "http://example.com/a/e/f/g", "../../e/f/g"},
        {"http://example.com/1/2/3/4/5", "http://example.com/1/2/x", "../../x"},
    };

    size_t n = sizeof(triplets) / sizeof(triplets[0]);

    for (size_t i = 0; i < n; i++) {
        char const* base_str = triplets[i][0];
        char const* specific_str = triplets[i][1];
        char const* expected_rel_str = triplets[i][2];

        // Parse base and specific
        u8cs base_slice = {(u8cp)base_str, (u8cp)(base_str + strlen(base_str))};
        u8cs specific_slice = {(u8cp)specific_str, (u8cp)(specific_str + strlen(specific_str))};

        uri base = {}, specific = {}, rel = {};
        call(URIutf8Drain, base_slice, &base);
        call(URIutf8Drain, specific_slice, &specific);

        // Compute relative
        call(URIRelative, &rel, &base, &specific);

        // Serialize relative
        a_pad(u8, relbuf, 512);
        call(URIutf8Feed, relbuf_idle, &rel);
        u8cs rel_result;
        u8csDup(rel_result, u8bDataC(relbuf));

        // Compare with expected
        u8cs expected_rel = {(u8cp)expected_rel_str, (u8cp)(expected_rel_str + strlen(expected_rel_str))};
        if (!$eq(rel_result, expected_rel)) {
            fprintf(stderr, "FAIL test %zu: URIRelative\n", i);
            fprintf(stderr, "  base:     %s\n", base_str);
            fprintf(stderr, "  specific: %s\n", specific_str);
            fprintf(stderr, "  expected: '%s'\n", expected_rel_str);
            fprintf(stderr, "  got:      '%.*s'\n", (int)$len(rel_result), *rel_result);
            return URIFAIL;
        }

        // Round-trip: resolve relative back to absolute
        uri resolved = {};
        call(URIAbsolute, &resolved, &base, &rel);

        // Compare all components
        if (!$eq(resolved.scheme, specific.scheme) ||
            !$eq(resolved.host, specific.host) ||
            !$eq(resolved.port, specific.port) ||
            !$eq(resolved.user, specific.user) ||
            !$eq(resolved.path, specific.path) ||
            !$eq(resolved.query, specific.query) ||
            !$eq(resolved.fragment, specific.fragment)) {
            fprintf(stderr, "FAIL test %zu: round-trip\n", i);
            fprintf(stderr, "  base:     %s\n", base_str);
            fprintf(stderr, "  specific: %s\n", specific_str);
            fprintf(stderr, "  relative: '%s'\n", expected_rel_str);
            a_pad(u8, resbuf, 512);
            call(URIutf8Feed, resbuf_idle, &resolved);
            fprintf(stderr, "  resolved: '%.*s'\n", (int)$len(u8bDataC(resbuf)), *u8bDataC(resbuf));
            return URIFAIL;
        }
    }

    done;
}

ok64 URITestEscape() {
    sane(1);

    // Test escape
    static struct { char const* raw; char const* escaped; } esc_cases[] = {
        {"hello", "hello"},
        {"hello world", "hello%20world"},
        {"a/b/c", "a%2Fb%2Fc"},
        {"foo@bar.com", "foo%40bar.com"},
        {"100%", "100%25"},
        {"a-b_c.d~e", "a-b_c.d~e"},  // unreserved pass through
        {"файл", "%D1%84%D0%B0%D0%B9%D0%BB"},  // UTF-8
        {"", ""},
    };

    for (size_t i = 0; i < sizeof(esc_cases) / sizeof(esc_cases[0]); i++) {
        u8cs raw = {(u8cp)esc_cases[i].raw, (u8cp)(esc_cases[i].raw + strlen(esc_cases[i].raw))};
        u8cs expected = {(u8cp)esc_cases[i].escaped, (u8cp)(esc_cases[i].escaped + strlen(esc_cases[i].escaped))};

        a_pad(u8, buf, 256);
        call(URIu8sEsc, buf_idle, raw);
        u8cs result = {buf[1], *buf_idle};

        if (!$eq(result, expected)) {
            fprintf(stderr, "FAIL escape[%zu]: '%s' expected '%s' got '%.*s'\n",
                    i, esc_cases[i].raw, esc_cases[i].escaped, (int)$len(result), *result);
            return URIFAIL;
        }
    }

    // Test unescape
    static struct { char const* escaped; char const* raw; } unesc_cases[] = {
        {"hello", "hello"},
        {"hello%20world", "hello world"},
        {"a%2Fb%2Fc", "a/b/c"},
        {"foo%40bar.com", "foo@bar.com"},
        {"100%25", "100%"},
        {"%D1%84%D0%B0%D0%B9%D0%BB", "файл"},
        {"bad%zz", "bad%zz"},  // invalid hex passes through
        {"trailing%", "trailing%"},  // incomplete passes through
        {"", ""},
    };

    for (size_t i = 0; i < sizeof(unesc_cases) / sizeof(unesc_cases[0]); i++) {
        u8cs escaped = {(u8cp)unesc_cases[i].escaped, (u8cp)(unesc_cases[i].escaped + strlen(unesc_cases[i].escaped))};
        u8cs expected = {(u8cp)unesc_cases[i].raw, (u8cp)(unesc_cases[i].raw + strlen(unesc_cases[i].raw))};

        a_pad(u8, buf, 256);
        call(URIu8sUnesc, buf_idle, escaped);
        u8cs result = {buf[1], *buf_idle};

        if (!$eq(result, expected)) {
            fprintf(stderr, "FAIL unescape[%zu]: '%s' expected '%s' got '%.*s'\n",
                    i, unesc_cases[i].escaped, unesc_cases[i].raw, (int)$len(result), *result);
            return URIFAIL;
        }
    }

    // Round-trip test
    a$str(original, "hello world/path?query=value&foo=bar");
    a_pad(u8, escbuf, 256);
    call(URIu8sEsc, escbuf_idle, original);
    u8cs escaped = {escbuf[1], *escbuf_idle};

    a_pad(u8, unescbuf, 256);
    call(URIu8sUnesc, unescbuf_idle, escaped);
    u8cs roundtrip = {unescbuf[1], *unescbuf_idle};

    if (!$eq(roundtrip, original)) {
        fprintf(stderr, "FAIL escape round-trip\n");
        return URIFAIL;
    }

    done;
}

// Test zeroing the path component of a parsed URI
ok64 URITestPathZero() {
    sane(1);

    a$str(uri_str, "xx://branch.name/some/path?v=123");
    uri u = {};
    call(URIutf8Drain, uri_str, &u);

    // Verify initial parse
    a$str(expect_host, "branch.name");
    a$str(expect_path, "/some/path");
    a$str(expect_query, "v=123");
    $testeq(u.host, expect_host);
    $testeq(u.path, expect_path);
    $testeq(u.query, expect_query);

    // Zero the path
    u.path[0] = u.path[1] = NULL;

    // Serialize back
    a_pad(u8, out, 256);
    call(URIutf8Feed, out_idle, &u);
    u8cs result = {out[1], *out_idle};

    // Verify serialization: scheme + authority + query, no path
    a$str(expect_result, "xx://branch.name?v=123");
    $testeq(result, expect_result);

    // Re-parse with a "/" path prefix so the parser is happy
    // (RFC 3986: path-abempty after authority can be empty,
    //  but our ragel grammar requires "/" to start a path)
    a$str(uri_fixed, "xx://branch.name/?v=123");
    uri u2 = {};
    call(URIutf8Drain, uri_fixed, &u2);

    // Host and query intact, path is just "/"
    $testeq(u2.host, expect_host);
    $testeq(u2.query, expect_query);
    a$str(expect_slash, "/");
    $testeq(u2.path, expect_slash);

    done;
}

ok64 URITestBareQuery() {
    sane(1);

    // bare ?query
    a$str(q, "?refs/heads/master");
    uri uq = {};
    call(URIutf8Drain, q, &uq);
    a$str(expect_q, "refs/heads/master");
    $testeq(uq.query, expect_q);
    test($empty(uq.authority), URIBAD);
    test($empty(uq.path), URIBAD);
    test($empty(uq.scheme), URIBAD);

    // bare #fragment
    a$str(f, "#abc123def");
    uri uf = {};
    call(URIutf8Drain, f, &uf);
    a$str(expect_f, "abc123def");
    $testeq(uf.fragment, expect_f);
    test($empty(uf.authority), URIBAD);
    test($empty(uf.path), URIBAD);

    // //authority/path?query
    a$str(full, "//localhost/src/git?refs/tags/v1");
    uri ufull = {};
    call(URIutf8Drain, full, &ufull);
    a$str(expect_host, "localhost");
    a$str(expect_path, "/src/git");
    a$str(expect_ref, "refs/tags/v1");
    $testeq(ufull.host, expect_host);
    $testeq(ufull.path, expect_path);
    $testeq(ufull.query, expect_ref);

    done;
}

// URI-003: serialization safety.  For a component tuple, URIutf8FeedSafe
// must EITHER emit a URI whose re-parse is byte-identical in all 8
// fields (safe == OK), OR refuse with URIUNSAFE and emit NOTHING — it
// must NEVER silently emit a URI that re-parses to different components.
// The table sets one reserved/foreign delimiter into each component in
// turn (path `a#b`/`a?b`, query `q#x`, fragment, host `h/p`, user, port)
// alongside the legitimately-allowed-reserved cases that MUST stay safe
// (query `q=v`, fragment `a/b?c`, user `u@x`, path `/a/b`).
ok64 URITestFeedSafe() {
    sane(1);

    enum { SC, AU, HO, PO, US, PA, QU, FR, NCOMP };
    static struct {
        char const *comp[NCOMP];   // scheme,auth,host,port,user,path,query,fragment (NULL = absent)
        b8          safe;          // YES => expect OK + identity; NO => expect URIUNSAFE
    } cases[] = {
        // --- foreign delimiter in a component => UNSAFE (would bleed).
        //  URIutf8Feed emits the `authority` slice as a whole, so the
        //  host-injection case lives in `authority`; FeedSafe's host
        //  field then disagrees with the re-parse. ---
        {{NULL,NULL,NULL,NULL,NULL,"a#b",NULL,NULL},                 NO},  // '#' -> fragment
        {{NULL,NULL,NULL,NULL,NULL,"a?b",NULL,NULL},                 NO},  // '?' -> query
        {{NULL,NULL,NULL,NULL,NULL,"/p","q#x",NULL},                 NO},  // '#' in query -> fragment
        {{NULL,"//h/p","h",NULL,NULL,NULL,NULL,NULL},                NO},  // '/' in authority -> path
        // --- legitimately-allowed reserved chars => SAFE (must round-trip) ---
        {{NULL,NULL,NULL,NULL,NULL,"/a/b",NULL,NULL},                YES}, // '/' belongs to path
        {{NULL,NULL,NULL,NULL,NULL,"/p","q=v",NULL},                 YES}, // '=' allowed in query
        {{NULL,NULL,NULL,NULL,NULL,"/p",NULL,"a/b?c"},               YES}, // '/' '?' allowed in fragment
        {{NULL,"//u@h","h",NULL,"u",NULL,NULL,NULL},                 YES}, // '@' splits userinfo/host
        {{"http","//ex.com","ex.com",NULL,NULL,"/path","q","f"},     YES}, // full valid URI
        {{NULL,NULL,NULL,NULL,NULL,"refs/heads/master",NULL,NULL},   YES}, // rootless multi-seg path
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        uri u = {};
        char const **c = cases[i].comp;
        if (c[SC]) { a_cstr(s, c[SC]); u8csDup(u.scheme, s); }
        if (c[AU]) { a_cstr(s, c[AU]); u8csDup(u.authority, s); }
        if (c[HO]) { a_cstr(s, c[HO]); u8csDup(u.host, s); }
        if (c[PO]) { a_cstr(s, c[PO]); u8csDup(u.port, s); }
        if (c[US]) { a_cstr(s, c[US]); u8csDup(u.user, s); }
        if (c[PA]) { a_cstr(s, c[PA]); u8csDup(u.path, s); }
        if (c[QU]) { a_cstr(s, c[QU]); u8csDup(u.query, s); }
        if (c[FR]) { a_cstr(s, c[FR]); u8csDup(u.fragment, s); }

        a_pad(u8, out, 256);
        ok64 o = URIutf8FeedSafe(out_idle, &u);
        size_t emitted = u8csLen(u8bDataC(out));

        if (cases[i].safe) {
            // Must succeed AND the bytes must re-parse to the same fields.
            if (o != OK) {
                fprintf(stderr, "FAIL FeedSafe[%zu]: expected OK got %s\n",
                        i, o == URIUNSAFE ? "URIUNSAFE" : "OTHER");
                return URIFAIL;
            }
            uri re = {};
            a_dup(u8c, txt, u8bDataC(out));
            call(URIutf8Drain, txt, &re);
            if (!$eq(re.scheme, u.scheme) || !$eq(re.authority, u.authority) ||
                !$eq(re.host, u.host) || !$eq(re.port, u.port) ||
                !$eq(re.user, u.user) || !$eq(re.path, u.path) ||
                !$eq(re.query, u.query) || !$eq(re.fragment, u.fragment)) {
                fprintf(stderr, "FAIL FeedSafe[%zu]: emitted not identity\n", i);
                return URIFAIL;
            }
        } else {
            // Must refuse AND emit nothing (no ambiguous URI).
            if (o != URIUNSAFE) {
                fprintf(stderr, "FAIL FeedSafe[%zu]: expected URIUNSAFE got %s\n",
                        i, o == OK ? "OK" : "OTHER");
                return URIFAIL;
            }
            test(emitted == 0, URIFAIL);   // emitted nothing on reject
        }
    }

    done;
}

// URI-003: the path-noscheme round-trip.  A relative reference with no
// authority and a rootless path (`ht/h/p#abc`) must survive
// parse -> URIRelative -> URIAbsolute with NO spurious leading `/`
// (RFC 3986 §5.3 recompose).  This is the original URI-003 defect:
// before the fix `resolved.path` came back as `/ht/h/p`.
ok64 URITestNoschemeRoundTrip() {
    sane(1);
    a_cstr(in, "ht/h/p#abc");
    uri spec = {};
    call(URIutf8Drain, in, &spec);

    a_cstr(base_str, "http://example.com/path");
    uri base = {};
    call(URIutf8Drain, base_str, &base);

    uri rel = {};
    call(URIRelative, &rel, &base, &spec);

    uri resolved = {};
    call(URIAbsolute, &resolved, &base, &rel);

    // Path must be preserved verbatim — no leading slash injected.
    a_cstr(want_path, "ht/h/p");
    $testeq(resolved.path, want_path);
    a_cstr(want_frag, "abc");
    $testeq(resolved.fragment, want_frag);
    // Sanity: leading byte is NOT '/'.
    test(resolved.path[0][0] != '/', URIFAIL);

    done;
}

ok64 URItest() {
    sane(1);
    call(URItest1);
    call(URItest2);
    call(URItest3);
    call(URItest4);
    call(URItest5);
    call(URITestSegments);
    call(URITestTable);
    call(URITestEscape);
    call(URITestPathZero);
    call(URITestBareQuery);
    call(URITestFeedSafe);
    call(URITestNoschemeRoundTrip);
    done;
}

TEST(URItest);
