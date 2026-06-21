//  ABC-004: fuzz abc/BIT.h word-parallel set algebra (Or/And/AndNot/
//  Xor/Count/Next/Eq/Any) against a simple byte-per-bit reference set
//  model.  Input bytes drive bit indices into two equal-length maps.
//  A u1 map is a u64 word buffer; lengths are whole words.

#include "01.h"
#include "BIT.h"
#include "PRO.h"
#include "TEST.h"

#define NBITS 320  // 5 whole words

FUZZ(u8, BITfuzz) {
    sane(1);
    size_t n = $len(input);

    u1b ba = {}, bb = {};
    must(u1bMap(ba, NBITS) == OK, "map a");
    must(u1bMap(bb, NBITS) == OK, "map b");
    u64s a; u64sDup(a, u1bData(ba));
    u64s b; u64sDup(b, u1bData(bb));

    //  Reference: byte-per-bit.  Even-index input bytes set bits in `a`,
    //  odd-index in `b` (index = byte % NBITS).
    u8 ra[NBITS] = {}, rb[NBITS] = {};
    for (size_t i = 0; i < n; i++) {
        u32 ix = (u32)(input[0][i] | ((i & 1) ? 0x100u : 0u)) % NBITS;
        if (i & 1) { u1sSet(b, ix); rb[ix] = 1; }
        else       { u1sSet(a, ix); ra[ix] = 1; }
    }

    //  Count + Any match the model.
    {
        u32 ca = 0, cb = 0;
        for (int i = 0; i < NBITS; i++) { ca += ra[i]; cb += rb[i]; }
        must(u1sCount(u1sConst(a)) == ca, "count a");
        must(u1sCount(u1sConst(b)) == cb, "count b");
        must(u1sAny(u1sConst(a)) == (ca != 0), "any a");
    }

    //  Next iteration visits exactly the set bits of `a`, in order.
    {
        u32 prev = 0, seen = 0;
        u1$for (ix, u1sConst(a)) {
            must(ra[ix], "next hit not in ref");
            must(ix >= prev, "next not monotone");
            prev = ix + 1; seen++;
        }
        u32 ca = 0; for (int i = 0; i < NBITS; i++) ca += ra[i];
        must(seen == ca, "next count mismatch");
    }

    //  Algebra: seed a fresh dst with `a`, apply op, compare to ref.
#define CHECK_OP(opfn, expr)                                             \
    do {                                                                 \
        u1b bd = {};                                                     \
        must(u1bMap(bd, NBITS) == OK, "map d");                          \
        u64s d; u64sDup(d, u1bData(bd));                                 \
        must(u1sOr(d, u1sConst(a)) == OK, "seed d = a");                 \
        must(opfn(d, u1sConst(b)) == OK, "op");                          \
        for (int i = 0; i < NBITS; i++) {                                \
            u8 want = (u8)(expr);                                        \
            must((u8)u1At(u1sConst(d), i) == want, "op vs ref");         \
        }                                                                \
        must(u1bUnMap(bd) == OK, "unmap d");                            \
    } while (0)

    CHECK_OP(u1sOr,     ra[i] | rb[i]);
    CHECK_OP(u1sAnd,    ra[i] & rb[i]);
    CHECK_OP(u1sAndNot, (u8)(ra[i] & ~rb[i] & 1));
    CHECK_OP(u1sXor,    ra[i] ^ rb[i]);
#undef CHECK_OP

    //  Eq: a copy equals, a tweaked one differs.
    {
        u1b bc = {};
        must(u1bMap(bc, NBITS) == OK, "map c");
        u64s c; u64sDup(c, u1bData(bc));
        must(u1sOr(c, u1sConst(a)) == OK, "copy a");
        must(u1sEq(u1sConst(c), u1sConst(a)), "eq copy");
        u1sPut(c, 0, !u1At(u1sConst(c), 0));
        must(!u1sEq(u1sConst(c), u1sConst(a)), "eq must differ after flip");
        must(u1bUnMap(bc) == OK, "unmap c");
    }

    must(u1bUnMap(ba) == OK, "unmap a");
    must(u1bUnMap(bb) == OK, "unmap b");
    done;
}
