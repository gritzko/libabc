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

#define T X(, )

#ifndef ABC_HASH_LINE
#define ABC_HASH_LINE 16
#endif

#ifndef ABC_HASH_CONVERGE
#define ABC_HASH_CONVERGE 0
#endif

#define MASK (ABC_HASH_LINE - 1)
#define LMASK (X(, sLen)(data) - 1)

// HASHx uses X(,hashEq) for key-equivalence (matches X(,hash) keying:
// for kv-like records, hashEq is key-equality; for plain scalars it
// coincides with full equality).  Each instantiating type defines
// `X(,hashEq)` next to its `X(,hash)`.
fun ok64 X(HASH, scan)(size_t *ndx, X($, ) data, T const *rec) {
    sane($ok(data) && ndx != NULL && 0 == ($len(data) & MASK));
    size_t off = (*ndx) & MASK;
    size_t base = (*ndx) & ~MASK;
    for (size_t i = off + 1; i < off + ABC_HASH_LINE; ++i) {
        *ndx = base + (i & MASK);
        if (X($, is0)(data, *ndx)) return HASHNONE;
        // ABC-015: (rec, slot) arg order, same as Get/Del below
        if (X(, hashEq)(rec, *data + *ndx)) return OK;
    }
    return HASHNOROOM;
}

// OK, HASHNONE, HASHNOROOM
fun ok64 X(HASH, find)(size_t *ndx, X($, ) data, T const *rec) {
    u64 hash = X(, hash)(rec);
    *ndx = hash & LMASK;
    if (X(, hashEq)(rec, $atp(data, *ndx))) return OK;
    if (X($, is0)(data, *ndx)) return HASHNONE;
    return X(HASH, scan)(ndx, data, rec);
}

fun ok64 X(HASH, _get)(T *rec, X($, ) data, size_t ndx) {
    size_t off = ndx & MASK;
    size_t base = ndx & ~MASK;
    for (size_t i = (off + 1) & MASK; i != off; i = (i + 1) & MASK) {
        ndx = base + i;
        if (X(, hashEq)(rec, $atp(data, ndx))) {
            *rec = $at(data, ndx);
            return OK;
        }
        if (X($, is0)(data, ndx)) return HASHNONE;
    }
    return HASHNONE;
}

fun ok64 X(HASH, Get)(T *rec, X($, ) data) {
    u64 hash = X(, hash)(rec);
    size_t ndx = hash & LMASK;
    if (X($, is0)(data, ndx)) return HASHNONE;
    if (X(, hashEq)(rec, $atp(data, ndx))) {
        *rec = $at(data, ndx);  // TODO mv
        return OK;
    }
    return X(HASH, _get)(rec, data, ndx);
}

fun ok64 X(HASH, _put)(T const *rec, X($, ) data, size_t hash) {
    T r;
    X(, mv)(&r, rec);
    size_t fit = hash & LMASK;
    size_t off = fit & MASK;
    size_t base = fit & ~MASK;
    for (size_t i = 0; i < ABC_HASH_LINE; ++i) {
        size_t ndx = base + ((off + i) & MASK);
        if (X($, is0)(data, ndx) || X(, hashEq)(&r, $atp(data, ndx))) {
            X(, mv)($atp(data, ndx), &r);
            return OK;
        }
        u64 hash2 = X(, hash)($atp(data, ndx));
        // ABC-015: after Swap the carried record is the ex-occupant, so
        // carry its hash too; a stale hash made layout depend on op order
        if (ABC_HASH_CONVERGE && hash2 > hash) {
            X(, Swap)(&r, $atp(data, ndx));
            hash = hash2;
        }
    }
    return HASHNOROOM;
}

fun ok64 X(HASH, Put)(X($, ) data, T const *rec) {
    u64 hash = X(, hash)(rec);
    size_t ndx = hash & LMASK;
    if (X($, is0)(data, ndx) || X(, hashEq)(rec, $atp(data, ndx))) {
        X(, mv)($atp(data, ndx), rec);
        return OK;
    }
    return X(HASH, _put)(rec, data, hash);
}

// Backward-shift deletion: zero slot ndx, refilling it from later slots
// of the line (copy-before-zero).  An entry moves into the hole iff the
// hole is on its probe path, home..slot, cyclic in the line.
// HASH-26A275TmIX: the scan runs until an empty slot or the hole itself,
// not one lap from the first hole: in a full line holding stale Grow
// copies (their home is another line, here a meaningless offset: they
// move or not, harmless) the last hole could land on the path of an
// entry moved earlier in the lap, losing it.
fun ok64 X(HASH, shift)(X($, ) data, size_t ndx) {
    sane($ok(data) && (i64)ndx < $len(data));
    size_t base = ndx & ~MASK;
    for (size_t x = base + ((ndx + 1) & MASK); x != ndx;
         x = base + ((x + 1) & MASK)) {
        if (X($, is0)(data, x)) break;
        size_t nom = X(, hash)(*data + x) & LMASK;
        if (((ndx - nom) & MASK) < ((x - nom) & MASK)) {
            Bat(data, ndx) = Bat(data, x);
            ndx = x;
        }
    }
    zero(Bat(data, ndx));
    done;
}

// HASHNONE if absent (std::erase count 0); callers that don't care ignore it
fun ok64 X(HASH, Del)(X($, ) data, T const *rec) {
    size_t ndx = 0;
    ok64 o = X(HASH, find)(&ndx, data, rec);
    if (o == HASHNONE || o == HASHNOROOM) return HASHNONE;
    return X(HASH, shift)(data, ndx);
}

// HASH-26A275TmIX: in-place growth.  A growable table is the buffer's
// DATA (pow2 length n >= ABC_HASH_LINE), its IDLE is the room it grows
// into: bMap the max range once, bFed the initial length.  Grow doubles
// DATA into the first n slots of IDLE in three phases, each leaving a
// valid table (stale duplicates, no holes); `data` is the doubled view.
// Phase 1: every entry of a lower line b whose home under 2n is line b+n
// is Put there; the upper half is invisible under length n.  Each upper
// line is zeroed first, so a re-run (and dirty room) is harmless.
fun void X(HASH, GrowCopy)(X($, ) data) {
    size_t half = $len(data) >> 1;
    for (size_t b = 0; b < half; b += ABC_HASH_LINE) {
        T *up = data[0] + half + b;
        for (T *z = up; z < up + ABC_HASH_LINE; ++z) zerop(z);
        for (T *p = data[0] + b; p < data[0] + b + ABC_HASH_LINE; ++p)
            if (!X(, IsZero)(p) && (X(, hash)(p) & half))
                X(HASH, Put)(data, p);  // <= LINE into a zero line: fits
    }
}

// Phase 3 step: slot ndx of the lower half holds a stale copy (its home
// under the doubled length is in the upper half)?  Delete it by the
// backward shift (copy-before-zero), OK; else HASHNONE.  An entry the
// shift brings into ndx may be stale too: call again.
fun ok64 X(HASH, unstale)(X($, ) data, size_t ndx) {
    T const *p = data[0] + ndx;
    if (X(, IsZero)(p) || !(X(, hash)(p) & ($len(data) >> 1))) return HASHNONE;
    return X(HASH, shift)(data, ndx);
}

// Phase 3: unstale the whole lower half, line by line.  A shift running
// past one lap may carry a not yet checked entry back before ndx, so a
// line is swept again until a sweep finds nothing.  Idempotent.
fun void X(HASH, GrowClean)(X($, ) data) {
    size_t half = $len(data) >> 1;
    for (size_t b = 0; b < half; b += ABC_HASH_LINE) {
        b8 dirty = YES;
        while (dirty) {
            dirty = NO;
            for (size_t i = b; i < b + ABC_HASH_LINE; ++i)
                while (OK == X(HASH, unstale)(data, i)) dirty = YES;
        }
    }
}

// Grow the table (DATA) 2x in place: phase 1, phase 2 (the flip: DATA's
// end moves, one word store), phase 3.  BNOROOM if IDLE is shorter than
// DATA; never maps.  Never overflows a line: one doubling always fits.
fun ok64 X(HASH, Grow)(X(B, ) buf) {
    sane(buf != NULL && buf[0] != NULL);
    size_t len = X(, bDataLen)(buf);
    test(len >= ABC_HASH_LINE && 0 == (len & (len - 1)), FAILSANITY);
    test(X(, bIdleLen)(buf) >= len, BNOROOM);
    X($, ) data = {buf[1], buf[2] + len};
    X(HASH, GrowCopy)(data);
    call(X(, bFed), buf, len);
    X(HASH, GrowClean)(data);
    done;
}

// HASH-26A275ba4V: rehash every entry of from's DATA into into's DATA,
// a table the caller set up (bMap + bFed, a power-of-2 length >=
// ABC_HASH_LINE, any size: bigger, same, smaller), never maps; into's
// residents stay, a key in both ends up with from's record (Put).
// from is read only; the two tables must not overlap.  Use: the
// fallback when Grow fails BNOROOM, shrinking, another ABC_HASH_LINE.
// HASHNOROOM: an entry's line in into is full; into is left partially
// filled (from's entries before it, in slot order, moved; none lost,
// as find is probed before each Put), from intact.
fun ok64 X(HASH, Move)(X(B, ) into, X(B, ) from) {
    sane(into != NULL && into[0] != NULL && from != NULL && from[0] != NULL);
    X($, ) data = {into[1], into[2]};
    size_t len = $len(data);
    test(len >= ABC_HASH_LINE && 0 == (len & (len - 1)), FAILSANITY);
    test(from[2] <= into[1] || into[2] <= from[1], FAILSANITY);
    for (T const *p = from[1]; p < from[2]; ++p) {
        if (X(, IsZero)(p)) continue;
        size_t ndx = 0;
        if (HASHNOROOM == X(HASH, find)(&ndx, data, p)) fail(HASHNOROOM);
        call(X(HASH, Put), data, p);
    }
    done;
}

// Put into a growable table (DATA, see Grow), Grow-ing first if rec's
// line is full.
// Probes with find before Put: with ABC_HASH_CONVERGE a NOROOM Put
// has already swapped some resident out, i.e. lost it.
fun ok64 X(HASH, PutGrow)(X(B, ) buf, T const *rec) {
    sane(buf != NULL && buf[0] != NULL);
    size_t ndx = 0;
    for (;;) {
        X($, ) data = {buf[1], buf[2]};
        if (HASHNOROOM != X(HASH, find)(&ndx, data, rec))
            return X(HASH, Put)(data, rec);
        call(X(HASH, Grow), buf);
    }
}

// HASH-26A274tV3C: get-or-insert in one probe, rec hashed once, never
// copied.  OK: *slot is the resident with rec's key.  HASHNONE: *slot
// is the zeroed slot Put would have put rec into; the caller fills it
// (a nonzero record with rec's key) or Unprobe-s it before any other op
// on the table: under CONVERGE an unfilled slot hides displaced residents.
// HASHNOROOM: rec's line is full, *slot and the table untouched.
// ABC_HASH_CONVERGE: on HASHNONE the residents Put would have displaced
// are moved first (hashed, as Put does), so Probe+fill leaves the very
// layout Put leaves; unlike Put, a NOROOM evicts nothing.  As Put, no
// guard for a table shorter than ABC_HASH_LINE.
fun ok64 X(HASH, Probe)(T **slot, X($, ) data, T const *rec) {
    u64 hash = X(, hash)(rec);
    size_t fit = hash & LMASK;
    size_t off = fit & MASK;
    T *line = data[0] + (fit & ~MASK);
    size_t disp = ABC_HASH_LINE;  // CONVERGE: rec displaces this one
    u64 dhash = 0;
    for (size_t i = 0; i < ABC_HASH_LINE; ++i) {
        T *s = line + ((off + i) & MASK);
        if (X(, IsZero)(s)) {
            if (ABC_HASH_CONVERGE && disp < i) {
                T *d = line + ((off + disp) & MASK);
                T r;
                X(, mv)(&r, d);
                zerop(d);
                for (size_t j = disp + 1; j <= i; ++j) {
                    T *c = line + ((off + j) & MASK);
                    if (X(, IsZero)(c)) {
                        X(, mv)(c, &r);
                        break;
                    }
                    u64 h2 = X(, hash)(c);
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
        if (X(, hashEq)(rec, s)) {
            *slot = s;
            return OK;
        }
        if (ABC_HASH_CONVERGE && disp == ABC_HASH_LINE) {
            u64 h2 = X(, hash)(s);
            if (h2 > hash) {
                disp = i;
                dhash = h2;
            }
        }
    }
    return HASHNOROOM;
}

// HASH-26B02GitHz: undo a HASHNONE Probe left unfilled (lookup-only use,
// an error path): a backward shift at the slot, the Put layout restored.
fun ok64 X(HASH, Unprobe)(X($, ) data, T *slot) {
    return X(HASH, shift)(data, slot - data[0]);
}

// Probe a growable table (DATA, see Grow), Grow-ing it while rec's line is full: OK or
// HASHNONE as Probe, *slot into the (grown) table; else Grow's failure.
fun ok64 X(HASH, ProbeGrow)(T **slot, X(B, ) buf, T const *rec) {
    sane(buf != NULL && buf[0] != NULL);
    for (;;) {
        X($, ) data = {buf[1], buf[2]};
        ok64 o = X(HASH, Probe)(slot, data, rec);
        if (o != HASHNOROOM) return o;
        call(X(HASH, Grow), buf);
    }
}

#undef MASK
#undef LMASK
#undef ABC_HASH_LINE
// ABC-015: per-instantiation knob, undef like ABC_HASH_LINE
#undef ABC_HASH_CONVERGE
#undef T
