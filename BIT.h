#ifndef ABC_BIT_H
#define ABC_BIT_H

//  ABC-004: u1 bitmap — a bit VALUE (`u1`), an offset-0 bit-slice
//  (`u1s`/`u1cs`, bespoke `{u8* base; u32 nbits}`, NOT an Sx.h/Bx.h
//  instance), an owned bit buffer (`u1b`, u64-word backed, DATA cursor
//  in BITS), and word-parallel set algebra.  Replaces ad-hoc
//  BitAt/BitSet/BitUnset over a raw u8b (abc/BUF.h) and kills the
//  BitUnset `|= ~(1<<bit)` bug by construction (no broken clear exists).

#include "01.h"
#include "B.h"
#include "INT.h"

//  Length mismatch in equal-length set algebra (Or/And/AndNot/Xor/Eq).
con ok64 BITLEN = 0x2d2755397;

//  ---- u1: a bit value -------------------------------------------------
//  There is no addressable `u1*`; a bit is carried as a b8 (0/1).
typedef b8 u1;

//  ---- u1s / u1cs: an offset-0 bit-slice -------------------------------
//  Bespoke struct, NOT a typed-slice instantiation.  `base` points at
//  u64-word-aligned backing (the u1b buffer guarantees this); `nbits`
//  counts addressable bits from offset 0.  Word-parallel ops read whole
//  LE u64 words off `base`; bit i lives in word i>>6, bit i&63 (LSB-first
//  within the LE word — matches the old byte-addressed BitAt).
typedef struct u1s_s {
    u8 *base;
    u32 nbits;
} u1s;

typedef struct u1cs_s {
    u8 const *base;
    u32 nbits;
} u1cs;

//  ---- u1b: an owned bit buffer ----------------------------------------
//  u64-word backing via the Bu64 family (INT.h); `nbits` is the DATA
//  cursor counted in BITS.  Word capacity comes from the backing buffer.
typedef struct u1b_s {
    Bu64 words;
    u32 nbits;
} u1b;

//  ---- internal word helpers (no pointer arithmetic on user data) ------
//  Whole-word view over a bit-slice's backing as a u64 slice spanning
//  ceil(nbits/64) words; the tail word's junk bits are masked by callers
//  that need it (Count/Or/And/...).
fun u32 u1WordsFor(u32 nbits) { return (u32)((nbits + 63) >> 6); }

//  Mask of the live low bits in the final (partial) word: bits [0, r)
//  where r = nbits & 63; a full final word (r == 0, nbits > 0) keeps all.
fun u64 u1TailMask(u32 nbits) {
    u32 r = nbits & 63u;
    return r ? (((u64)1 << r) - 1) : (u64)~(u64)0;
}

fun u1cs u1sConst(u1s s) {
    u1cs c = {s.base, s.nbits};
    return c;
}

//  ---- element access + length ----------------------------------------
fun u1 u1At(u1cs s, u32 i) {
    if (i >= s.nbits) return 0;
    u64 const *w = (u64 const *)s.base;
    return (u1)((w[i >> 6] >> (i & 63u)) & 1u);
}
fun void u1sSet(u1s s, u32 i) {
    if (i >= s.nbits) return;
    u64 *w = (u64 *)s.base;
    w[i >> 6] |= ((u64)1 << (i & 63u));
}
fun void u1sClr(u1s s, u32 i) {
    if (i >= s.nbits) return;
    u64 *w = (u64 *)s.base;
    w[i >> 6] &= ~((u64)1 << (i & 63u));
}
fun void u1sPut(u1s s, u32 i, u1 v) { v ? u1sSet(s, i) : u1sClr(s, i); }
fun u32 u1sLen(u1cs s) { return s.nbits; }

//  ---- buffer family (mirrors Bx.h naming) -----------------------------
//  Heap-backed: `cap_bits` rounds up to whole u64 words; cleared to 0.
fun ok64 u1bAllocate(u1b *b, size_t cap_bits) {
    b->nbits = 0;
    return u64bAllocate(b->words, u1WordsFor((u32)cap_bits));
}
fun ok64 u1bMap(u1b *b, size_t cap_bits) {
    b->nbits = 0;
    return u64bMap(b->words, u1WordsFor((u32)cap_bits));
}
fun ok64 u1bFree(u1b *b) {
    b->nbits = 0;
    return u64bFree(b->words);
}
fun ok64 u1bUnMap(u1b *b) {
    b->nbits = 0;
    return u64bUnMap(b->words);
}
fun ok64 u1bAcquire(u8a arena, u1b *b, size_t cap_bits) {
    b->nbits = 0;
    return u64bAcquire(arena, b->words, u1WordsFor((u32)cap_bits));
}

//  Total bit capacity = whole-word capacity of the backing buffer.
fun u32 u1bLen(u1b const *b) {
    return (u32)((u64)((u8 *)b->words[3] - (u8 *)b->words[0]) << 3);
}
fun u32 u1bDataLen(u1b const *b) { return b->nbits; }
fun b8 u1bIdle(u1b const *b) { return b->nbits >= u1bLen(b); }
fun b8 u1bOK(u1b const *b) { return Bok((void *const *)b->words); }

//  Reset DATA to empty: zero the live words, drop the bit cursor.
fun void u1bReset(u1b *b) {
    u32 nw = u1WordsFor(b->nbits);
    u64 *w = (u64 *)b->words[0];
    for (u32 i = 0; i < nw; i++) w[i] = 0;
    b->nbits = 0;
}

//  Append one bit, growing the DATA cursor; zeros any freshly-exposed
//  word so junk never leaks.  BITNOROOM when capacity is exhausted.
fun ok64 u1bFeed1(u1b *b, u1 v) {
    if (b->nbits >= u1bLen(b)) return BNOROOM;
    u32 i = b->nbits;
    u64 *w = (u64 *)b->words[0];
    if ((i & 63u) == 0) w[i >> 6] = 0;
    if (v) w[i >> 6] |= ((u64)1 << (i & 63u));
    b->nbits = i + 1;
    return OK;
}

//  Views onto the live DATA region as bit-slices.
fun u1s u1bData(u1b *b) {
    u1s s = {(u8 *)b->words[0], b->nbits};
    return s;
}
fun u1cs u1bDataC(u1b const *b) {
    u1cs s = {(u8 const *)b->words[0], b->nbits};
    return s;
}

//  ---- word-parallel set algebra (equal-length, tail-masked) -----------
//  dst |= src ; dst, src must be the same length (BITLEN otherwise).
fun ok64 u1sOr(u1s dst, u1cs src) {
    if (dst.nbits != src.nbits) return BITLEN;
    u64 *d = (u64 *)dst.base;
    u64 const *s = (u64 const *)src.base;
    u32 nw = u1WordsFor(dst.nbits);
    for (u32 i = 0; i < nw; i++) d[i] |= s[i];
    return OK;
}
fun ok64 u1sAnd(u1s dst, u1cs src) {
    if (dst.nbits != src.nbits) return BITLEN;
    u64 *d = (u64 *)dst.base;
    u64 const *s = (u64 const *)src.base;
    u32 nw = u1WordsFor(dst.nbits);
    for (u32 i = 0; i < nw; i++) d[i] &= s[i];
    return OK;
}
fun ok64 u1sAndNot(u1s dst, u1cs src) {
    if (dst.nbits != src.nbits) return BITLEN;
    u64 *d = (u64 *)dst.base;
    u64 const *s = (u64 const *)src.base;
    u32 nw = u1WordsFor(dst.nbits);
    for (u32 i = 0; i < nw; i++) d[i] &= ~s[i];
    return OK;
}
fun ok64 u1sXor(u1s dst, u1cs src) {
    if (dst.nbits != src.nbits) return BITLEN;
    u64 *d = (u64 *)dst.base;
    u64 const *s = (u64 const *)src.base;
    u32 nw = u1WordsFor(dst.nbits);
    for (u32 i = 0; i < nw; i++) d[i] ^= s[i];
    return OK;
}

//  Population count (rank): set bits in [0, nbits); tail word masked so
//  junk bits past nbits never count.  Uses 64-bit popcount directly
//  (01.h's popc64 macro is a 32-bit __builtin_popcount — see ABC-004).
fun u32 u1sCount(u1cs s) {
    u64 const *w = (u64 const *)s.base;
    u32 nw = u1WordsFor(s.nbits);
    if (nw == 0) return 0;
    u32 c = 0;
    for (u32 i = 0; i + 1 < nw; i++) c += (u32)__builtin_popcountll(w[i]);
    c += (u32)__builtin_popcountll(w[nw - 1] & u1TailMask(s.nbits));
    return c;
}

//  Index of the next set bit at or after `from`; returns nbits when none
//  remain (the natural loop terminator for u1$for).
fun u32 u1sNext(u1cs s, u32 from) {
    if (from >= s.nbits) return s.nbits;
    u64 const *w = (u64 const *)s.base;
    u32 nw = u1WordsFor(s.nbits);
    u32 wi = from >> 6;
    u64 word = w[wi] >> (from & 63u);
    if (word) return from + (u32)__builtin_ctzll(word);
    for (u32 i = wi + 1; i < nw; i++) {
        if (w[i]) {
            u32 hit = (i << 6) + (u32)__builtin_ctzll(w[i]);
            return hit < s.nbits ? hit : s.nbits;
        }
    }
    return s.nbits;
}

//  Iterate set bits: `u1$for (i, set) { ... }`.
#define u1$for(i, set)                                                   \
    for (u32 i = u1sNext((set), 0); i < (set).nbits;                     \
         i = u1sNext((set), i + 1))

//  Equality over equal-length maps (tail-masked); NO for length mismatch.
fun b8 u1sEq(u1cs a, u1cs b) {
    if (a.nbits != b.nbits) return NO;
    u64 const *wa = (u64 const *)a.base;
    u64 const *wb = (u64 const *)b.base;
    u32 nw = u1WordsFor(a.nbits);
    if (nw == 0) return YES;
    for (u32 i = 0; i + 1 < nw; i++)
        if (wa[i] != wb[i]) return NO;
    u64 m = u1TailMask(a.nbits);
    return (wa[nw - 1] & m) == (wb[nw - 1] & m);
}

//  Any bit set (tail-masked).
fun b8 u1sAny(u1cs s) {
    u64 const *w = (u64 const *)s.base;
    u32 nw = u1WordsFor(s.nbits);
    if (nw == 0) return NO;
    for (u32 i = 0; i + 1 < nw; i++)
        if (w[i]) return YES;
    return (w[nw - 1] & u1TailMask(s.nbits)) != 0;
}

#endif
