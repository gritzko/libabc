#include "B.h"
#include "OK.h"
#include "Sx.h"
#include "01.h"

#define T X(, )

typedef T *X(B, )[4];
typedef X($, ) * X(B$, )[4];
#ifndef ABC_X_$
typedef X($, c) * X(B$, c)[4];
#endif

typedef T *const X(, b)[4];
typedef T const *const X(, cb)[4];
typedef T *const *X(, bp);
typedef T const *const *X(, cbp);
typedef X($, ) *const X(, sb)[4];
typedef X($, c) *const X(, csb)[4];

fun T **X(B, $1)(X(B, ) buf) { return (T **)buf + 1; }
fun T **X(B, $2)(X(B, ) buf) { return (T **)buf + 2; }
fun T const **X(B, c$1)(X(B, ) buf) { return (T const **)buf + 1; }
fun T const **X(B, c$2)(X(B, ) buf) { return (T const **)buf + 2; }

fun void X(B, eat$1)(X(B, ) buf) { ((T **)buf)[1] = buf[2]; }
fun void X(B, eat$2)(X(B, ) buf) { ((T **)buf)[2] = buf[3]; }
fun void X(B, eatdata)(X(B, ) buf) { ((T **)buf)[1] = buf[2]; }
fun void X(B, eatidle)(X(B, ) buf) { ((T **)buf)[2] = buf[3]; }

fun void X(B, resetpast)(X(B, ) buf) { ((T **)buf)[1] = buf[0]; }
fun void X(B, resetdata)(X(B, ) buf) { ((T **)buf)[2] = buf[1]; }

fun T const *const *X(, cbPast)(X(, cb) buf) { return (T const **)buf + 0; }
fun T const **X(, cbData)(X(, cb) buf) { return (T const **)buf + 1; }
fun T const **X(, cbIdle)(X(, cb) buf) { return (T const **)buf + 2; }
fun T const *const *X(, bPastC)(X(, b) buf) { return (T const **)buf + 0; }
fun T const **X(, bDataC)(X(, b) buf) { return (T const **)buf + 1; }
fun T const **X(, bIdleC)(X(, b) buf) { return (T const **)buf + 2; }
fun T *const *X(, bPast)(X(, b) buf) { return (T **)buf + 0; }
fun T **X(, bData)(X(, b) buf) { return (T **)buf + 1; }
fun T **X(, bIdle)(X(, b) buf) { return (T **)buf + 2; }

fun X(, gp) X(, bPastData)(X(, bp) buf) { return (X(, gp))buf; }
fun X(, gp) X(, bDataIdle)(X(, bp) buf) { return (X(, gp))(buf + 1); }

// PastDataS: joined [buf[0], buf[2]) — span over PAST + DATA together,
// excluding IDLE.  Intended use is "all-known-records" reads when a
// buffer uses the PAST/DATA boundary to separate inherited entries
// (parents / read-only views) from owned-leaf entries (writes).
// See keeper/KEEP.h "Branch-aware object store" — `packs` lays trunk
// → … → parent dirs into PAST and the active leaf branch into DATA.
fun void X(, PastDataS)(X(, b) buf, X(, sp) into) {
    into[0] = (T *)buf[0];
    into[1] = (T *)buf[2];
}
fun size_t X(, bPastDataLen)(X(, b) buf) {
    return (size_t)((T *)buf[2] - (T *)buf[0]);
}

fun size_t X(, bLen)(X(, b) buf) { return ((T *)buf[3]) - ((T *)buf[0]); }
fun size_t X(, bBusyLen)(X(, b) buf) { return ((T *)buf[2]) - ((T *)buf[0]); }
fun size_t X(, bPastLen)(X(, b) buf) { return $len((T **)buf + 0); }
fun size_t X(, bDataLen)(X(, b) buf) { return $len((T **)buf + 1); }
fun size_t X(, bIdleLen)(X(, b) buf) { return $len((T **)buf + 2); }

fun size_t X(, bSize)(X(, b) buf) { return ((u8c *)buf[3]) - ((u8c *)buf[0]); }

fun size_t X(, cbPastLen)(X(, cb) buf) { return $len((T **)buf + 0); }
fun size_t X(, cbDataLen)(X(, cb) buf) { return $len((T **)buf + 1); }
fun size_t X(, cbIdleLen)(X(, cb) buf) { return $len((T **)buf + 2); }

fun T *X(, bHead)(X(, b) b) {
    return b[0];
}
fun T *X(, bDataHead)(X(, b) b) {
    assert(b[1] <= b[2]);
    return b[1];
}
fun T *X(, bIdleHead)(X(, b) b) {
    assert(b[2] <= b[3]);
    return b[2];
}
fun T *X(, bTerm)(X(, b) b) {
    return b[3];
}

fun b8 X(, bHasRoom)(X(, bp) buf) { return !$empty(X(, bIdle)(buf)); }
fun b8 X(, bHasData)(X(, bp) buf) { return !$empty(X(, bData)(buf)); }

fun b8 X(, bOK)(const X(, bp) buf) { return Bok(buf); }
fun b8 X(, cbOK)(const X(, cbp) buf) { return Bok(buf); }

fun T const *const *X(Bc, cpast)(X(B, ) buf) {
    return (T const *const *)buf + 0;
}
fun T const *const *X(Bc, cdata)(X(B, ) buf) {
    return (T const *const *)buf + 1;
}
fun T const *const *X(Bc, cidle)(X(B, ) buf) {
    return (T const *const *)buf + 2;
}
fun b8 X(B, empty)(X(B, ) buf) { return buf[2] == buf[1]; }

fun T *X(, bAtP)(X(, b) buf, size_t ndx) {
    T *p = buf[0] + ndx;
    assert(p < buf[3]);
    return p;
}

fun T *X(, bDataAtP)(X(, b) buf, size_t ndx) {
    T *p = buf[1] + ndx;
    assert(p < buf[3]);
    return p;
}

fun size_t X(, bIdx)(X(, b) buf, X(, cp) p) {
    assert(p >= buf[0] && p < buf[3]);
    return (size_t)(p - buf[0]);
}

#ifndef ABC_X_$
fun T X(, bAt)(X(, b) buf, size_t ndx) { return *X(, bAtP)(buf, ndx); }

fun ok64 X(, bFedP)(X(, b) b, X(, p) * p) {
    if (b[2] >= b[3]) return NOROOM;
    *p = ((T **)b)[2]++;
    return OK;
}
#endif

fun void X(B, eat)(X(B, ) buf) {
    T const **b = (T const **)buf;
    b[1] = b[2];
}

fun void X(, bZero)(X(, b) buf) { memset((void *)*buf, 0, Bsize(buf)); }

// --- Boundary movers ---
//
// PAST | DATA | IDLE  (see B.md).  bFed/bShed move the DATA↔IDLE
// boundary (buf[2]); bUsed moves the PAST↔DATA boundary (buf[1]).
//
//                bUsed      bFed/bShed
//                  v          v
//   ... PAST ... | DATA ... | IDLE ...
//   buf[0]      buf[1]      buf[2]      buf[3]
//
//   Fed  : grow DATA into IDLE (DATA gained, IDLE shrank).
//   Shed : shrink DATA from the IDLE side (DATA lost, IDLE grew).
//   Used : consume DATA from the PAST side (DATA lost, PAST grew).
//
// Use case: producers `bFed1`/`bFed` to append; consumers `bUsed` to
// drop the prefix once handled (typical for parsers with a moving
// cursor); `bShed*` rolls back recently appended data — useful when a
// staging area must be rewound on failure or arena reuse (e.g. an arena
// where DATA is intentionally kept empty and IDLE is the scratchpad:
// `bShedAll` empties DATA which makes IDLE span the whole buffer
// again).  Note that `bShedAll` is the typed equivalent of the cast
// idiom `((T **)buf)[2] = buf[1]`.

fun ok64 X(, bFed)(X(, b) buf, size_t len) {
    return X(, sFed)(X(, bIdle)(buf), len);
}

// bShed*: roll the DATA/IDLE boundary back into DATA (data shrinks,
// idle grows).  No memory move.
fun ok64 X(, bShed1)(X(, b) buf) { return X(, gShed1)(X(, bDataIdle)(buf)); }
fun ok64 X(, bShed)(X(, b) buf, size_t l) {
    return X(, gShed)(X(, bDataIdle)(buf), l);
}
fun ok64 X(, bShedAll)(X(, b) buf) {
    return X(, gShedAll)(X(, bDataIdle)(buf));
}
fun ok64 X(, bUsedAll)(X(, b) buf) {
    return X(, gUsedAll)(X(, bPastData)(buf));
}
// bUsed: consume DATA from buffer start (advance buf[1]).
// Counterpart to bShed (which trims from the IDLE side).
fun ok64 X(, bUsed)(X(, b) buf, size_t len) {
    return X(, gUsed)(X(, bPastData)(buf), len);
}

fun ok64 X(, bFed1)(X(, b) buf) { return X(, sFed)(X(, bIdle)(buf), 1); }

fun b8 X(, bDataEmpty)(const X(, cbp) buf) { return X(, cbDataLen)(buf) == 0; }

fun b8 X(, bEmpty)(const X(, bp) buf) { return buf[2] == buf[0]; }

fun T *X(, bLast)(X(, b) buf) {
    assert(buf[2] > buf[1]);
    return buf[2] - 1;
}

/*fun T X(B, at)(X(B, ) buf, size_t len) {
    T *p = buf[0] + len;
    assert(p < buf[3]);
    return *p;
}*/

fun ok64 X(, bAllocate)(X(, bp) buf, size_t len) {
    size_t sz = len * sizeof(T);
    ok64 o = Balloc((void **)buf, sz);
    if (o != OK) return o;
    memset((void *)*buf, 0, sz);
    return OK;
}
fun ok64 X(, bAlloc)(X(, bp) buf, size_t len) {
    return X(, bAllocate)(buf, len);
}

fun ok64 X(, bFree)(X(, bp) buf) { return Bfree((void **)buf); }

fun ok64 X(, bReserve)(X(, bp) buf, size_t len) {
    return Breserve((void *const *)buf, len * sizeof(T));
}
/*
fun ok64 X(B, feedp)(X(B, ) buf, T const *one) {
    ok64 re = X(B, reserve)(buf, 1);
    if (re != OK) return re;
    T **idle = X(,bIdle)(buf);
    memcpy(*idle, one, sizeof(T));
    ++*idle;
    return OK;
}
*/

fun ok64 X(, bPush)(X(, bp) buf, X(, cp) one) {
    return X(, sFeedP)(X(, bIdle)(buf), one);
}
#ifndef ABC_X_$
fun ok64 X(, bPushed)(X(, bp) buf, X(, pp) pp) {
    T **idle = X(, bIdle)(buf);
    if (!$len(idle)) return NOROOM;
    *pp = *idle;
    ++*idle;
    return OK;
}
fun ok64 X(, bTop)(X(, bp) buf, X(, pp) pp) {
    T **data = X(, bData)(buf);
    if ($empty(data)) return NODATA;
    *pp = data[1] - 1;
    return OK;
}
#endif

fun ok64 X(, bFeedP)(X(, bp) buf, T const *one) {
    return X(, sFeedP)(X(, bIdle)(buf), one);
}

fun ok64 X(, bFeed2)(X(, bp) buf, T a, T b) {
    // ok64 re = X(B, reserve)(buf, 2);
    // f (re != OK) return re;
    T **idle = X(, bIdle)(buf);
    if ($len(idle) < 2) return BNOROOM;
    memcpy((void *)*idle, &a, sizeof(T));
    ++*idle;
    memcpy((void *)*idle, &b, sizeof(T));
    ++*idle;
    return OK;
}

fun ok64 X(, bFeed1)(X(, bp) buf, T one) {
    return X(, sFeed1)(X(, bIdle)(buf), one);
}

fun ok64 X(, bFeed)(X(, b) buf, X(, csc) from) {
    T **into = X(, bIdle)(buf);
    if ($len(into) < $len(from)) return BNOROOM;
    X(, sCopy)(into, from);
    *into += $len(from);
    return OK;
}

// bHost: append `orig` to `buf`'s IDLE and report the resulting
// range — `as[0..1]` borders the freshly-written bytes inside `buf`.
// The buffer is the new host for the copied content; callers keep a
// slice that aliases into it.  Refuses with BNOROOM when IDLE is too
// small (buf untouched, `as` not written).
fun ok64 X(, bHost)(X(, b) buf, X(, csp) as, X(, csc) orig) {
    T *p = X(, bIdleHead)(buf);
    ok64 o = X(, bFeed)(buf, orig);
    if (o != OK) return o;
    as[0] = p;
    as[1] = X(, bIdleHead)(buf);
    return OK;
}

fun ok64 X(B, mark)(X(B, ) const buf, range64 *range) {
    range->from = buf[1] - buf[0];
    range->till = buf[2] - buf[0];
    return OK;
}

fun void X(, bReset)(X(, b) buf) {
    T **b = (T **)buf;
    b[1] = b[0];
    b[2] = b[0];
}

//  Move buffer ownership: copy the four descriptor pointers from
//  `src` into `dst` and zero `src` so it no longer references the
//  backing memory.  Use this in place of a raw `memcpy(dst, src,
//  sizeof(*dst))` whenever a buffer changes hands — copying the
//  descriptor without zeroing the source creates two owners of the
//  same allocation, which is exactly the trap CLAUDE.md's "buffer
//  ownership rule" warns about.
fun void X(, bHandOver)(X(, b) dst, X(, b) src) {
    T **d = (T **)dst;
    T **s = (T **)src;
    d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
    s[0] = NULL; s[1] = NULL; s[2] = NULL; s[3] = NULL;
}

fun ok64 X(B, rewind)(X(B, ) buf, range64 range) {
    size_t len = Blen(buf);
    if (range.till < range.from || range.till > len) return SMISS;
    T **b = (T **)buf;
    b[1] = b[0] + range.from;
    b[2] = b[0] + range.till;
    return OK;
}

// fun void X(B, reset)(X(B, ) buf) { X(B, rewind)(buf, 0, 0); }

fun ok64 X(B, mark$)(X(B, ) const buf, X($, ) slice, range64 *range) {
    if (!Bwithin(buf, slice)) return BMISS;
    range->from = slice[0] - buf[0];
    range->till = slice[1] - buf[0];
    return OK;
}

fun ok64 X($, mark)(X($, c) const host, X($, c) const slice, range64 *range) {
    if (!$within(host, slice)) return SMISS;
    range->from = slice[0] - host[0];
    range->till = slice[1] - host[0];
    return OK;
}

fun ok64 X($, rewind)(X($c, c) host, X($, c) slice, range64 range) {
    size_t len = $len(host);
    if (range.till < range.from || range.till > len) return SMISS;
    slice[0] = host[0] + range.from;
    slice[1] = host[0] + range.till;
    return OK;
}

fun ok64 X(B, rewind$)(X(B, ) buf, X($, ) slice, range64 range) {
    size_t len = Blen(buf);
    if (range.till < range.from || range.till > len) return BMISS;
    slice[0] = buf[0] + range.from;
    slice[1] = buf[0] + range.till;
    return OK;
}

fun ok64 X(, bPop)(X(, b) buf) {
    if (buf[2] <= buf[1]) return BNODATA;
    T const **b = (T const **)buf;
    --b[2];
    return OK;
}

// MAP_NORESERVE is a Linux-only swap-reservation hint; elsewhere it is a no-op.
#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif

fun ok64 X(, bMap)(X(, b) buf, size_t len) {
    size_t size = len * sizeof(T);
    T *map = (T *)mmap(NULL, size, PROT_READ | PROT_WRITE,
                       MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE, -1, 0);
    if (map == MAP_FAILED) return MMAPFAIL;
    T **b = (T **)buf;
    b[0] = b[1] = b[2] = b[3] = map;
    b[3] += len;
    return OK;
}

fun ok64 X(, bReMap)(X(, bp) buf, size_t new_len) {
    if (BNULL(buf) || new_len == 0) return BADARG;
    size_t old_size = X(, bSize)(buf);
    size_t new_size = new_len * sizeof(T);
#ifdef MREMAP_MAYMOVE
    u8 *new_mem =
        (u8 *)mremap((void *)buf[0], old_size, new_size, MREMAP_MAYMOVE);
    if (new_mem == MAP_FAILED) return MMAPFAIL;
#else
    void *new_mem = mmap(NULL, new_size, PROT_READ | PROT_WRITE,
                         MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (new_mem == MAP_FAILED) return MMAPFAIL;
    memmove(new_mem, (void *)buf[0], old_size);
    int rc = munmap((void *)buf[0], old_size);
    if (rc != 0) return MMAPFAIL;
#endif
    T **b = (T **)buf;
    size_t l1 = b[1] - b[0];
    size_t l2 = b[2] - b[0];
    if (l1 > new_len) l1 = new_len;
    if (l2 > new_len) l2 = new_len;
    b[0] = (T *)new_mem;
    b[1] = b[0] + l1;
    b[2] = b[0] + l2;
    b[3] = b[0] + new_len;
    return OK;
}

fun ok64 X(, bUnMap)(X(, b) buf) {
    if (unlikely(buf == NULL || *buf == NULL)) return FAILSANITY;
    if (-1 == munmap((void *)buf[0], Bsize(buf))) return MMAPFAIL;
    void **b = (void **)buf;
    b[0] = b[1] = b[2] = b[3] = NULL;
    return OK;
}

fun ok64 X(, bShift)(X(, b) buf, size_t pastlen) {
    if (unlikely(!Bok(buf))) return FAILSANITY;
    size_t datalen = $len(Bdata(buf));
    if (!datalen) {
        Breset(buf);
        return OK;
    }
    if (unlikely(pastlen + datalen > Blen(buf))) return NOROOM;
    T *to = buf[0] + pastlen;
    memmove((void *)(to), (void *)(buf[1]), BDataSize(buf));
    ((T **)buf)[1] = to;
    ((T **)buf)[2] = to + datalen;
    return OK;
}

fun ok64 X(, bSplice)(X(, bp) buf, size_t off, size_t cut, X(, csc) paste) {
    if (!Bok(buf) || !X(, csOK)(paste)) return FAILSANITY;
    if (X(, bDataLen)(buf) < off + cut ||
        X(, bIdleLen)(buf) + cut < X(, csLen)(paste))
        return BMISS;
    u8 *b = ((u8 **)buf)[1];
    memmove(b + off + $len(paste), b + off + cut,
            X(, bDataLen)(buf) - off - cut);
    memmove(b + off, paste[0], $len(paste));
    ((T **)buf)[2] = buf[2] + $len(paste) - cut;
    return OK;
}

// Arena trio (bAlign / bAcq / bAren): rent typed slices out of a u8
// arena.  PAST keeps every sealed rental; DATA is the in-flight one;
// IDLE is the free space.  Padding for T-alignment lands in PAST.

// bAlign(arena) -> T##gp
//   Collapse any pending DATA into PAST, align IDLE up to _Alignof(T),
//   return a typed gauge over the aligned IDLE.  Cannot fail; if IDLE
//   is exhausted the returned gauge is empty.
fun X(, gp) X(, bAlign)(u8 *const *arena) {
    uintptr_t al = ((uintptr_t)arena[2] + _Alignof(T) - 1)
                 & ~((uintptr_t)_Alignof(T) - 1);
    u8 *base = (u8 *)al;
    if (base > arena[3]) base = arena[3];
    ((u8 **)arena)[1] = base;
    ((u8 **)arena)[2] = base;
    return (X(, gp))(arena + 1);
}

// bAcq(arena, ren)
//   Snapshot current DATA into typed const slice `ren`, then collapse
//   DATA into PAST so the slice survives subsequent rentals.
fun void X(, bAcq)(u8 *const *arena, X(, csp) ren) {
    ren[0] = (T const *)arena[1];
    ren[1] = (T const *)arena[2];
    ((u8 **)arena)[1] = arena[2];
}

// bContains(buf, p): is pointer `p` inside the buffer's backing range?
fun b8 X(, bContains)(X(, b) buf, void const *p) {
    return (u8c *)p >= (u8c *)buf[0] && (u8c *)p < (u8c *)buf[3];
}

// bAren(arena, ren, orig)
//   One-shot rental: append `orig` into the arena (with T-alignment
//   padding folded into PAST), fill `ren` to span the freshly stored
//   copy.  Equivalent to bAlign + gFeed + bAcq but does a single
//   bounds-checked memcpy.  Returns BNOROOM if the arena can't fit.
fun ok64 X(, bAren)(u8 *const *arena, X(, csp) ren, X(, csc) orig) {
    size_t count = (size_t)(orig[1] - orig[0]);
    size_t need  = count * sizeof(T);
    uintptr_t al = ((uintptr_t)arena[2] + _Alignof(T) - 1)
                 & ~((uintptr_t)_Alignof(T) - 1);
    u8 *base = (u8 *)al;
    if (base + need > arena[3]) return BNOROOM;
    if (need) memcpy(base, orig[0], need);
    ren[0] = (T const *)base;
    ren[1] = (T const *)base + count;
    ((u8 **)arena)[1] = base + need;
    ((u8 **)arena)[2] = base + need;
    return OK;
}

// Acquire a fixed-cap child T-buffer from a u8 arena's IDLE.  See
// `abc/B.md` §Arenas for the LIFO-along-call-tree discipline.
// After Acquire: child has `cap`-sized capacity, empty PAST/DATA;
// arena's PAST/DATA boundary advances past the acquired region
// (aligned up to T) so subsequent acquires get fresh space.  Returns
// BNOROOM if IDLE lacks `cap*sizeof(T)` bytes after alignment.
//
// The child borrows arena memory: NEVER u8bFree / u8bReMap /
// u8bUnMap an Acquired child, and ensure it does not outlive its
// arena scope.  Release happens via u8aRewind / u8aReset on the
// arena (see abc/B.h).
fun ok64 X(, bAcquire)(u8a arena, X(, b) child, size_t cap) {
    uintptr_t al = ((uintptr_t)arena[2] + sizeof(T) - 1) & ~(sizeof(T) - 1);
    u8 *base = (u8 *)al;
    size_t sz = cap * sizeof(T);
    if (base + sz > arena[3]) return BNOROOM;
    T **c = (T **)child;
    c[0] = c[1] = c[2] = (T *)base;
    c[3] = (T *)(base + sz);
    ((u8 **)arena)[1] = base + sz;
    ((u8 **)arena)[2] = base + sz;
    return OK;
}

#undef T
