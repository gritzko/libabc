// HITx.h — Heap of ITerators template
// A HIT is a min-heap of sorted slices (iterators).
// Instantiate with element type: #define X(M, name) M##u64##name
// Entry type: X(,cs) (e.g. u64cs = u64 const *[2])
// Heap type:  X(,csps) — slice of POINTERS to entries (DOG-027); the
//             entries sit in the caller's oldest-first array, never move,
//             and only the pointers permute, so csp order = age.
// Comparator: X(,Z) on element pointers; ties by entry pointer, highest
//             (= youngest run) wins
// Advance:    ++(*entry)[0], eject when $empty(*entry)

#include "OK.h"
#include "S.h"

// DOG-027: LSM policy as C defines — ONE run cap and ONE ladder ratio for
// every entry point and both JS leaves.  Above the cap the caller re-derives.
#ifndef HIT_MAX_RUNS
#define HIT_MAX_RUNS 64   //  a strict ladder never nears it; past it = damaged
#define HIT_LADDER_DIV 8  //  1/8 size-tiered: run i+1 < run i / HIT_LADDER_DIV
con ok64 HITTOOMANY = 0x45275d61858a5e2;  //  "HITTOOMANY": drop + re-derive
#endif

#define HIT_T X(, )
#define HIT_E X(, cs)    // entry = const slice
#define HIT_H X(, csps)  // heap = slice of pointers to entries

// --- Comparator: compare entries by head element ---

// DOG-027: total order — equal heads resolve by entry pointer, highest
// wins (entries are oldest-first, so the highest pointer is youngest).
fun b8 X(HIT, Z)(X(, csp) const *a, X(, csp) const *b) {
    if (X(, Z)((*a)[0], (*b)[0])) return YES;
    if (X(, Z)((*b)[0], (*a)[0])) return NO;
    return *a > *b;
}

fun void X(HIT, Swap)(X(, csp) *a, X(, csp) *b) {
    X(, csp) t = *a;
    *a = *b;
    *b = t;
}

// --- Heap operations ---

fun void X(HIT, Down)(X(, csps) heap, size_t at) {
    size_t n = $len(heap);
    size_t i = at;
    for (;;) {
        size_t left = 2 * i + 1;
        if (left >= n || left < i) break;
        size_t right = left + 1;
        size_t j = left;
        if (right < n && X(HIT, Z)($atp(heap, right), $atp(heap, left)))
            j = right;
        if (!X(HIT, Z)($atp(heap, j), $atp(heap, i))) break;
        X(HIT, Swap)($atp(heap, i), $atp(heap, j));
        i = j;
    }
}

fun void X(HIT, Heap)(X(, csps) heap) {
    size_t n = $len(heap);
    for (size_t i = n / 2; i > 0; --i)
        X(HIT, Down)(heap, i - 1);
}

// --- Eject entry at position ---

fun void X(HIT, Eject)(X(, csps) heap, size_t at) {
    size_t last = $len(heap) - 1;
    if (at != last) X(HIT, Swap)($atp(heap, at), $atp(heap, last));
    --heap[1];
}

// --- Load: compact `runs` in place, build the pointer heap over it ---
// DOG-027: SIDE EFFECT on `runs` — empty entries are dropped, survivors
// shift down keeping age order, runs[1] moves back; the heap aims at those.
// Past HIT_MAX_RUNS the stack is damaged or foreign: HITTOOMANY, no window.

fun ok64 X(HIT, Load)(X(, csps) heap, X(, csp) *slots, X(, css) runs) {
    if ($len(runs) > HIT_MAX_RUNS) return HITTOOMANY;
    heap[0] = heap[1] = slots;
    HIT_E *w = runs[0];
    for (HIT_E *r = runs[0]; r < runs[1]; r++) {
        if ($empty(*r)) continue;
        if (w != r) X(, csMv)(*w, *r);
        *heap[1]++ = *w;
        w++;
    }
    runs[1] = w;
    X(HIT, Heap)(heap);
    return OK;
}

// --- Step: advance top entry, eject if exhausted, re-heapify ---

fun void X(HIT, Step)(X(, csps) heap) {
    ++(*heap[0])[0];
    if ($empty(*heap[0])) {
        X(HIT, Eject)(heap, 0);
        if ($empty(heap)) return;
    }
    X(HIT, Down)(heap, 0);
}

// --- Tops: find all entries with head equal to minimum ---
// Moves them to front of heap. Returns count.

fun size_t X(HIT, Tops)(X(, csps) heap) {
    size_t l = $len(heap);
    if (l == 0) return 0;
    size_t eqlen = 1;
    size_t lim = 2;
    for (size_t i = 1; i < l && i <= lim; ++i) {
        // DOG-027: element compare — the total-order Z would exclude
        // equal heads that lose the pointer tiebreak
        if (X(, Z)((*$atp(heap, 0))[0], (*$atp(heap, i))[0])) continue;
        if (eqlen != i) {
            X(HIT, Swap)($atp(heap, eqlen), $atp(heap, i));
            X(HIT, Down)(heap, i);
            --i;
        } else {
            lim = i * 2 + 2;
        }
        eqlen++;
    }
    return eqlen;
}

// --- AdvanceTops: advance ntops entries, eject exhausted ---

fun void X(HIT, AdvanceTops)(X(, csps) heap, size_t ntops) {
    for (size_t j = ntops; j > 0; --j) {
        size_t i = j - 1;
        ++(*$atp(heap, i))[0];
        if ($empty(*$atp(heap, i))) {
            X(HIT, Eject)(heap, i);
            if ((i64)i < $len(heap)) X(HIT, Down)(heap, i);
        } else {
            X(HIT, Down)(heap, i);
        }
    }
}

// --- MergeBag: drain heap producing sorted output (keeps duplicates) ---
// ABC-015: drains write into a bounded slice (head advances past the
// output) and return OKNOROOM when it fills.

fun ok64 X(HIT, MergeBag)(X(, css) runs, X(, s) into) {
    X(, csp) slots[HIT_MAX_RUNS];
    X(, csps) heap;
    ok64 lo = X(HIT, Load)(heap, slots, runs);
    if (lo != OK) return lo;
    while (!$empty(heap)) {
        if ($empty(into)) return OKNOROOM;
        *into[0]++ = *(*heap[0])[0];
        X(HIT, Step)(heap);
    }
    return OK;
}

// --- Merge: drain heap producing sorted deduplicated output ---

fun ok64 X(HIT, Merge)(X(, css) runs, X(, s) into) {
    X(, csp) slots[HIT_MAX_RUNS];
    X(, csps) heap;
    ok64 lo = X(HIT, Load)(heap, slots, runs);
    if (lo != OK) return lo;
    while (!$empty(heap)) {
        HIT_T val = *(*heap[0])[0];
        if ($empty(into)) return OKNOROOM;
        *into[0]++ = val;
        size_t ntops = X(HIT, Tops)(heap);
        X(HIT, AdvanceTops)(heap, ntops);
        // skip remaining duplicates
        while (!$empty(heap) && !X(, Z)((*heap[0])[0], &val)
                              && !X(, Z)(&val, (*heap[0])[0]))
            X(HIT, Step)(heap);
    }
    return OK;
}

// --- Intersect: emit values present in ALL nruns iterators ---

fun ok64 X(HIT, Intersect)(X(, css) runs, X(, s) into, size_t nruns) {
    X(, csp) slots[HIT_MAX_RUNS];
    X(, csps) heap;
    ok64 lo = X(HIT, Load)(heap, slots, runs);
    if (lo != OK) return lo;
    while (!$empty(heap)) {
        size_t ntops = X(HIT, Tops)(heap);
        HIT_T val = *(*heap[0])[0];
        if (ntops >= nruns) {
            if ($empty(into)) return OKNOROOM;
            *into[0]++ = val;
        }
        X(HIT, AdvanceTops)(heap, ntops);
        // skip remaining duplicates of val
        while (!$empty(heap) && !X(, Z)((*heap[0])[0], &val)
                              && !X(, Z)(&val, (*heap[0])[0]))
            X(HIT, Step)(heap);
    }
    return OK;
}

// --- Seek: advance all entries until heap top >= key ---

fun ok64 X(HIT, Seek)(X(, css) runs, X(, cp) key) {
    X(, csp) slots[HIT_MAX_RUNS];
    X(, csps) heap;
    ok64 lo = X(HIT, Load)(heap, slots, runs);
    if (lo != OK) return lo;
    while (!$empty(heap) && X(, Z)((*heap[0])[0], key)) {
        X(, c) *const run[2] = {(*heap[0])[0], (*heap[0])[1]};
        X(, c) *pos = X(, sFindGE)(run, key);
        (*heap[0])[0] = pos;
        if ($empty(*heap[0])) {
            X(HIT, Eject)(heap, 0);
            if ($empty(heap)) break;
        }
        X(HIT, Down)(heap, 0);
    }
    return $empty(heap) ? NODATA : OK;  // DOG-027: the heap is the live set
}

// --- SeekRange: trim all entries to [lo, hi), eject empty ---
// After this, the entries only contain elements in [lo, hi); a drain
// (Load + Step) emits them in order with no prefix checks needed.

fun ok64 X(HIT, SeekRange)(X(, css) heap, X(, cp) lo, X(, cp) hi) {
    if ($len(heap) > HIT_MAX_RUNS) return HITTOOMANY;  // DOG-027: uniform cap
    HIT_E *w = heap[0];
    for (HIT_E *r = heap[0]; r < heap[1]; r++) {
        if ($empty(*r)) continue;
        HIT_T *sub[2];
        X(, sFindRange)(sub, *r, lo, hi);
        if (sub[0] < sub[1]) {
            (*w)[0] = sub[0];
            (*w)[1] = sub[1];
            w++;
        }
    }
    heap[1] = w;
    if ($empty(heap)) return NODATA;
    return OK;
}

// --- SkipValue: advance inner HIT past its current top merged value ---
// DOG-027: rebuilds the pointer heap per call — an inner HIT's state
// lives entirely in its (oldest-first, in-place-advanced) entries.

fun ok64 X(HIT, SkipValue)(X(, css) runs) {
    X(, csp) slots[HIT_MAX_RUNS];
    X(, csps) inner;
    ok64 lo = X(HIT, Load)(inner, slots, runs);
    if (lo != OK) return lo;
    if ($empty(inner)) return OK;
    HIT_T val = *(*inner[0])[0];
    size_t ntops = X(HIT, Tops)(inner);
    X(HIT, AdvanceTops)(inner, ntops);
    while (!$empty(inner) && !X(, Z)((*inner[0])[0], &val)
                           && !X(, Z)(&val, (*inner[0])[0]))
        X(HIT, Step)(inner);
    // DOG-027: re-Load to compact `runs` — sIntersectMerge's cssTop reads
    // the inner's entries directly, with no Load of its own to filter them.
    return X(HIT, Load)(inner, slots, runs);
}

// --- IntersectMerge: intersect the merged outputs of N inner HITs ---
// Each inner HIT is a X(,css) (heap of sorted runs producing merged output).
// The outer heap walks N such inner HITs in lockstep: emit a value only
// when ALL inner HITs produce it. Zero intermediate allocations.
//
// Usage:
//   u64cs *outers[N][2];   // N inner HITs
//   outers[i][0] = runs_i; outers[i][1] = runs_i + nruns_i;
//   u64csss oh = {outers, outers + N};
//   u64 buf[...]; u64p out = buf;
//   HITu64sIntersectMerge(oh, &out);

typedef X(, css) *X(, csss)[2];

// Top value pointer of an inner HIT
// DOG-027: entries are no longer heap-ordered — scan for the min head.
fun X(, cp) X(HIT, cssTop)(X(, css) *hit) {
    X(, cp) m = (*(*hit)[0])[0];
    for (X(, cs) *r = (*hit)[0] + 1; r < (*hit)[1]; r++)
        if (X(, Z)((*r)[0], m)) m = (*r)[0];
    return m;
}

fun void X(, cssSwap)(X(, css) *a, X(, css) *b) {
    X(, cs) *t0 = (*a)[0], *t1 = (*a)[1];
    (*a)[0] = (*b)[0]; (*a)[1] = (*b)[1];
    (*b)[0] = t0; (*b)[1] = t1;
}

fun void X(HIT, cssDown)(X(, csss) oh, size_t at) {
    size_t n = (size_t)$len(oh);
    size_t i = at;
    for (;;) {
        size_t left = 2 * i + 1;
        if (left >= n || left < i) break;
        size_t right = left + 1;
        size_t j = left;
        if (right < n && X(, Z)(X(HIT, cssTop)($atp(oh, right)),
                                 X(HIT, cssTop)($atp(oh, left))))
            j = right;
        if (!X(, Z)(X(HIT, cssTop)($atp(oh, j)),
                     X(HIT, cssTop)($atp(oh, i)))) break;
        X(, cssSwap)($atp(oh, i), $atp(oh, j));
        i = j;
    }
}

fun size_t X(HIT, cssTops)(X(, csss) oh) {
    size_t l = (size_t)$len(oh);
    if (l == 0) return 0;
    size_t eqlen = 1;
    size_t lim = 2;
    for (size_t i = 1; i < l && i <= lim; ++i) {
        if (X(, Z)(X(HIT, cssTop)($atp(oh, 0)),
                    X(HIT, cssTop)($atp(oh, i)))) continue;
        if (eqlen != i) {
            X(, cssSwap)($atp(oh, eqlen), $atp(oh, i));
            X(HIT, cssDown)(oh, i);
            --i;
        } else {
            lim = i * 2 + 2;
        }
        eqlen++;
    }
    return eqlen;
}

fun ok64 X(HIT, sIntersectMerge)(X(, csss) oheap, X(, s) into) {
    // Filter empty inner HITs
    X(, css) *w = oheap[0];
    for (X(, css) *r = oheap[0]; r < oheap[1]; r++) {
        if (!$empty(*r)) {
            if (w != r) X(, cssSwap)(w, r);
            w++;
        }
    }
    oheap[1] = w;
    size_t nruns = (size_t)$len(oheap);
    if (nruns == 0) return OK;

    // Heapify outer
    for (size_t i = nruns / 2; i > 0; --i)
        X(HIT, cssDown)(oheap, i - 1);

    while ((size_t)$len(oheap) == nruns) {
        size_t ntops = X(HIT, cssTops)(oheap);
        HIT_T val = *X(HIT, cssTop)($atp(oheap, 0));

        if (ntops >= nruns) {
            if ($empty(into)) return OKNOROOM;
            *into[0]++ = val;
        }

        // Advance top ntops inner HITs past val
        for (size_t j = ntops; j > 0; --j) {
            size_t i = j - 1;
            ok64 so = X(HIT, SkipValue)(*$atp(oheap, i));
            if (so != OK) return so;
            if ($empty(*$atp(oheap, i))) return OK;
            X(HIT, cssDown)(oheap, i);
        }
    }
    return OK;
}

// --- LSM-stack compaction (1/8 size-tiered ladder) ---
//
// `stack` is an oldest-first sequence of sorted runs (each run is
// itself sorted; the stack as a whole is *not* — we maintain the
// 1/8 invariant: each newer (higher-index) run is strictly less
// than 1/8 of its predecessor).  IsCompact is the predicate; Compact
// merges the youngest runs that violate it into a single sorted+
// dedup'd run, cascading until the invariant holds again.  The merge
// uses HIT's stream-merge-with-dedup (HITMerge), so
// identical full-element rows across runs collapse to one.

// DOG-027: HIT is now the ONE ladder home — MSET is deleted, so ABC-015's
// deferred "unify IsCompact/Compact into one home" follow-up is closed.
fun b8 X(HIT, IsCompact)(X(, css) stack) {
    size_t n = $len(stack);
    for (size_t i = 0; i + 1 < n; i++) {
        if ($len(stack[0][i + 1]) * HIT_LADDER_DIV > $len(stack[0][i]))
            return NO;
    }
    return YES;
}

// Merge youngest runs to restore the 1/8 invariant.  Stack is
// oldest-first; youngest at the tail.  Cascades: if merging tail m
// runs still violates the invariant against the run before them,
// includes that run too, and so on.  `into` is a slice of free
// space; its head advances past the merged elements.  The merged
// run is spliced back into the stack at position n-m, and the
// stack's idle pointer is moved up so $len(stack) drops by m-1.
fun ok64 X(HIT, Compact)(X(, css) stack, X(, s) into) {
    size_t n = $len(stack);
    if (n > HIT_MAX_RUNS) return HITTOOMANY;  // DOG-027: uniform cap
    if (n < 2) return OK;
    size_t m = 1;
    size_t total = $len(stack[0][n - 1]);
    while (m < n && (i64)(total * HIT_LADDER_DIV) > $len(stack[0][n - 1 - m])) {
        total += $len(stack[0][n - 1 - m]);
        m++;
    }
    if (m < 2) return OK;
    if ($len(into) < (i64)total) return OKNOROOM;
    HIT_T *base = *into;
    X(, css) sub = {stack[0] + (n - m), stack[0] + n};
    ok64 o = X(HIT, Merge)(sub, into);
    if (o != OK) return o;
    stack[0][n - m][0] = base;
    stack[0][n - m][1] = *into;
    stack[1] = stack[0] + (n - m + 1);
    return OK;
}

#undef HIT_H
#undef HIT_E
#undef HIT_T
