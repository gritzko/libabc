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

// HASH-26B02GfveW: CONVERGE keeps a key behind every resident of its
// probe path with a greater hash, so a Get miss may stop at the first
// one: a hash per probed resident.  Pays where hashing is cheap.
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
        if (X($, is0)(data, *ndx)) return NONE;
        // ABC-015: (rec, slot) arg order, same as Get/Del below
        if (X(, KeyEq)(rec, *data + *ndx)) return OK;
    }
    return FULL;
}

// OK, NONE, FULL
fun ok64 X(HASH, _find)(size_t *ndx, X($, ) data, HASH_T const *rec) {
    u64 hash = X(, Hash)(rec);
    *ndx = hash & LMASK;
    // is0 first: the all-zero record is the empty slot, never found
    if (X($, is0)(data, *ndx)) return NONE;
    if (X(, KeyEq)(rec, $atp(data, *ndx))) return OK;
    return X(HASH, _scan)(ndx, data, rec);
}

fun ok64 X(HASH, _get)(HASH_T *rec, X($, ) data, size_t ndx, u64 hash) {
    size_t off = ndx & MASK;
    size_t base = ndx & ~MASK;
    for (size_t i = (off + 1) & MASK; i != off; i = (i + 1) & MASK) {
        ndx = base + i;
        if (X($, is0)(data, ndx)) return NONE;
        if (X(, KeyEq)(rec, $atp(data, ndx))) {
            *rec = $at(data, ndx);
            return OK;
        }
        if (ABC_HASH_CONVERGE && ABC_HASH_MISSSTOP &&
            X(, Hash)($atp(data, ndx)) > hash)
            return NONE;
    }
    return NONE;
}

fun ok64 X(HASH, Get)(HASH_T *rec, X($, ) data) {
    u64 hash = X(, Hash)(rec);
    size_t ndx = hash & LMASK;
    if (X($, is0)(data, ndx)) return NONE;
    if (X(, KeyEq)(rec, $atp(data, ndx))) {
        *rec = $at(data, ndx);  // TODO mv
        return OK;
    }
    if (ABC_HASH_CONVERGE && ABC_HASH_MISSSTOP &&
        X(, Hash)($atp(data, ndx)) > hash)
        return NONE;
    return X(HASH, _get)(rec, data, ndx, hash);
}

// HASH-26B02G3r5y: read-only lookup, no copies, no mutation (unlike
// Probe under CONVERGE).  OK: *slot is the resident with rec's key;
// NONE: absent, *slot untouched.
fun ok64 X(HASH, At)(HASH_T const **slot, X($, ) data, HASH_T const *rec) {
    size_t ndx = 0;
    if (OK != X(HASH, _find)(&ndx, data, rec)) return NONE;
    *slot = data[0] + ndx;
    return OK;
}

fun ok64 X(HASH, _probe)(HASH_T **slot, X($, ) data, HASH_T const *rec,
                         u64 hash);

// OK (rec's key now holds rec, or the X(,Merge) fold) or FULL (rec's line
// is full, the table untouched): Probe + Mv, the same layout either way.
fun ok64 X(HASH, Put)(X($, ) data, HASH_T const *rec) {
    HASH_T *slot = NULL;
    ok64 o = X(HASH, _probe)(&slot, data, rec, X(, Hash)(rec));
    if (o == FULL) return o;
#ifdef ABC_HASH_MERGE
    if (o == OK) {
        X(, Merge)(slot, rec);
        return OK;
    }
#endif
    X(, Mv)(slot, rec);
    return OK;
}

// Backward-shift deletion: zero slot ndx, refilling it from later slots
// of the line (copy-before-zero).  An entry moves into the hole iff the
// hole is on its probe path, home..slot, cyclic in the line.
// HASH-26A275TmIX: the scan runs until an empty slot or the hole itself,
// not one lap from the first hole: in a full line holding stale Grow
// copies (their home is another line, here a meaningless offset: they
// move or not, harmless) the last hole could land on the path of an
// entry moved earlier in the lap, losing it.
// HASH-26B02Ggncg: OK if ndx got refilled, NONE if only zeroed.
// YES: the scan ran past one lap, i.e. past the first hole, and may
// have carried an entry from after it to before it (see GrowClean).
fun b8 X(HASH, _shiftlap)(X($, ) data, size_t ndx) {
    size_t base = ndx & ~MASK, at = ndx;
    b8 lap = NO;
    for (size_t x = base + ((ndx + 1) & MASK); x != ndx;
         x = base + ((x + 1) & MASK)) {
        if (X($, is0)(data, x)) break;
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
    return X($, is0)(data, ndx) ? NONE : OK;  // refilled: nonzero
}

// NONE if absent (std::erase count 0); callers that don't care ignore it
fun ok64 X(HASH, Del)(X($, ) data, HASH_T const *rec) {
    size_t ndx = 0;
    ok64 o = X(HASH, _find)(&ndx, data, rec);
    if (o == NONE || o == FULL) return NONE;
    o = X(HASH, _shift)(data, ndx);
    return o == NONE ? OK : o;
}

// HASH-26B02Ggncg: delete the entry at slot ndx, e.g. while iterating.
// OK: a later entry moved into ndx, re-examine the slot before going on;
// NONE: ndx is now empty.  Erase-while-iterating:
//     for (size_t i = 0; OK == Next(&i, data);)
//         if (!match(data[0] + i) || OK != DelAt(data, i)) ++i;
// An unvisited entry only moves into ndx, never before it; a visited
// one (a wrapped line) may move forward and be seen twice.
fun ok64 X(HASH, DelAt)(X($, ) data, size_t ndx) {
    return X(HASH, _shift)(data, ndx);
}

// Iteration: OK and *ndx at the first live slot at or after *ndx, else
// NONE (the table's end).  Start at 0, ++ndx to go on.
fun ok64 X(HASH, Next)(size_t *ndx, X($, ) data) {
    for (; *ndx < (size_t)$len(data); ++*ndx)
        if (!X($, is0)(data, *ndx)) return OK;
    return NONE;
}

// HASH-26A275TmIX: in-place growth.  A growable table is the buffer's
// DATA (pow2 length n >= ABC_HASH_LINE), its IDLE is the room it grows
// into: bMap the max range once, bFed the initial length.  Grow doubles
// DATA into the first n slots of IDLE in three phases, each leaving a
// valid table (stale duplicates, no holes); `data` is the doubled view.
// Phase 1: every entry of a lower line b whose home under 2n is line b+n
// is Put there; the upper half is invisible under length n.  Each upper
// line is zeroed first, so a re-run (and dirty room) is harmless.
fun void X(HASH, _growCopy)(X($, ) data) {
    size_t half = $len(data) >> 1;
    for (size_t b = 0; b < half; b += ABC_HASH_LINE) {
        HASH_T *up = data[0] + half + b;
        for (HASH_T *z = up; z < up + ABC_HASH_LINE; ++z) zerop(z);
        for (HASH_T *p = data[0] + b; p < data[0] + b + ABC_HASH_LINE; ++p) {
            if (X(, IsZero)(p)) continue;
            u64 h = X(, Hash)(p);  // hashed once, Put would rehash
            HASH_T *s = NULL;  // <= LINE into 0s: fits
            if ((h & half) && NONE == X(HASH, _probe)(&s, data, p, h))
                X(, Mv)(s, p);
        }
    }
}

// Phase 3 step: slot ndx of the lower half holds a stale copy (its home
// under the doubled length is in the upper half)?  Delete it by the
// backward shift (copy-before-zero), OK; else NONE.  An entry the
// shift brings into ndx may be stale too: call again.
fun ok64 X(HASH, _unstale)(X($, ) data, size_t ndx) {
    HASH_T const *p = data[0] + ndx;
    if (X(, IsZero)(p) || !(X(, Hash)(p) & ($len(data) >> 1))) return NONE;
    ok64 o = X(HASH, _shift)(data, ndx);
    return o == NONE ? OK : o;
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
// end moves, one word store), phase 3.  NOROOM if IDLE is shorter than
// DATA; never maps.  Never overflows a line: one doubling always fits.
fun ok64 X(HASH, Grow)(X(B, ) buf) {
    sane(buf != NULL && buf[0] != NULL);
    size_t len = X(, bDataLen)(buf);
    test(len >= ABC_HASH_LINE && 0 == (len & (len - 1)), BADARG);
    test(X(, bIdleLen)(buf) >= len, NOROOM);
    X($, ) data = {buf[1], buf[2] + len};
    X(HASH, _growCopy)(data);
    call(X(, bFed), buf, len);
    X(HASH, _growClean)(data);
    done;
}

// HASH-26A275ba4V: rehash every entry of from's DATA into into's DATA,
// a table the caller set up (bMap + bFed, a power-of-2 length >=
// ABC_HASH_LINE, any size: bigger, same, smaller), never maps; into's
// residents stay, a key in both ends up with from's record (Put).
// from is read only; the two tables must not overlap.  Use: the
// fallback when Grow fails NOROOM, shrinking, another ABC_HASH_LINE.
// FULL: an entry's line in into is full; into is left partially
// filled (from's entries before it, in slot order, moved; none lost),
// from intact.
fun ok64 X(HASH, Move)(X(B, ) into, X(B, ) from) {
    sane(into != NULL && into[0] != NULL && from != NULL && from[0] != NULL);
    X($, ) data = {into[1], into[2]};
    size_t len = $len(data);
    test(len >= ABC_HASH_LINE && 0 == (len & (len - 1)), BADARG);
    test(from[2] <= into[1] || into[2] <= from[1], BADARG);
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
        if (o != FULL) return o;
        call(X(HASH, Grow), buf);
    }
}

// HASH-26A274tV3C: get-or-insert in one probe, rec hashed once, never
// copied.  OK: *slot is the resident with rec's key.  NONE: *slot
// is the zeroed slot Put would have put rec into; the caller fills it
// (a nonzero record with rec's key) or Unprobe-s it before any other op
// on the table: under CONVERGE an unfilled slot hides displaced residents.
// FULL: rec's line is full, *slot and the table untouched.
// ABC_HASH_CONVERGE: on NONE the residents Put would have displaced
// are moved first (hashed, as Put does), so Probe+fill leaves the very
// layout Put leaves; a NOROOM evicts nothing.  As Put, no
// guard for a table shorter than ABC_HASH_LINE.
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
            return NONE;
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
    return FULL;
}

fun ok64 X(HASH, Probe)(HASH_T **slot, X($, ) data, HASH_T const *rec) {
    return X(HASH, _probe)(slot, data, rec, X(, Hash)(rec));
}

// HASH-26B02GitHz: undo a NONE Probe left unfilled (lookup-only use,
// an error path): a backward shift at the slot, the Put layout restored.
fun ok64 X(HASH, Unprobe)(X($, ) data, HASH_T *slot) {
    ok64 o = X(HASH, _shift)(data, slot - data[0]);
    return o == NONE ? OK : o;
}

// Probe a growable table (DATA, see Grow), Grow-ing it while rec's line is full: OK or
// NONE as Probe, *slot into the (grown) table; else Grow's failure.
fun ok64 X(HASH, ProbeGrow)(HASH_T **slot, X(B, ) buf, HASH_T const *rec) {
    sane(buf != NULL && buf[0] != NULL);
    for (;;) {
        X($, ) data = {buf[1], buf[2]};
        ok64 o = X(HASH, Probe)(slot, data, rec);
        if (o != FULL) return o;
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
