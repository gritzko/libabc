// URI serialization-safety fuzz: parse an arbitrary URI, then assert
// URIutf8FeedSafe re-emits it as a URI that re-parses to the SAME 8
// components — or explicitly refuses with URIUNSAFE.  It must NEVER
// silently emit a URI that means something different (cross-component
// injection).  This is the URI-003 invariant; minimized crashes feed
// back into abc/test/URI.c URITestFeedSafe.
#include "URI.h"

#include "PRO.h"
#include "TEST.h"

FUZZ(u8, URIfuzz) {
    sane(1);

    // Skip inputs that are too large or empty
    if ($len(input) > 1024) done;
    if ($len(input) < 5) done;

    // Input must look like a URI (start with scheme-like chars)
    u8c first = *input[0];
    if (first < 'a' || first > 'z') done;

    // Parse a URI from the fuzz input.
    uri u = {};
    ok64 o = URIutf8Drain(input, &u);
    if (o != OK) done;  // Invalid URI is fine, just skip

    // Serialize with the safety guarantee.  Two legal outcomes:
    //   1. URIUNSAFE  -> a component carries a foreign delimiter; the
    //      serializer refused and emitted nothing.  Acceptable.
    //   2. OK         -> the emitted text MUST re-parse to the same 8
    //      components.  Verify that here (FeedSafe already checked it,
    //      but re-prove it independently so a regression in FeedSafe's
    //      own comparison cannot hide a real injection).
    a_pad(u8, out, MAX_URI_LEN);
    o = URIutf8FeedSafe(out_idle, &u);
    if (o == URIUNSAFE) done;        // explicit reject — fine
    if (o != OK) return FAILSANITY;  // no other error is expected here

    uri re = {};
    a_dup(u8c, txt, u8bDataC(out));
    if (URIutf8Drain(txt, &re) != OK) return FAILSANITY;

    if (!$eq(re.scheme, u.scheme)) return FAILSANITY;
    if (!$eq(re.authority, u.authority)) return FAILSANITY;
    if (!$eq(re.user, u.user)) return FAILSANITY;
    if (!$eq(re.host, u.host)) return FAILSANITY;
    if (!$eq(re.port, u.port)) return FAILSANITY;
    if (!$eq(re.path, u.path)) return FAILSANITY;
    if (!$eq(re.query, u.query)) return FAILSANITY;
    if (!$eq(re.fragment, u.fragment)) return FAILSANITY;

    done;
}
