#include "URI.h"

#include "PATH.h"
#include "PRO.h"

ok64 URIonPath(u8cs tok, urip state) {
    u8csMv(state->path, tok);
    return OK;
}
ok64 URIonPathNoscheme(u8cs tok, urip state) {
    u8csMv(state->path, tok);
    return OK;
}
ok64 URIonPathRootless(u8cs tok, urip state) {
    u8csMv(state->path, tok);
    return OK;
}
ok64 URIonScheme(u8cs tok, urip state) {
    $mv(state->scheme, tok);
    --state->scheme[1];
    return OK;
}
ok64 URIonIPv4address(u8cs tok, urip state) { return OK; }
ok64 URIonIPvFuture(u8cs tok, urip state) { return OK; }
ok64 URIonIPv6address(u8cs tok, urip state) { return OK; }
ok64 URIonIP_literal(u8cs tok, urip state) { return OK; }
ok64 URIonUser(u8cs tok, urip state) {
    $mv(state->user, tok);
    return OK;
}
ok64 URIonHost(u8cs tok, urip state) {
    $mv(state->host, tok);
    return OK;
}
ok64 URIonPort(u8cs tok, urip state) {
    $mv(state->port, tok);
    return OK;
}
ok64 URIonFragment(u8cs tok, urip state) {
    $mv(state->fragment, tok);
    return OK;
}
ok64 URIonQuery(u8cs tok, urip state) {
    $mv(state->query, tok);
    return OK;
}
ok64 URIonAuthority(u8cs tok, urip state) {
    u8csMv(state->authority, tok);
    return OK;
}
ok64 URIonURI(u8cs tok, urip state) { return OK; }
ok64 URIonRoot(u8cs tok, urip state) { return OK; }
//  Path segments are no longer materialized at parse time (URI-004):
//  the lexer leaves the whole path in `state->path`; readers walk it
//  on demand via abc/PATH (see URISplitPath below).  These two
//  callbacks are now inert.
ok64 URIonSegment(u8cs tok, urip state) { return OK; }
ok64 URIonSegment_nz(u8cs tok, urip state) { return OK; }

ok64 URIutf8Drain(u8cs from, urip u) {
    zerop(u);
    $mv(u->data, from);
    return URILexer(u);
}

ok64 URIutf8Feed(u8s into, uricp u) {
    if (!$empty(u->scheme)) {
        ok64 o = u8sFeed(into, u->scheme);
        if (o != OK) return o;
        o = u8sFeed1(into, ':');
        if (o != OK) return o;
    }
    if (!u8csEmpty(u->authority)) {
        ok64 o = u8sFeed(into, u->authority);
        if (o != OK) return o;
    }
    if (!$empty(u->path)) {
        ok64 o = u8sFeed(into, u->path);
        if (o != OK) return o;
    }
    //  Presence test is `[0] != NULL` (not $empty): a slice `{p,p}`
    //  with both endpoints non-NULL means "explicitly empty" —
    //  `?#<sha>` (trunk move) and `?branch#` (deletion) depend on
    //  the sigil being emitted for a present-but-empty component.
    if (u->query[0] != NULL) {
        ok64 o = u8sFeed1(into, '?');
        if (o != OK) return o;
        if (!$empty(u->query)) {
            o = u8sFeed(into, u->query);
            if (o != OK) return o;
        }
    }
    if (u->fragment[0] != NULL) {
        ok64 o = u8sFeed1(into, '#');
        if (o != OK) return o;
        if (!$empty(u->fragment)) {
            o = u8sFeed(into, u->fragment);
            if (o != OK) return o;
        }
    }
    return OK;
}

//  Serialize `u`, then PROVE the text re-parses to the same 8
//  components — else err out without touching the caller's `into`.
//  We never want to hand back a URI that means something other than
//  what the caller assembled, so the bytes go into BASS scratch first,
//  get re-parsed and field-compared, and are copied into `into` only
//  once every component matches.  Mismatch ⇒ URIUNSAFE, nothing emitted.
ok64 URIutf8FeedSafe(u8s into, uricp u) {
    sane($ok(into));
    //  Serialize into private scratch (not the caller's buffer).
    a_carve(u8, scratch, MAX_URI_LEN);
    call(URIutf8Feed, u8bIdle(scratch), u);
    a_dup(u8c, text, u8bDataC(scratch));

    //  Re-parse the emitted bytes.  URIutf8Drain CONSUMES its input, so
    //  hand it a copy of the slice (the underlying bytes stay in
    //  scratch); `round.data` views into them.
    uri round = {};
    a_dup(u8c, parse_in, text);
    call(URIutf8Drain, parse_in, &round);

    //  Every one of the 8 components must be byte-identical.  $eq is
    //  abc/S.md's slice equality (same content), matching the ticket's
    //  per-field u8csEq compare; a foreign delimiter that bled across a
    //  boundary lands in a DIFFERENT field, so some pair differs.
    b8 same = $eq(round.scheme, u->scheme) && $eq(round.authority, u->authority) &&
        $eq(round.user, u->user) && $eq(round.host, u->host) &&
        $eq(round.port, u->port) && $eq(round.path, u->path) &&
        $eq(round.query, u->query) && $eq(round.fragment, u->fragment);
    test(same, URIUNSAFE);

    //  Verified: copy the exact bytes into the caller's buffer.
    call(u8sFeed, into, text);
    done;
}

// Split path into its non-empty segments, storing each (a view into
// `path`) in the `segs` buffer.  Walks the path on demand via abc/PATH
// (URI-004): no hand-rolled '/'-splitting, no parse-time segments
// buffer.  PATHu8sDrainNE skips empty runs (leading '/', '//',
// trailing '/'), matching the old behaviour exactly.
static ok64 URISplitPath(u8csbp segs, u8csc path) {
    sane(segs);
    u8csbReset(segs);
    if (u8csEmpty(path)) done;

    a_dup(u8c, cursor, path);
    u8cs seg = {};
    while (PATHu8sDrainNE(cursor, seg) == OK) {
        call(u8cssFeed1, u8csbIdle(segs), seg);
    }
    done;
}

// PTR-009: compute relative path from base to specific.  The `../`-climb
// + tail is fed into the CALLER-OWNED writable slice `out` (no static
// buf) via a gauge; `result` views the WRITTEN PREFIX (gauge left),
// which the caller keeps live.  Typed slice feeds only — zero ptr math.
static ok64 URIRelativePath(u8csp result, u8s out, u8cscs base_segs,
                            u8cscs spec_segs) {
    sane(result && $ok(out));
    u8g g;
    u8gOf(g, out);
    size_t base_n = u8cscsLen(base_segs);
    size_t spec_n = u8cscsLen(spec_segs);

    // Base "directory" is all but last segment (last is the "file")
    size_t base_dir_n = base_n > 0 ? base_n - 1 : 0;

    // Find common prefix length
    size_t common = 0;
    while (common < base_dir_n && common < spec_n) {
        u8cs const *bs = u8cscsAtP(base_segs, common);
        u8cs const *ss = u8cscsAtP(spec_segs, common);
        if (!u8csEq(*bs, *ss)) break;
        common++;
    }

    a_cstr(dotdot, "..");

    // Generate ".." for each segment to climb from base dir
    size_t up = base_dir_n - common;
    for (size_t i = 0; i < up; i++) {
        if (i > 0) call(u8gFeed1, g, '/');
        call(u8gFeed, g, dotdot);
    }

    // Append remaining specific segments
    for (size_t i = common; i < spec_n; i++) {
        if (up > 0 || i > common) call(u8gFeed1, g, '/');
        u8cs const *seg = u8cscsAtP(spec_segs, i);
        call(u8gFeed, g, *seg);
    }

    u8csMv(result, u8gLeftC(g));
    done;
}

// Produce relative URI: parts of `specific` that differ from `base`.
// PTR-009: `out` is a caller-owned writable slice for the computed
// relative path; `rel->path` may view its written prefix, so `out` must
// outlive `rel`.
ok64 URIRelative(urip rel, uricp base, uricp specific, u8s out) {
    sane(rel && base && specific && $ok(out));
    zerop(rel);

    // If schemes differ, return full specific URI
    if (!$eq(base->scheme, specific->scheme)) {
        *rel = *specific;
        done;
    }

    // Same scheme - check authority (user, host, port)
    if (!$eq(base->host, specific->host) || !$eq(base->port, specific->port) ||
        !$eq(base->user, specific->user)) {
        // Different authority - include authority and path
        u8csDup(rel->authority, specific->authority);
        u8csDup(rel->host, specific->host);
        u8csDup(rel->port, specific->port);
        u8csDup(rel->user, specific->user);
        u8csDup(rel->path, specific->path);
        u8csDup(rel->query, specific->query);
        u8csDup(rel->fragment, specific->fragment);
        done;
    }

    // Same authority - check path
    if (!$eq(base->path, specific->path)) {
        // Split paths into segments
        a_pad(u8cs, base_segs_arr, 64);
        a_pad(u8cs, spec_segs_arr, 64);
        call(URISplitPath, base_segs_arr, base->path);
        call(URISplitPath, spec_segs_arr, specific->path);

        u8cscs base_segs, spec_segs;
        u8cscsDup(base_segs, u8csbDataC(base_segs_arr));
        u8cscsDup(spec_segs, u8csbDataC(spec_segs_arr));

        // Compute relative path
        call(URIRelativePath, rel->path, out, base_segs, spec_segs);
        // Copy query/fragment (or mark explicitly empty)
        if (*specific->query) {
            u8csDup(rel->query, specific->query);
        } else if (*base->query) {
            rel->query[0] = rel->query[1] = *specific->data;
        }
        if (*specific->fragment) {
            u8csDup(rel->fragment, specific->fragment);
        } else if (*base->fragment) {
            rel->fragment[0] = rel->fragment[1] = *specific->data;
        }
        done;
    }

    // Same path - check query
    if (!$eq(base->query, specific->query)) {
        // Use non-NULL empty slice to indicate "explicitly no query" vs NULL "not set"
        if (*specific->query) {
            u8csDup(rel->query, specific->query);
        } else {
            // Empty but non-NULL: marks "explicitly no query"
            rel->query[0] = rel->query[1] = *specific->data;
        }
        // Copy fragment (or mark explicitly empty only when base has
        // one to override; otherwise leave NULL so the emit drops it).
        if (*specific->fragment) {
            u8csDup(rel->fragment, specific->fragment);
        } else if (*base->fragment) {
            rel->fragment[0] = rel->fragment[1] = *specific->data;
        }
        done;
    }

    // Same query - check fragment
    if (!$eq(base->fragment, specific->fragment)) {
        if (*specific->fragment) {
            u8csDup(rel->fragment, specific->fragment);
        } else {
            // Empty but non-NULL: marks "explicitly no fragment"
            rel->fragment[0] = rel->fragment[1] = *specific->data;
        }
    }

    done;
}

// PTR-009: RFC 3986 §5.2.4 remove_dot_segments as a PATH segment-walk
// (no in/out cursor): push each segment, drop ".", pop the last segment
// on "..", then rebuild into the caller-owned slice `out` via a gauge;
// `result` views the written prefix.  `is_abs`/`trail` carry the
// absolute-prefix and dot-induced trailing slash so the result is
// byte-identical to the old cursor code.
static ok64 URIRemoveDots(u8csp result, u8s out, u8csc merged) {
    sane(result && $ok(out));
    b8 is_abs = !u8csEmpty(merged) && merged[0][0] == '/';
    // Output ends with '/' iff merged does, OR the final token is a
    // "."/".." (a directory ref, per RFC).  `ends_slash` is the fixed
    // property; `last_dot` tracks whether the LAST token was a dot-seg.
    b8 ends_slash = !u8csEmpty(merged) && merged[1][-1] == '/';
    b8 last_dot = NO;

    a_pad(u8cs, segs, 256);
    a_dup(u8c, cursor, merged);
    u8cs seg = {};
    while (PATHu8sDrain(cursor, seg) == OK) {
        if (u8csEmpty(seg)) continue;
        b8 dot = u8csLen(seg) == 1 && seg[0][0] == '.';
        b8 dotdot = u8csLen(seg) == 2 && seg[0][0] == '.' && seg[0][1] == '.';
        last_dot = dot || dotdot;
        if (dot) continue;
        if (dotdot) {
            if (u8cssLen(u8csbData(segs)) > 0) u8cssShed(u8csbData(segs), 1);
            continue;
        }
        call(u8cssFeed1, u8csbIdle(segs), seg);
    }
    b8 trail = ends_slash || last_dot;

    u8g g;
    u8gOf(g, out);
    if (is_abs) call(u8gFeed1, g, '/');
    size_t n = u8cssLen(u8csbData(segs));
    for (size_t i = 0; i < n; i++) {
        if (i > 0) call(u8gFeed1, g, '/');
        u8cs const *s = u8cssAtP(u8csbData(segs), i);
        call(u8gFeed, g, *s);
    }
    if (trail && n > 0) call(u8gFeed1, g, '/');
    u8csMv(result, u8gLeftC(g));
    done;
}

// PTR-009: merge relative path with base path (RFC 3986 §5.2.3) then
// remove dot-segments, feeding the result into the CALLER-OWNED writable
// slice `out`; `result` views its written prefix, which the caller keeps
// live.  No static buffer, no pointer cursor — abc/PATH segment-walk only.
static ok64 URIMergePath(u8csp result, u8s out, u8csc base_path,
                         u8csc rel_path) {
    sane(result && $ok(out));

    // Relative path starts with '/': it's absolute, use it verbatim.
    if (!u8csEmpty(rel_path) && rel_path[0][0] == '/') {
        u8g g;
        u8gOf(g, out);
        call(u8gFeed, g, rel_path);
        u8csMv(result, u8gLeftC(g));
        done;
    }

    // Merge base "directory" (through and including the last '/') with
    // the relative path into BASS scratch, then remove dot-segments into
    // caller storage.  base_dir = base_path up to its last '/' (RFC
    // §5.2.3); empty when base has no '/'.
    a_carve(u8, merged, MAX_URI_LEN);
    u8cs base_dir = {base_path[0], base_path[0]};
    a_dup(u8c, scan, base_path);
    $for(u8c, c, scan) {
        if (*c == '/') base_dir[1] = c + 1;
    }
    if (!u8csEmpty(base_dir)) call(u8bFeed, merged, base_dir);
    call(u8bFeed, merged, rel_path);

    call(URIRemoveDots, result, out, u8bDataC(merged));
    done;
}

// Resolve relative URI against base to produce absolute URI.  PTR-009:
// the merged path is fed into the CALLER-OWNED writable slice `out`;
// `abs->path` may view its written prefix, so `out` must outlive every
// use of `abs`.
ok64 URIAbsolute(urip abs, uricp base, uricp rel, u8s out) {
    sane(abs && base && rel && $ok(out));
    zerop(abs);

    // If relative has scheme, it's already absolute
    if (!$empty(rel->scheme)) {
        *abs = *rel;
        done;
    }

    // Inherit scheme from base
    u8csDup(abs->scheme, base->scheme);

    // If relative has authority, use it
    if (!$empty(rel->authority) || !$empty(rel->host)) {
        u8csDup(abs->authority, rel->authority);
        u8csDup(abs->host, rel->host);
        u8csDup(abs->port, rel->port);
        u8csDup(abs->user, rel->user);
        u8csDup(abs->path, rel->path);
        u8csDup(abs->query, rel->query);
        u8csDup(abs->fragment, rel->fragment);
        done;
    }

    // Inherit authority from base
    u8csDup(abs->authority, base->authority);
    u8csDup(abs->host, base->host);
    u8csDup(abs->port, base->port);
    u8csDup(abs->user, base->user);

    // If relative has path, merge with base path.
    if (!$empty(rel->path)) {
        //  Round-trip identity for a path-noscheme reference (RFC 3986
        //  §5.3 recompose): when `rel` is a VERBATIM relative reference
        //  (no scheme, no authority, a rootless path) that still carries
        //  its own source text (`rel->data` set — the signature of a ref
        //  returned whole by URIRelative, as opposed to a path freshly
        //  built into caller scratch by URIRelativePath, which leaves
        //  `rel->data` empty), the reference IS the answer: preserve its
        //  rootless path instead of prepending base's directory.  This
        //  is what keeps `ht/h/p#abc` from gaining a spurious leading
        //  `/`.  A relativized rel (data empty) still merges normally so
        //  the table's `path2` → `/path2` resolution is unchanged.
        b8 verbatim_relref = rel->data[0] != NULL && $empty(rel->scheme) &&
            $empty(rel->authority) && $empty(rel->host) &&
            rel->path[0][0] != '/';
        if (verbatim_relref) {
            u8csDup(abs->path, rel->path);
        } else {
            call(URIMergePath, abs->path, out, base->path, rel->path);
        }
        u8csDup(abs->query, rel->query);
        u8csDup(abs->fragment, rel->fragment);
        done;
    }

    // Relative has no path - inherit from base
    u8csDup(abs->path, base->path);

    // Check if relative specifies query (non-NULL pointer = explicitly set)
    if (*rel->query) {
        // Non-empty means use that query, empty means explicitly no query
        if (!$empty(rel->query)) {
            u8csDup(abs->query, rel->query);
        }
        // else: abs->query stays empty (explicitly no query)
        // Fragment: use rel's if set, otherwise inherit from base
        if (*rel->fragment) {
            u8csDup(abs->fragment, rel->fragment);
        } else {
            u8csDup(abs->fragment, base->fragment);
        }
        done;
    }

    // No relative query info - inherit from base
    u8csDup(abs->query, base->query);

    // Fragment: use rel's if specified, otherwise inherit from base
    if (*rel->fragment) {
        u8csDup(abs->fragment, rel->fragment);
    } else {
        u8csDup(abs->fragment, base->fragment);
    }

    done;
}

// Check if character is unreserved per RFC 3986
fun b8 URIIsUnreserved(u8c c) {
    if (c >= 'A' && c <= 'Z') return YES;
    if (c >= 'a' && c <= 'z') return YES;
    if (c >= '0' && c <= '9') return YES;
    if (c == '-' || c == '.' || c == '_' || c == '~') return YES;
    return NO;
}

con u8c HEX_DIGITS[] = "0123456789ABCDEF";

// Percent-encode: all non-unreserved chars → %XX
ok64 URIu8sEsc(u8s into, u8cs raw) {
    sane($ok(into) && $ok(raw));
    $for(u8c, c, raw) {
        if (URIIsUnreserved(*c)) {
            call(u8sFeed1, into, *c);
        } else {
            test($len(into) >= 3, URIFAIL);
            call(u8sFeed1, into, '%');
            call(u8sFeed1, into, HEX_DIGITS[(*c >> 4) & 0xF]);
            call(u8sFeed1, into, HEX_DIGITS[*c & 0xF]);
        }
    }
    done;
}

// Hex digit to value, returns -1 if not hex
fun int URIHexVal(u8c c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// Percent-decode: %XX → raw bytes
ok64 URIu8sUnesc(u8s into, u8cs esc) {
    sane($ok(into) && $ok(esc));
    while (!$empty(esc)) {
        u8c c = *esc[0]++;
        if (c == '%' && $len(esc) >= 2) {
            int hi = URIHexVal(esc[0][0]);
            int lo = URIHexVal(esc[0][1]);
            if (hi >= 0 && lo >= 0) {
                call(u8sFeed1, into, (u8c)((hi << 4) | lo));
                esc[0] += 2;
                continue;
            }
        }
        call(u8sFeed1, into, c);
    }
    done;
}

ok64 URIMake(u8s into, u8cs scheme, u8cs auth, u8cs path, u8cs query, u8cs fragm) {
    sane($ok(into));
    uri u = {};
    if (scheme) u8csMv(u.scheme, scheme);
    if (auth) u8csMv(u.authority, auth);
    if (path) u8csMv(u.path, path);
    if (query) u8csMv(u.query, query);
    if (fragm) u8csMv(u.fragment, fragm);
    call(URIutf8Feed, into, &u);
    done;
}

// Return bitmask of which URI components are present (sigil seen
// in the source).  Per the URI lexer convention, a slot is present
// iff its data pointer is non-NULL — including bare `?` / `#` with
// an empty body.  Emptiness is a separate question; check the slot
// itself with u8csEmpty().
u8 URIPattern(uricp u) {
    u8 p = 0;
    if (u->scheme[0]    != NULL) p |= URI_SCHEME;
    if (u->authority[0] != NULL) p |= URI_AUTHORITY;
    if (u->user[0]      != NULL) p |= URI_USER;
    if (u->host[0]      != NULL) p |= URI_HOST;
    if (u->port[0]      != NULL) p |= URI_PORT;
    if (u->path[0]      != NULL) p |= URI_PATH;
    if (u->query[0]     != NULL) p |= URI_QUERY;
    if (u->fragment[0]  != NULL) p |= URI_FRAGMENT;
    return p;
}
