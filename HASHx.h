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

fun ok64 X(HASH, shift)(X($, ) data, size_t ndx) {
    sane($ok(data) && (i64)ndx < $len(data));
    size_t off = ndx & MASK;
    size_t base = ndx & ~MASK;
    for (size_t i = off + 1; i < off + ABC_HASH_LINE; ++i) {
        size_t x = base + (i & MASK);  // all pow 2
        if (X($, is0)(data, x)) {
            break;
        }
        size_t nom = X(, hash)(*data + x) & LMASK;
        if ((x > ndx && (nom <= ndx || nom > x)) ||
            (x < ndx && (nom <= ndx && nom > x))) {
            Bat(data, ndx) = Bat(data, x);
            ndx = x;
        }
    }
    zero(Bat(data, ndx));
    done;
}

fun ok64 X(HASH, Del)(X($, ) data, T const *rec) {
    size_t ndx = 0;
    ok64 o = X(HASH, find)(&ndx, data, rec);
    if (o == HASHNONE || o == HASHNOROOM) return OK;
    return X(HASH, shift)(data, ndx);
}

// HASH-26A26Cq06k: grow a bMap-ed table.  The table is the buffer's
// idle slice (the whole map, as X(,bMap) leaves it).  Map a range of
// len*2 (pow2), re-Put every live entry scanning the old, unmap the old.
// Should a line still overflow, the new map is dropped and len doubled
// again.  Needs X(,bMap)/X(,bUnMap) (Bx.h instantiated for T).
fun ok64 X(HASH, Grow)(X(B, ) buf) {
    sane(buf != NULL && buf[0] != NULL && buf[0] == buf[2]);
    X($, ) old = {buf[2], buf[3]};
    size_t len = $len(old);
    test(len >= ABC_HASH_LINE && 0 == (len & (len - 1)), FAILSANITY);
    for (len <<= 1; len != 0; len <<= 1) {
        X(B, ) nb = {};
        call(X(, bMap), nb, len);
        X($, ) data = {nb[2], nb[3]};
        ok64 o = OK;
        for (T *p = old[0]; p < old[1] && o == OK; ++p)
            if (!X($, is0)(old, p - old[0])) o = X(HASH, Put)(data, p);
        if (o == OK) {
            call(X(, bUnMap), buf);
            for (int i = 0; i < 4; ++i) buf[i] = nb[i];
            done;
        }
        call(X(, bUnMap), nb);
        if (o != HASHNOROOM) fail(o);
    }
    fail(HASHNOROOM);
}

// Put into a bMap-ed table, Grow-ing first if rec's line is full.
// Probes with find before Put: with ABC_HASH_CONVERGE a NOROOM Put
// has already swapped some resident out, i.e. lost it.
fun ok64 X(HASH, PutGrow)(X(B, ) buf, T const *rec) {
    sane(buf != NULL && buf[0] != NULL);
    size_t ndx = 0;
    for (;;) {
        X($, ) data = {buf[2], buf[3]};
        if (HASHNOROOM != X(HASH, find)(&ndx, data, rec))
            return X(HASH, Put)(data, rec);
        call(X(HASH, Grow), buf);
    }
}

// HASH-26A274tV3C: get-or-insert in one probe, rec hashed once, never
// copied.  OK: *slot is the resident with rec's key.  HASHNONE: *slot
// is the zeroed slot Put would have put rec into; the caller fills it
// (a nonzero record with rec's key) before any other op on the table.
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

// Probe a bMap-ed table, Grow-ing it while rec's line is full: OK or
// HASHNONE as Probe, *slot into the (grown) table; else Grow's failure.
fun ok64 X(HASH, ProbeGrow)(T **slot, X(B, ) buf, T const *rec) {
    sane(buf != NULL && buf[0] != NULL);
    for (;;) {
        X($, ) data = {buf[2], buf[3]};
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
