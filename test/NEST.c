//
// Created by gritzko on 8 Dec 24.
//
#include "NEST.h"

#include "PRO.h"
#include "TEST.h"

ok64 NESTtest1() {
    sane(1);
    aBpad(u8, ct, 128);
    NESTreset(ct);
    con ok64 mood = 0xc73ce8;
    a_cstr(templ, "Hello $mood world!");
    call(NESTFeed, ct, templ);
    call(NESTSplice, ct, mood);
    a_cstr(good, "beautiful");
    call(u8sFeed, NESTidle(ct), good);

    aBpad2(u8, res, 128);
    call(NESTRender, residle, ct);
    a_cstr(correct, "Hello beautiful world!");
    $testeq(correct, resdata);

    done;
}

ok64 NESTtest2() {
    sane(1);
    aBpad(u8, ct, 128);
    NESTreset(ct);
    con ok64 a = 0x25;
    a_cstr(templ, "1. $a 2. ${a} 3. a");
    call(NESTFeed, ct, templ);
    call(NESTSpliceAll, ct, a);
    a_cstr(good, "A");
    call(u8sFeed, NESTidle(ct), good);

    aBpad2(u8, res, 128);
    call(NESTRender, residle, ct);
    a_cstr(correct, "1. A 2. A 3. a");
    $testeq(correct, resdata);

    done;
}

// ABC-014: NESTFeed checked room once up-front, then copied literals
// without re-checking while NESTaddvar shrank idle[1] 16B/var.  A '$var'
// followed by enough literals overran idle into the stored mark128s.
// Post-fix the per-char room check must stop it with NESTNOROOM.
ok64 NESTtest3() {
    sane(1);
    aBpad(u8, ct, 48);
    NESTreset(ct);
    // "$a" adds a 16B mark (idle end drops 48->32), then 40 space literals
    // would run idle head to 40, smashing the mark; insert len 42 < 48 so
    // the single up-front check is fooled.  Spaces end the var name.
    a_pad(u8, tbuf, 64);
    u8 *tp = tbuf[0];
    *tp++ = '$';
    *tp++ = 'a';
    for (int i = 0; i < 40; ++i) *tp++ = ' ';
    u8cs templ = {tbuf[0], tp};
    __ = NESTFeed(ct, templ);
    test(__ == NESTNOROOM, NESTBAD);  // must refuse, not overrun
    __ = OK;
    done;
}

ok64 NESTtest() {
    sane(1);
    call(NESTtest1);
    call(NESTtest2);
    call(NESTtest3);
    done;
}

TEST(NESTtest);
