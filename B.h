#ifndef LIBRDX_B_h
#define LIBRDX_B_h
#define _BSD_SOURCE 1
#define _DEFAULT_SOURCE 1
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "01.h"
#include "OK.h"
#include "S.h"

#define MMAPFAIL BNOMEM  // CODE-26B068XgTv: one-release alias
con ok64 BNOTNULL = 0x2d761d5de555;
con ok64 BISNULL = 0xb49c5de555;
con ok64 BNOROOM = 0xb5d86d8616;
con ok64 BBADARG = 0xb2ca34a6d0;
con ok64 BFAIL = 0xb3ca495;
con ok64 BNODATA = 0xb5d834a74a;
con ok64 BNOMEM = 0x2d7616396;

#define B_MAX_LEN_BITS 48
#define B_MAX_LEN (1ULL << B_MAX_LEN_BITS)

#define BTYPE(T) typedef T *const B##T[4];

BTYPE(void);
typedef void *voidb[4];
typedef void **voidbp;

//  u8a — an *arena*: same 4-pointer shape as Bu8, but used as a
//  byte dispenser for `u8bAcquire`.  Arena use is strictly along
//  the call tree, strictly LIFO — see `abc/B.md` §Arenas.  The
//  type is interchangeable with Bu8 at the ABI level; the name
//  signals intent (this buffer dispenses, not stores).
typedef u8 *u8a[4];

//  Save the arena's current dispense point (= IdleHead).  Pair with
//  u8aRewind to pop everything Acquired since the mark.
fun u8 *u8aMark(u8a arena) { return arena[2]; }

//  Pop all bytes Acquired since `mark`; arena is ready to dispense
//  again from `mark` forward.  Any child `Bu8` Acquired between
//  the Mark and this Rewind is now invalid — do not touch it.
fun void u8aRewind(u8a arena, u8 *mark) {
    arena[1] = mark;
    arena[2] = mark;
}

//  Full reset — pop everything ever Acquired from this arena.
//  Equivalent to u8aRewind back to arena[0].
fun void u8aReset(u8a arena) {
    arena[1] = arena[0];
    arena[2] = arena[0];
}

#define B(T, b) B##T b = {0, 0, 0, 0}

#define Blen(b) (b[3] - b[0])
#define Bsize(b) ((u8 *)(b[3]) - (u8 *)(b[0]))
#define BDataSize(b) ((u8 *)(b[2]) - (u8 *)(b[1]))
#define Busysize(b) ((u8 const *)b[2] - (u8 const *)b[0])
#define Bpastlen(b) $len((b) + 0)
#define Bdatalen(b) $len((b) + 1)
#define Bidlelen(b) $len((b) + 2)
#define Bempty(b) $empty((b) + 1)

#define Bok(b) \
    (b != NULL && b[0] != NULL && b[0] <= b[1] && b[1] <= b[2] && b[2] <= b[3])
#define BNULL(b) (b == NULL || b[0] == NULL)

#define aBpad(T, n, l) \
    T _##n[(l)] = {0}; \
    B##T n = {_##n, _##n, _##n, _##n + (l)};

// ABC-017: aux views are macro API; tag unused so -Wall stays clean
#define aBpad2(T, n, l)                                        \
    T _##n[(l)] = {0};                                         \
    B##T n##buf = {_##n, _##n, _##n, _##n + (l)};              \
    T##$ n##idle __attribute__((unused)) = T##bIdle(n##buf);   \
    T##$ n##data __attribute__((unused)) = T##bData(n##buf);

// a_wrap: an empty buffer `n` over a preexisting fixed array `arr` (a struct
// member, a local), its whole length idle, plus the `n_idle` / `n_data` /
// `n_datac` slices; fill it with checked feeds.  `arr` must be an array, not
// a pointer.  No token pasting, so T may be a template type (X(, cs)).
// a_pad(T, n, l) is the same over a fresh stack array T _n[l].
#define a_wrap(T, n, arr)                                                  \
    static_assert(!__builtin_types_compatible_p(typeof(arr),               \
                                                typeof(&*(arr))));         \
    T *n[4] = {(arr), (arr), (arr), (arr) + sizeof(arr) / sizeof(*(arr))}; \
    T **n##_idle __attribute__((unused)) = n + 2;                          \
    T **n##_data __attribute__((unused)) = n + 1;                          \
    T const **n##_datac __attribute__((unused)) = (T const **)n + 1;

#define a_pad(T, n, l) \
    T _##n[(l)];       \
    a_wrap(T, n, _##n)

#define a_pad0(T, n, l) \
    a_pad(T, n, l);     \
    zerob(n);

// The BASS-implicit `a_lign` / `a_cquire` / `a_rent` / `a_ren` /
// `a_carve` macros live in PRO.h and act on ABC_BASS.  Use those
// for the common case.
//
// `b_lign` / `b_cq` / `b_rent` / `b_ren` are the explicit-arena
// variants — same shape, plus an `arena` argument naming the u8
// buffer to dispense from.  Use them when you need a buffer other
// than BASS as the arena (e.g. a per-session arena passed down).
#define b_lign(T, gauge, arena) T##gp gauge = T##bAlign(arena)

#define b_cq(T, slice, arena) \
    T##cs slice = {};         \
    T##bAcq(arena, slice)

#define b_rent(T, news, arena, src) \
    T##cs news = {};                \
    call(T##bAren, arena, news, src)

#define b_ren(news, arena, src) b_rent(u8, news, arena, src)

#define aBcpad(T, n, l)                                        \
    T _##n[(l)];                                               \
    B##T n##buf = {_##n, _##n, _##n, _##n + (l)};              \
    T##$ n##idle __attribute__((unused)) = T##bIdle(n##buf);   \
    T##c##$ n##data __attribute__((unused)) = T##bDataC(n##buf);

#define aB(T, name)                                            \
    T *name##buf[4] = {};                                      \
    T **name##data __attribute__((unused)) = name##buf + 1;    \
    T const **name##cdata __attribute__((unused)) =            \
        (T const **)name##buf + 1;                             \
    T **name##idle __attribute__((unused)) = name##buf + 2;

#define zerob(buf) memset(buf[0], 0, Bsize(buf))

#define Bwithin(b, s) (s[0] >= b[0] && s[1] <= b[3])
// Can a byte buffer be seen as a buffer of T (FUZZ-26B04Iucge).
#define Bfits(T, b)                                                  \
    ((uintptr_t)(b)[0] % alignof(T) == 0 &&                          \
     ((char const *)(b)[1] - (char const *)(b)[0]) % sizeof(T) == 0 && \
     ((char const *)(b)[2] - (char const *)(b)[0]) % sizeof(T) == 0 && \
     ((char const *)(b)[3] - (char const *)(b)[0]) % sizeof(T) == 0)

fun void _Brebase(Bvoid buf, void *newhead, size_t newlen) {
    u8 **b = (u8 **)buf;
    size_t data = b[1] - b[0];
    if (data > newlen) data = newlen;
    size_t idle = b[2] - b[0];
    if (idle > newlen) idle = newlen;
    b[0] = (u8 *)newhead;
    b[1] = b[0] + data;
    b[2] = b[0] + idle;
    b[3] = b[0] + newlen;
}

fun ok64 Balloc(Bvoid b, size_t sz) {
    if (!BNULL(b)) return BNOTNULL;
    u8 *p = (u8 *)malloc(sz);
    if (p == NULL) return BNOMEM;
    u8 **buf = (u8 **)b;
    buf[0] = buf[1] = buf[2] = p;
    buf[3] = p + sz;
    return OK;
}

fun ok64 Brealloc(Bvoid b, size_t sz) {
    void *m = realloc(b[0], sz);
    if (m == NULL) return BNOMEM;
    _Brebase(b, m, sz);
    return OK;
}

fun ok64 Breserve(Bvoid b, size_t sz) {
    size_t i = $size(b + 2);
    if (i >= sz) return OK;
    return Brealloc(b, roundup2(Busysize(b) + sz, 256));
}

#define BNULLify(buf) memset((void **)buf, 0, sizeof(Bvoid));

fun ok64 Bfree(Bvoid buf) {
    if (BNULL(buf)) return BISNULL;
    free(buf[0]);
    BNULLify(buf);
    return OK;
}

#define Back(b)             \
    {                       \
        u8 **bb = (u8 **)b; \
        bb[2] = bb[1];      \
    }

#define Bate(b)              \
    {                        \
        u8c **v = (u8c **)b; \
        v[1] = v[2];         \
    }

#define Breset(buf)          \
    {                        \
        u8 **b = (u8 **)buf; \
        b[1] = b[2] = b[0];  \
    }

#define Breset1(buf)         \
    {                        \
        u8 **b = (u8 **)buf; \
        b[1] = b[0];         \
        b[2] = b[3];         \
    }

#define Batp(buf, ndx) (buf[0] + ndx)
#define Bat(buf, ndx) (buf[0][(ndx)])

typedef struct {
    u64 from;
    u64 till;
} range64;

// Byte offset range [lo, hi)
typedef struct { u32 lo; u32 hi; } range32;

fun b8 range32Z(range32 const *a, range32 const *b) {
    if (a->lo != b->lo) return a->lo < b->lo;
    return a->hi < b->hi;
}
con range32 range32Max = {u32max, u32max};

// Match range: pairs haystack and needle byte ranges
typedef struct { range32 hay; range32 ndl; } match32;

fun b8 match32Z(match32 const *a, match32 const *b) {
    if (a->hay.lo != b->hay.lo) return a->hay.lo < b->hay.lo;
    if (a->hay.hi != b->hay.hi) return a->hay.hi < b->hay.hi;
    if (a->ndl.lo != b->ndl.lo) return a->ndl.lo < b->ndl.lo;
    return a->ndl.hi < b->ndl.hi;
}
con match32 match32Max = {{u32max, u32max}, {u32max, u32max}};

#endif
