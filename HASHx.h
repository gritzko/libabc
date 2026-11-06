//
// (c) Victor Grishchenko, 2020-2024
//
//  IMPORTANT: the hash-table length passed in via the data slice MUST
//  be a power of 2 (see HASH.md).  We map hash → slot via a bitmask
//  rather than `%`, so a non-power-of-2 length silently corrupts the
//  table and may infinite-loop the probe.
//
#include "01.h"
#include "HASH.h"
#include "S.h"

#define HASH_T X(, )

#ifndef ABC_HASH_LINE
#define ABC_HASH_LINE 64
#endif

#ifndef ABC_HASH_CONVERGE
#define ABC_HASH_CONVERGE 0
#endif

// HASH-26B02GfveW: a CONVERGE Get miss may stop at the first resident hashing
// above the key, a hash per probed resident; see HASH.md "Knobs".
#ifndef ABC_HASH_MISSSTOP
#define ABC_HASH_MISSSTOP 0
#endif

#ifdef lineX
#error "lineX is retired: ABC_HASH_LINE"
#endif
static_assert(ABC_HASH_LINE >= 2 && 0 == (ABC_HASH_LINE & (ABC_HASH_LINE - 1)),
              "ABC_HASH_LINE: a power of 2");
static_assert(ABC_HASH_CONVERGE || !ABC_HASH_MISSSTOP,
              "ABC_HASH_MISSSTOP needs ABC_HASH_CONVERGE");

// ABC_HASH_MERGE (optional, any value): Put folds rec into a resident of
// the same key by X(,Merge)(resident, rec) instead of overwriting it.

#define MASK (ABC_HASH_LINE - 1)
#define LMASK (X(, sLen)(data) - 1)

// The key view (wiki/Containers.mkd): X(,Hash) and X(,KeyEq) read the key
// only; each instantiating type defines both.  The all-zero record is the
// empty slot.
fun ok64 X(HASH, _scan)(size_t *ndx, X($, ) data, HASH_T const *rec) {
    sane($ok(data) && ndx != NULL && 0 == ($len(data) & MASK));
    size_t off = (*ndx) & MASK;
    size_t base = (*ndx) & ~MASK;
    for (size_t i = off + 1; i < off + ABC_HASH_LINE; ++i) {
        *ndx = base + (i & MASK);
        if (X(, IsZero)(data[0] + *ndx)) return HASHNONE;
        // ABC-015: (rec, slot) arg order, same as Get/Del below
        if (X(, KeyEq)(rec, *data + *ndx)) return OK;
    }
    return HASHFULL;
}

fun ok64 X(HASH, _find)(size_t *ndx, X($, ) data, HASH_T const *rec) {
    u64 hash = X(, Hash)(rec);
    *ndx = hash & LMASK;
    // is0 first: the all-zero record is the empty slot, never found
    if (X(, IsZero)(data[0] + *ndx)) return HASHNONE;
    if (X(, KeyEq)(rec, $atp(data, *ndx))) return OK;
    return X(HASH, _scan)(ndx, data, rec);
}

fun ok64 X(HASH, _get)(HASH_T *rec, X($, ) data, size_t ndx, u64 hash) {
    size_t off = ndx & MASK;
    size_t base = ndx & ~MASK;
    for (size_t i = (off + 1) & MASK; i != off; i = (i + 1) & MASK) {
        ndx = base + i;
        if (X(, IsZero)(data[0] + ndx)) return HASHNONE;
        if (X(, KeyEq)(rec, $atp(data, ndx))) {
            *rec = $at(data, ndx);
            return OK;
        }
        if (ABC_HASH_CONVERGE && ABC_HASH_MISSSTOP &&
            X(, Hash)($atp(data, ndx)) > hash)
            return HASHNONE;
    }
    return HASHNONE;
}

fun ok64 X(HASH, Get)(HASH_T *rec, X($, ) data) {
    u64 hash = X(, Hash)(rec);
    size_t ndx = hash & LMASK;
    if (X(, IsZero)(data[0] + ndx)) return HASHNONE;
    if (X(, KeyEq)(rec, $atp(data, ndx))) {
        *rec = $at(data, ndx);  // TODO mv
        return OK;
    }
    if (ABC_HASH_CONVERGE && ABC_HASH_MISSSTOP &&
        X(, Hash)($atp(data, ndx)) > hash)
        return HASHNONE;
    return X(HASH, _get)(rec, data, ndx, hash);
}

// HASH-26B02G3r5y: read-only lookup, no copies, no mutation (unlike
// Probe under CONVERGE).  OK: *slot is the resident with rec's key;
// HASHNONE: absent, *slot untouched.
fun ok64 X(HASH, At)(HASH_T const **slot, X($, ) data, HASH_T const *rec) {
    size_t ndx = 0;
    if (OK != X(HASH, _find)(&ndx, data, rec)) return HASHNONE;
    *slot = data[0] + ndx;
    return OK;
}

fun ok64 X(HASH, _probe)(HASH_T **slot, X($, ) data, HASH_T const *rec,
                         u64 hash);

// OK (rec's key now holds rec, or the X(,Merge) fold) or HASHFULL (rec's line
// is full, the table untouched): Probe + Mv, the same layout either way.
fun ok64 X(HASH, Put)(X($, ) data, HASH_T const *rec) {
    HASH_T *slot = NULL;
    ok64 o = X(HASH, _probe)(&slot, data, rec, X(, Hash)(rec));
    if (o == HASHFULL) return o;
#ifdef ABC_HASH_MERGE
    if (o == OK) {
        X(, Merge)(slot, rec);
        return OK;
    }
#endif
    X(, Mv)(slot, rec);
    return OK;
}

// Backward-shift deletion: zero slot ndx, refill it from later slots of the
// line (copy-before-zero) whose probe path covers the hole.  The scan runs to
// an empty slot or the hole itself (HASH-26A275TmIX: stale Grow copies).  YES
// if it ran past one lap, so an entry may have moved before ndx (_growClean).
fun b8 X(HASH, _shiftlap)(X($, ) data, size_t ndx) {
    size_t base = ndx & ~MASK, at = ndx;
    b8 lap = NO;
    for (size_t x = base + ((ndx + 1) & MASK); x != ndx;
         x = base + ((x + 1) & MASK)) {
        if (X(, IsZero)(data[0] + x)) break;
        if (x == at) lap = YES;
        size_t nom = X(, Hash)(*data + x) & LMASK;
        if (((ndx - nom) & MASK) < ((x - nom) & MASK)) {
            Bat(data, ndx) = Bat(data, x);
            ndx = x;
        }
    }
    zero(Bat(data, ndx));
    return lap;
}

fun ok64 X(HASH, _shift)(X($, ) data, size_t ndx) {
    sane($ok(data) && (i64)ndx < $len(data));
    X(HASH, _shiftlap)(data, ndx);
    return X(, IsZero)(data[0] + ndx) ? HASHNONE : OK;  // refilled: nonzero
}

// HASHNONE if absent (std::erase count 0); callers that don't care ignore it
fun ok64 X(HASH, Del)(X($, ) data, HASH_T const *rec) {
    size_t ndx = 0;
    ok64 o = X(HASH, _find)(&ndx, data, rec);
    if (o == HASHNONE || o == HASHFULL) return HASHNONE;
    o = X(HASH, _shift)(data, ndx);
    return o == HASHNONE ? OK : o;
}

// HASH-26B02Ggncg: delete the entry at slot ndx while iterating.  OK: a later
// entry moved into ndx, re-examine it; HASHNONE: ndx is empty now.  An unvisited
// entry never moves before ndx; a visited one (a wrapped line) may be seen
// twice.  The loop: HASH.md "Delete while iterating".
fun ok64 X(HASH, DelAt)(X($, ) data, size_t ndx) {
    return X(HASH, _shift)(data, ndx);
}

// Iteration: OK and *ndx at the first live slot at or after *ndx, else
// HASHNONE (the table's end).  Start at 0, ++ndx to go on.
fun ok64 X(HASH, Next)(size_t *ndx, X($, ) data) {
    for (; *ndx < (size_t)$len(data); ++*ndx)
        if (!X(, IsZero)(data[0] + *ndx)) return OK;
    return HASHNONE;
}

// HASH-26A275TmIX: in-place growth (HASH.md "Growth in place"): DATA is the
// pow2 table, IDLE the room it grows into; Grow doubles DATA into IDLE's first
// n slots in three phases, each leaving a valid table.  Phase 1: an entry of
// lower line b homed at b+n under 2n is Put there; upper lines zeroed first.
fun void X(HASH, _growCopy)(X($, ) data) {
    size_t half = $len(data) >> 1;
    for (size_t b = 0; b < half; b += ABC_HASH_LINE) {
        HASH_T *up = data[0] + half + b;
        for (HASH_T *z = up; z < up + ABC_HASH_LINE; ++z) zerop(z);
        for (HASH_T *p = data[0] + b; p < data[0] + b + ABC_HASH_LINE; ++p) {
            if (X(, IsZero)(p)) continue;
            u64 h = X(, Hash)(p);  // hashed once, Put would rehash
            HASH_T *s = NULL;  // <= LINE into 0s: fits
            if ((h & half) && HASHNONE == X(HASH, _probe)(&s, data, p, h))
                X(, Mv)(s, p);
        }
    }
}

// Phase 3 step: slot ndx of the lower half holds a stale copy (its home
// under the doubled length is in the upper half)?  Delete it by the
// backward shift (copy-before-zero), OK; else HASHNONE.  An entry the
// shift brings into ndx may be stale too: call again.
fun ok64 X(HASH, _unstale)(X($, ) data, size_t ndx) {
    HASH_T const *p = data[0] + ndx;
    if (X(, IsZero)(p) || !(X(, Hash)(p) & ($len(data) >> 1))) return HASHNONE;
    ok64 o = X(HASH, _shift)(data, ndx);
    return o == HASHNONE ? OK : o;
}

// Phase 3: unstale the whole lower half, line by line.  A shift running
// past one lap may carry a not yet checked entry back before ndx, so
// then the line is swept again; otherwise slots before ndx only ever
// get checked entries and one sweep is enough.  Idempotent.
fun void X(HASH, _growClean)(X($, ) data) {
    size_t half = $len(data) >> 1;
    for (size_t b = 0; b < half; b += ABC_HASH_LINE) {
        b8 dirty = YES;
        while (dirty) {
            dirty = NO;
            for (HASH_T *p = data[0] + b; p < data[0] + b + ABC_HASH_LINE; ++p)
                while (!X(, IsZero)(p) && (X(, Hash)(p) & half))
                    dirty |= X(HASH, _shiftlap)(data, p - data[0]);
        }
    }
}

// Grow the table (DATA) 2x in place: phase 1, phase 2 (the flip: DATA's
// end moves, one word store), phase 3.  HASHNOROOM if IDLE is shorter than
// DATA; never maps.  Never overflows a line: one doubling always fits.
fun ok64 X(HASH, Grow)(X(B, ) buf) {
    sane(buf != NULL && buf[0] != NULL);
    size_t len = X(, bDataLen)(buf);
    test(len >= ABC_HASH_LINE && 0 == (len & (len - 1)), HASHBADARG);
    test(X(, bIdleLen)(buf) >= len, HASHNOROOM);
    X($, ) data = {buf[1], buf[2] + len};
    X(HASH, _growCopy)(data);
    call(X(, bFed), buf, len);
    X(HASH, _growClean)(data);
    done;
}

// HASH-26A275ba4V: rehash every entry of from's DATA into into's DATA, a
// table the caller set up at any power-of-2 length; never maps, from is read
// only, the tables must not overlap (HASHBADARG).  HASHFULL: into is left partially
// filled, none lost, from intact.  HASH.md "Move into another table".
fun ok64 X(HASH, Move)(X(B, ) into, X(B, ) from) {
    sane(into != NULL && into[0] != NULL && from != NULL && from[0] != NULL);
    X($, ) data = {into[1], into[2]};
    size_t len = $len(data);
    test(len >= ABC_HASH_LINE && 0 == (len & (len - 1)), HASHBADARG);
    test(from[2] <= into[1] || into[2] <= from[1], HASHBADARG);
    for (HASH_T const *p = from[1]; p < from[2]; ++p) {
        if (X(, IsZero)(p)) continue;
        call(X(HASH, Put), data, p);
    }
    done;
}

// Put into a growable table (DATA, see Grow), Grow-ing first if rec's
// line is full.
fun ok64 X(HASH, PutGrow)(X(B, ) buf, HASH_T const *rec) {
    sane(buf != NULL && buf[0] != NULL);
    for (;;) {
        X($, ) data = {buf[1], buf[2]};
        ok64 o = X(HASH, Put)(data, rec);
        if (o != HASHFULL) return o;
        call(X(HASH, Grow), buf);
    }
}

// HASH-26A274tV3C: get-or-insert in one probe, rec hashed once, never copied.
// OK: *slot is the resident.  HASHNONE: *slot is the zeroed slot Put would use;
// fill it or Unprobe it before any other op (under CONVERGE it hides the
// displaced residents, moved as Put moves them).  HASHFULL: nothing touched.
fun ok64 X(HASH, _probe)(HASH_T **slot, X($, ) data, HASH_T const *rec,
                         u64 hash) {
    size_t fit = hash & LMASK;
    size_t off = fit & MASK;
    HASH_T *line = data[0] + (fit & ~MASK);
    size_t disp = ABC_HASH_LINE;  // CONVERGE: rec displaces this one
    u64 dhash = 0;
    for (size_t i = 0; i < ABC_HASH_LINE; ++i) {
        HASH_T *s = line + ((off + i) & MASK);
        if (X(, IsZero)(s)) {
            if (ABC_HASH_CONVERGE && disp < i) {
                HASH_T *d = line + ((off + disp) & MASK);
                HASH_T r;
                X(, Mv)(&r, d);
                zerop(d);
                for (size_t j = disp + 1; j <= i; ++j) {
                    HASH_T *c = line + ((off + j) & MASK);
                    if (X(, IsZero)(c)) {
                        X(, Mv)(c, &r);
                        break;
                    }
                    u64 h2 = X(, Hash)(c);
                    if (h2 > dhash) {
                        X(, Swap)(&r, c);
                        dhash = h2;
                    }
                }
                s = d;
            }
            *slot = s;
            return HASHNONE;
        }
        if (X(, KeyEq)(rec, s)) {
            *slot = s;
            return OK;
        }
        if (ABC_HASH_CONVERGE && disp == ABC_HASH_LINE) {
            u64 h2 = X(, Hash)(s);
            if (h2 > hash) {
                disp = i;
                dhash = h2;
            }
        }
    }
    return HASHFULL;
}

fun ok64 X(HASH, Probe)(HASH_T **slot, X($, ) data, HASH_T const *rec) {
    return X(HASH, _probe)(slot, data, rec, X(, Hash)(rec));
}

// HASH-26B02GitHz: undo a HASHNONE Probe left unfilled (lookup-only use,
// an error path): a backward shift at the slot, the Put layout restored.
fun ok64 X(HASH, Unprobe)(X($, ) data, HASH_T *slot) {
    ok64 o = X(HASH, _shift)(data, slot - data[0]);
    return o == HASHNONE ? OK : o;
}

// Probe a growable table (DATA, see Grow), Grow-ing it while rec's line is
// full: OK or HASHNONE as Probe, *slot into the (grown) table; else Grow's failure.
fun ok64 X(HASH, ProbeGrow)(HASH_T **slot, X(B, ) buf, HASH_T const *rec) {
    sane(buf != NULL && buf[0] != NULL);
    for (;;) {
        X($, ) data = {buf[1], buf[2]};
        ok64 o = X(HASH, Probe)(slot, data, rec);
        if (o != HASHFULL) return o;
        call(X(HASH, Grow), buf);
    }
}

#undef MASK
#undef LMASK
#undef ABC_HASH_LINE
// ABC-015: per-instantiation knob, undef like ABC_HASH_LINE
#undef ABC_HASH_CONVERGE
#undef ABC_HASH_MISSSTOP
#undef ABC_HASH_MERGE
#undef HASH_T
