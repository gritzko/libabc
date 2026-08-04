// MEM-021 repro: DNSNameText ignored the label `u8sFeed` return, so a
// short `into` silently dropped whole labels and still reported OK —
// the caller got a truncated name it could not tell from the real one.

#include "DNS.h"

#include "PRO.h"
#include "TEST.h"

// wire form of "www.abc": 3 w w w 3 a b c 0
static u8 const wwwabc[] = {3, 'w', 'w', 'w', 3, 'a', 'b', 'c', 0};

ok64 DNStextRoom() {
    sane(1);
    a$(u8c, wire, wwwabc);

    // ample room: the whole name decodes
    a_pad(u8, big, 64);
    call(DNSNameText, big_idle, wire);
    a_cstr(full, "www.abc");
    want(u8csEq(big_datac, full));

    // 5 bytes fit "www" and the dot, but not the "abc" label.  The old
    // code fed the label, dropped the NOROOM and returned OK with "www."
    a_pad(u8, small, 5);
    same(DNSNameText(small_idle, wire), DNSNOROOM);
    done;
}

ok64 DNStextTest() {
    sane(1);
    call(DNStextRoom);
    done;
}

TEST(DNStextTest);
