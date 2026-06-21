#ifndef ABC_BIT_H
#define ABC_BIT_H

//  ABC-004: u1 bitmap — bit addressing layered over the real u64 word
//  families (INT.h).  NO bespoke struct: a bit map *is* a `u64s`/`u64cs`
//  view or an owned word buffer `u1b` (== `u64b`).  Bit i lives in word
//  i>>6, bit i&63 (LSB-first in the LE word); set algebra is word-
//  parallel; iteration is ctz-based.  u1sClr is `&= ~mask` (the old
//  BUF.h `|= ~(1<<bit)` clear bug cannot be expressed here).

#include "01.h"
#include "INT.h"

//  Length mismatch in equal-length set algebra (Or/And/AndNot/Xor/Eq).
con ok64 BITLEN = 0x2d2755397;

//  A bit VALUE; there is no addressable u1*, a bit rides in a b8 (0/1).
typedef b8 u1;
typedef u64s  u1s;    //  bit-map view (writable)
typedef u64cs u1cs;   //  bit-map view (const)
typedef u64b  u1b;    //  owned bit map; DATA cursor counts whole WORDS

//  u64 words needed to hold `bits` bits.
fun u32 u1Words(u32 bits) { return (bits + 63) >> 6; }

//  Const view aliasing a writable map, for the readers below.
#define u1sConst(s) ((u64cs){(s)[0], (s)[1]})

//  ---- bit element access over a word slice ----------------------------
fun u1 u1At(u64csc s, u32 i) {
    return (i >> 6) < (u32)u64csLen(s)
             ? (u1)((s[0][i >> 6] >> (i & 63u)) & 1u)
             : 0;
}
fun void u1sSet(u64s s, u32 i) {
    if ((i >> 6) < (u32)u64sLen(s)) s[0][i >> 6] |= (u64)1 << (i & 63u);
}
fun void u1sClr(u64s s, u32 i) {
    if ((i >> 6) < (u32)u64sLen(s)) s[0][i >> 6] &= ~((u64)1 << (i & 63u));
}
fun void u1sPut(u64s s, u32 i, u1 v) { v ? u1sSet(s, i) : u1sClr(s, i); }

//  Addressable bits = words * 64.
fun u32 u1sLen(u64csc s) { return (u32)u64csLen(s) << 6; }

//  ---- word-parallel set algebra (equal WORD length, else BITLEN) ------
fun ok64 u1sOr(u64s d, u64csc s) {
    if (u64sLen(d) != u64csLen(s)) return BITLEN;
    for (size_t i = 0; i < u64sLen(d); i++) d[0][i] |= s[0][i];
    return OK;
}
fun ok64 u1sAnd(u64s d, u64csc s) {
    if (u64sLen(d) != u64csLen(s)) return BITLEN;
    for (size_t i = 0; i < u64sLen(d); i++) d[0][i] &= s[0][i];
    return OK;
}
fun ok64 u1sAndNot(u64s d, u64csc s) {
    if (u64sLen(d) != u64csLen(s)) return BITLEN;
    for (size_t i = 0; i < u64sLen(d); i++) d[0][i] &= ~s[0][i];
    return OK;
}
fun ok64 u1sXor(u64s d, u64csc s) {
    if (u64sLen(d) != u64csLen(s)) return BITLEN;
    for (size_t i = 0; i < u64sLen(d); i++) d[0][i] ^= s[0][i];
    return OK;
}

fun u32 u1sCount(u64csc s) {
    u32 c = 0;
    for (size_t i = 0; i < u64csLen(s); i++)
        c += (u32)__builtin_popcountll(s[0][i]);
    return c;
}
fun b8 u1sAny(u64csc s) {
    for (size_t i = 0; i < u64csLen(s); i++)
        if (s[0][i]) return YES;
    return NO;
}

//  Next set bit at/after `from`; returns u1sLen(s) when none (u1$for end).
fun u32 u1sNext(u64csc s, u32 from) {
    u32 nw = (u32)u64csLen(s), end = nw << 6;
    if (from >= end) return end;
    u64 w = s[0][from >> 6] >> (from & 63u);
    if (w) return from + (u32)__builtin_ctzll(w);
    for (u32 i = (from >> 6) + 1; i < nw; i++)
        if (s[0][i]) return (i << 6) + (u32)__builtin_ctzll(s[0][i]);
    return end;
}
#define u1$for(i, set)                                                    \
    for (u32 i = u1sNext((set), 0), i##__e = u1sLen(set); i < i##__e;      \
         i = u1sNext((set), i + 1))

fun b8 u1sEq(u64csc a, u64csc b) {
    if (u64csLen(a) != u64csLen(b)) return NO;
    for (size_t i = 0; i < u64csLen(a); i++)
        if (a[0][i] != b[0][i]) return NO;
    return YES;
}

//  ---- owned bit map: a u64b whose DATA spans the whole (zeroed) map; ---
//  Map/Acquire take BITS, round up to whole words, present them all.
fun ok64 u1bAllocate(u64b b, u32 bits) {
    ok64 o = u64bAllocate(b, u1Words(bits));    //  heap, zero-filled
    return o == OK ? u64bFed(b, u1Words(bits)) : o;
}
fun ok64 u1bMap(u64b b, u32 bits) {
    ok64 o = u64bMap(b, u1Words(bits));         //  mmap zero-fills
    return o == OK ? u64bFed(b, u1Words(bits)) : o;
}
fun ok64 u1bAcquire(u8a arena, u64b b, u32 bits) {
    u32 nw = u1Words(bits);
    ok64 o = u64bAcquire(arena, b, nw);
    if (o != OK) return o;
    u64bZero(b);                                //  arena memory may be dirty
    return u64bFed(b, nw);
}
fun ok64 u1bUnMap(u64b b) { return u64bUnMap(b); }
fun ok64 u1bFree(u64b b) { return u64bFree(b); }
fun void u1bReset(u64b b) { u64bZero(b); }      //  keep size, clear bits
fun u64sp u1bData(u64b b) { return u64bData(b); }    //  use as u64s
fun u64csp u1bDataC(u64b b) { return u64bDataC(b); } //  use as u64cs
#endif
