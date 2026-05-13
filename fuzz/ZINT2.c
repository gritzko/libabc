#include "ZINT.h"

#include "TEST.h"
#define MAX_LEN 1024

FUZZ(u64, ZINT2fuzz) {
    sane(1);
    if ($empty(input) || $len(input)>MAX_LEN) done;
    a_dup(u64c, src, input);
    aBpad(u8, pad, MAX_LEN * 9 + 8);
    u8sp into = u8bIdle(pad);
    call(ZINTu8sFeedBlocked, into, src);
    u8csp data = u8bDataC(pad);
    aBpad(u64, out, MAX_LEN);
    u64sp drain = u64bIdle(out);
    call(ZINTu8sDrainBlocked, data, drain);
    a_dup(u64c, cmp, u64bDataC(out));
    if ($len(cmp) != $len(input)) return FAILSANITY;
    $for(u64c, p, input) {
        if (*p != **cmp) return FAILSANITY;
        ++*cmp;
    }
    done;
}
