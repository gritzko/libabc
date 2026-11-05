//  SWISS-26B05A2Jxv: a Swiss table over a record type, chosen explicitly next
//  to HASHx.h (SWISS.md).  One mapped buffer: PAST holds a u64 header and the
//  1-byte control array for the max slots, DATA the slots (a power of 2, at
//  least 16), IDLE the room Grow doubles into.  A lookup scans 16 control
//  bytes per group and reads a slot only on a tag match; the home slot line
//  is prefetched before the control line arrives, so a hit costs one wait.
#include "01.h"
#include "SWISS.h"
#include "S.h"

#define SWISS_T X(, )
static_assert(sizeof(SWISS_T) >= 16, "SWISSx.h: records of 16 B and up");

#define CTL(buf) ((u8 *)(buf)[0] + SWISS_HEAD)
#define LEFT(buf) (*(u64 *)(buf)[0])
#define LEN(buf) ((size_t)((buf)[2] - (buf)[1]))

// ABC_SWISS_MERGE (optional, any value): Put folds rec into a resident of
// the same key by X(,Merge)(resident, rec) instead of overwriting it.

// The max slots the buffer's range fits, the control array included.
fun size_t X(SWISS, _max)(X(, b) buf) {
    size_t all = (size_t)(buf[3] - buf[0]) * sizeof(SWISS_T);
    if (all < SWISS_HEAD + sizeof(SWISS_T)) return 0;
    return (all - SWISS_HEAD - sizeof(SWISS_T)) / (sizeof(SWISS_T) + 1);
}

// Set up an empty table of len slots (a power of 2, >= 16) on a mapped
// buffer: PAST takes the header and control array, DATA the slots.
fun ok64 X(SWISS, Init)(X(B, ) buf, size_t len) {
    sane(buf != NULL && buf[0] != NULL);
    test(len >= SWISS_GROUP && 0 == (len & (len - 1)), BADARG);
    size_t max = X(SWISS, _max)(buf);
    test(len <= max, NOROOM);
    size_t past = (SWISS_HEAD + max + sizeof(SWISS_T) - 1) / sizeof(SWISS_T);
    buf[1] = buf[0] + past;
    buf[2] = buf[1] + len;
    memset(CTL(buf), 0, len);
    LEFT(buf) = len - len / 8;
    done;
}

// Tag and home slot: the low 7 hash bits tag, the rest place (mix32
// hashes are 32 bits wide, so tags come off the bottom).
#define TAG(h) ((u8)(SWISS_LIVE | ((h) & 0x7f)))
#define HOME(h, mask) (((h) >> 7) & (mask))

// OK: *ndx holds rec's key.  NONE: absent; *ndx is the first free slot of
// the probe path, from the home offset on, so residents sit at their home.
fun ok64 X(SWISS, _seek)(size_t *ndx, X(, b) buf, SWISS_T const *rec,
                         u64 h) {
    u8 const *ctl = CTL(buf);
    SWISS_T const *slot = buf[1];
    size_t mask = LEN(buf) - 1, p = HOME(h, mask), at = SIZE_MAX;
    __builtin_prefetch(slot + p);
    u8 tag = TAG(h);
    for (size_t g = p & ~(size_t)15, k = 16;; g = (g + k) & mask, k += 16) {
        for (u32 m = SWISSMatch(ctl + g, tag); m; m &= m - 1) {
            size_t i = g + ctz32(m);
            if (likely(X(, KeyEq)(rec, slot + i))) {
                *ndx = i;
                return OK;
            }
        }
        u32 fr = ~SWISSLive(ctl + g) & 0xffff;
        if (at == SIZE_MAX && fr) {
            u32 up = fr & (0xffffu << (p & 15));
            at = g + ctz32(up ? up : fr);
        }
        if (SWISSEmpty(ctl + g)) break;
    }
    *ndx = at;
    return NONE;
}

fun ok64 X(SWISS, Get)(SWISS_T *rec, X(, b) buf) {
    size_t i = 0;
    if (OK != X(SWISS, _seek)(&i, buf, rec, X(, Hash)(rec))) return NONE;
    *rec = buf[1][i];
    return OK;
}

// Read-only lookup: OK, *slot the resident with rec's key; NONE untouched.
fun ok64 X(SWISS, At)(SWISS_T const **slot, X(, b) buf, SWISS_T const *rec) {
    size_t i = 0;
    if (OK != X(SWISS, _seek)(&i, buf, rec, X(, Hash)(rec))) return NONE;
    *slot = buf[1] + i;
    return OK;
}

// OK: *slot the resident.  NONE: *slot claimed for rec (tagged, contents
// stale).  FULL: no room left (7/8 of the slots live or dead), untouched.
fun ok64 X(SWISS, _claim)(SWISS_T **slot, X(, b) buf, SWISS_T const *rec) {
    u64 h = X(, Hash)(rec);
    size_t i = 0;
    ok64 o = X(SWISS, _seek)(&i, buf, rec, h);
    if (o == NONE) {
        u8 *c = CTL(buf) + i;
        if (*c == SWISS_EMPTY) {
            if (LEFT(buf) == 0) return FULL;
            --LEFT(buf);
        }
        *c = TAG(h);
    }
    *slot = buf[1] + i;
    return o;
}

// OK (rec's key now holds rec, or the X(,Merge) fold) or FULL, untouched.
fun ok64 X(SWISS, Put)(X(B, ) buf, SWISS_T const *rec) {
    SWISS_T *s = NULL;
    ok64 o = X(SWISS, _claim)(&s, buf, rec);
    if (o == FULL) return o;
#ifdef ABC_SWISS_MERGE
    if (o == OK) {
        X(, Merge)(s, rec);
        return OK;
    }
#endif
    X(, Mv)(s, rec);
    return OK;
}

// Get-or-insert, rec hashed once.  OK: *slot the resident.  NONE: *slot is
// a zeroed slot for rec's key; fill it or Unprobe it.  FULL: untouched.
fun ok64 X(SWISS, Probe)(SWISS_T **slot, X(B, ) buf, SWISS_T const *rec) {
    ok64 o = X(SWISS, _claim)(slot, buf, rec);
    if (o == NONE) zerop(*slot);
    return o;
}

// Nothing ever moves: a slot whose group has an empty byte was never on a
// longer probe path, so it empties; else it turns a tomb.  NONE: ndx empty.
fun ok64 X(SWISS, DelAt)(X(B, ) buf, size_t ndx) {
    u8 *ctl = CTL(buf);
    if (ndx >= LEN(buf) || !(ctl[ndx] & SWISS_LIVE)) return NONE;
    if (SWISSEmpty(ctl + (ndx & ~(size_t)15))) {
        ctl[ndx] = SWISS_EMPTY;
        ++LEFT(buf);
    } else {
        ctl[ndx] = SWISS_TOMB;
    }
    return NONE;
}

fun ok64 X(SWISS, Unprobe)(X(B, ) buf, SWISS_T *slot) {
    X(SWISS, DelAt)(buf, slot - buf[1]);
    return OK;
}

// NONE if absent (std::erase count 0)
fun ok64 X(SWISS, Del)(X(B, ) buf, SWISS_T const *rec) {
    size_t i = 0;
    if (OK != X(SWISS, _seek)(&i, buf, rec, X(, Hash)(rec))) return NONE;
    X(SWISS, DelAt)(buf, i);
    return OK;
}

// Iteration: OK and *ndx at the first live slot at or after *ndx, else NONE.
fun ok64 X(SWISS, Next)(size_t *ndx, X(, b) buf) {
    u8 const *ctl = CTL(buf);
    size_t len = LEN(buf);
    if (likely(*ndx < len && (ctl[*ndx] & SWISS_LIVE))) return OK;
    while (*ndx < len) {
        size_t g = *ndx & ~(size_t)15;
        u32 m = SWISSLive(ctl + g) & (0xffffu << (*ndx & 15));
        if (m) {
            *ndx = g + ctz32(m);
            return OK;
        }
        *ndx = g + SWISS_GROUP;
    }
    return NONE;
}

// Rehash in place into len2 slots (len or 2·len): residents turn MOVE, then
// each goes to its first free slot under len2, swapping with a MOVE one there
// (absl's drop-deletes scheme); tombs are purged.  A slot left in its group
// stays.  Not a sequence of valid states: never interrupted.
fun void X(SWISS, _rehash)(X(B, ) buf, size_t len2) {
    u8 *ctl = CTL(buf);
    SWISS_T *slot = buf[1];
    size_t len = LEN(buf), mask = len2 - 1, live = 0;
    memset(ctl + len, 0, len2 - len);
    for (size_t i = 0; i < len; ++i)
        ctl[i] = (ctl[i] & SWISS_LIVE) ? SWISS_MOVE : SWISS_EMPTY;
    buf[2] = buf[1] + len2;
    for (size_t i = 0; i < len; ++i) {
        while (ctl[i] == SWISS_MOVE) {
            u64 h = X(, Hash)(slot + i);
            size_t p = HOME(h, mask), t = 0;
            for (size_t g = p & ~(size_t)15, k = 16;; g = (g + k) & mask, k += 16) {
                u32 fr = ~SWISSLive(ctl + g) & 0xffff;
                if (!fr) continue;
                u32 up = fr & (0xffffu << (p & 15));
                t = g + ctz32(up ? up : fr);
                break;
            }
            ++live;
            if ((t ^ i) < SWISS_GROUP) {
                ctl[i] = TAG(h);
            } else if (ctl[t] == SWISS_EMPTY) {
                X(, Mv)(slot + t, slot + i);
                ctl[t] = TAG(h);
                ctl[i] = SWISS_EMPTY;
            } else {
                X(, Swap)(slot + t, slot + i);
                ctl[t] = TAG(h);
            }
        }
    }
    LEFT(buf) = len2 - len2 / 8 - live;
}

// Grow DATA 2x in place, rehashing.  NOROOM if IDLE is shorter than DATA or
// the control array has no room; BADARG on a bad length.  Never maps.
fun ok64 X(SWISS, Grow)(X(B, ) buf) {
    sane(buf != NULL && buf[0] != NULL);
    size_t len = LEN(buf);
    test(len >= SWISS_GROUP && 0 == (len & (len - 1)), BADARG);
    test(X(, bIdleLen)(buf) >= len && 2 * len <= X(SWISS, _max)(buf), NOROOM);
    X(SWISS, _rehash)(buf, 2 * len);
    done;
}

// FULL: rehash at the same length if tombs hold half the room, else Grow.
fun ok64 X(SWISS, _room)(X(B, ) buf) {
    size_t len = LEN(buf), live = 0;
    for (size_t g = 0; g < len; g += SWISS_GROUP)
        live += __builtin_popcount(SWISSLive(CTL(buf) + g));
    if (live * 16 < len * 7) {
        X(SWISS, _rehash)(buf, len);
        return OK;
    }
    return X(SWISS, Grow)(buf);
}

fun ok64 X(SWISS, PutGrow)(X(B, ) buf, SWISS_T const *rec) {
    sane(buf != NULL && buf[0] != NULL);
    for (;;) {
        ok64 o = X(SWISS, Put)(buf, rec);
        if (o != FULL) return o;
        call(X(SWISS, _room), buf);
    }
}

fun ok64 X(SWISS, ProbeGrow)(SWISS_T **slot, X(B, ) buf, SWISS_T const *rec) {
    sane(buf != NULL && buf[0] != NULL);
    for (;;) {
        ok64 o = X(SWISS, Probe)(slot, buf, rec);
        if (o != FULL) return o;
        call(X(SWISS, _room), buf);
    }
}

// Put every resident of from into into (an Init-ed table, residents kept, a
// key in both gets from's record), from read only; BADARG if the ranges
// overlap.  FULL: into holds the residents before that one (in from's order).
fun ok64 X(SWISS, Move)(X(B, ) into, X(B, ) from) {
    sane(into != NULL && into[0] != NULL && from != NULL && from[0] != NULL);
    test(from[3] <= into[0] || into[3] <= from[0], BADARG);
    for (size_t i = 0; OK == X(SWISS, Next)(&i, from); ++i)
        call(X(SWISS, Put), into, from[1] + i);
    done;
}

#undef TAG
#undef HOME
#undef CTL
#undef LEFT
#undef LEN
#undef ABC_SWISS_MERGE
#undef SWISS_T
