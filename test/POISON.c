//  ABC-26B08Hw_ki: ASan sees inside arenas.  No argument: legit carve,
//  rewind and gauge reuse, no reports in any build.  `over`, `rewind`,
//  `mark`, `past`: one deliberate bad access each; CMake registers them
//  under ASan only, expecting `use-after-poison`.
#include "PRO.h"

#include <string.h>

static volatile u8 sink;

ok64 carve13(u8 **out) {
    sane(out);
    a_carve(u8, c, 13);
    u8bFeed1(c, 'x');
    *out = c[0];
    done;
}

ok64 clean() {
    sane(1);
    u8 *p = NULL;
    for (int i = 0; i < 3; ++i) call(carve13, &p);
    a_lign(u8, g);  //  a gauge over the IDLE the calls popped
    for (int i = 0; i < 100; ++i) test(OK == u8gFeed1(g, 'y'), FAILSANITY);
    a_cquire(u8, s);
    test($len(s) == 100 && s[0][99] == 'y', FAILSANITY);
    u8 *mark = u8aMark(ABC_BASS);
    a_carve(u8, a, 13);
    memset(a[0], 'z', 13);  //  the whole carve is writable
    u8aRewind(ABC_BASS, mark);
    a_carve(u8, b, 13);
    memset(b[0], 'w', 13);
    a_ren(r, u8bDataC(b));
    done;
}

ok64 over() {  //  overrun into the next carve
    sane(1);
    a_carve(u8, a, 13);
    a_carve(u8, b, 8);
    (void)b;
    a[0][13] = 1;
    done;
}

ok64 rewind_() {  //  use after a call() rewind
    sane(1);
    u8 *p = NULL;
    call(carve13, &p);
    sink = *p;
    done;
}

ok64 mark() {  //  use after a u8aRewind
    sane(1);
    u8 *m = u8aMark(ABC_BASS);
    a_carve(u8, a, 16);
    u8aRewind(ABC_BASS, m);
    sink = a[0][0];
    done;
}

ok64 past() {  //  read past a carve's end
    sane(1);
    a_carve(u8, a, 5);
    sink = a[3][0];
    done;
}

b8 named(u8csc mode, char const *name) {
    u8cs n = $u8str(name);
    return u8csEq(mode, n);
}

ok64 poison() {
    sane(1);
    if ($arglen < 2) return clean();
    a$rg(mode, 1);
    if (named(mode, "over")) return over();
    if (named(mode, "rewind")) return rewind_();
    if (named(mode, "mark")) return mark();
    if (named(mode, "past")) return past();
    fail(BADARG);
}

MAIN(poison);
