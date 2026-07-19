#ifndef ABC_SORT_H
#define ABC_SORT_H
#include "INT.h"

con ok64 SORTNOROOM = 0x7186dd5d86d8616;
con ok64 SORTNODATA = 0x7186dd5d834a74a;

fun ok64 SORTu64x(u8cs s, u8cs rest) { return $u8take(s, rest, sizeof(u64)); }

// ABC-021: a sort keeps duplicates -- feed every tied element
fun ok64 SORTu64y($u8 into, u8css eqs) {
    ok64 o = OK;
    for (u8cs *e = eqs[0]; o == OK && e != eqs[1]; ++e)
        o = u8sFeed(into, (u8c$c)e);
    return o;
}

fun b8 SORTu64z($cu8c *a, $cu8c *b) { return u64Z((u64c *)**a, (u64c *)**b); }

#define _X(name) SORTu64##name
#include "Yx.h"
#undef _X

ok64 SORTu64($u64 into, $u64 from);

#endif
