// HITx.h — Heap of ITerators template: a min-heap of sorted slices (iterators).
// Instantiate with the element type: #define X(M, name) M##u64##name
// Entry type: X(,cs) (e.g. u64cs = u64 const *[2])
// Heap type:  X(,csps) — slice of POINTERS to entries (DOG-027); the entries
//             sit in the caller's oldest-first array, never move, and only
//             the pointers permute, so csp order = age.
// Comparator: X(,Z) on element pointers; ties by entry pointer, highest
//             (= youngest run) wins.  Advance: ++(*entry)[0], eject when empty.

#include "B.h"
#include "HIT.h"
#include "OK.h"
#include "S.h"

#ifdef ABC_HIT_MERGE
#error "ABC_HIT_MERGE is retired: ABC_X_MERGE (HIT.h)"
#endif

#define HIT_T X(, )
#define HIT_E X(, cs)    // entry = const slice
#define HIT_H X(, csps)  // heap = slice of pointers to entries

// The heap's buffer X(,cspb) (Sx.h): the pointers live in its DATA.

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

// --- Load: compact `runs` IN PLACE, build the pointer heap over it ---
// DOG-027: empties drop out of `runs`, runs[1] moves back; past the cap: FULL.

fun ok64 X(HIT, Load)(X(, cspb) heap, X(, css) runs) {
    if ($len(runs) > heap[3] - heap[0]) return FULL;
    heap[1] = heap[2] = heap[0];
    HIT_E *w = runs[0];
    for (HIT_E *r = runs[0]; r < runs[1]; r++) {
        if ($empty(*r)) continue;
        if (w != r) X(, csMv)(*w, *r);
        *heap[2]++ = *w;
        w++;
    }
    runs[1] = w;
    X(HIT, Heap)(heap + 1);
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

// --- MergeBag: drain heap into `into` (destination first, ABC-26B02JHe_k) ---
// ABC-015: sorted, duplicates kept, head advances; NOROOM when `into` fills.

fun ok64 X(HIT, MergeBag)(X(, s) into, X(, css) runs) {
    a_pad(X(, csp), slots, ABC_HIT_MAX_RUNS);
    X(, csp) **heap = slots_data;
    ok64 lo = X(HIT, Load)(slots, runs);
    if (lo != OK) return lo;
    while (!$empty(heap)) {
        if ($empty(into)) return NOROOM;
        *into[0]++ = *(*heap[0])[0];
        X(HIT, Step)(heap);
    }
    return OK;
}

// --- Merge: drain heap into sorted deduplicated output; equal keys: the
// youngest run's row wins, or with ABC_X_MERGE they fold oldest first ---

#ifdef ABC_X_MERGE
//  fold the ntops equal heads in age order (entry pointer order)
fun void X(HIT, _fold)(HIT_T *acc, X(, csps) heap, size_t ntops) {
    X(, csp) prev = NULL;
    for (size_t k = 0; k < ntops; k++) {
        X(, csp) next = NULL;
        for (size_t i = 0; i < ntops; i++) {
            X(, csp) e = heap[0][i];
            if ((!prev || e > prev) && (!next || e < next)) next = e;
        }
        if (k == 0)
            *acc = *next[0];
        else
            X(, Merge)(acc, next[0]);
        prev = next;
    }
}
#endif

fun ok64 X(HIT, Merge)(X(, s) into, X(, css) runs) {
    a_pad(X(, csp), slots, ABC_HIT_MAX_RUNS);
    X(, csp) **heap = slots_data;
    ok64 lo = X(HIT, Load)(slots, runs);
    if (lo != OK) return lo;
    while (!$empty(heap)) {
        if ($empty(into)) return NOROOM;
        size_t ntops = X(HIT, Tops)(heap);
        HIT_T val = *(*heap[0])[0];
#ifdef ABC_X_MERGE
        X(HIT, _fold)(&val, heap, ntops);
#endif
        *into[0]++ = val;
        X(HIT, AdvanceTops)(heap, ntops);
        // skip remaining duplicates
        while (!$empty(heap) && X(, KeyEq)((*heap[0])[0], &val))
            X(HIT, Step)(heap);
    }
    return OK;
}

// --- Intersect: emit values present in ALL nruns iterators ---

fun ok64 X(HIT, Intersect)(X(, s) into, X(, css) runs, size_t nruns) {
    a_pad(X(, csp), slots, ABC_HIT_MAX_RUNS);
    X(, csp) **heap = slots_data;
    ok64 lo = X(HIT, Load)(slots, runs);
    if (lo != OK) return lo;
    while (!$empty(heap)) {
        size_t ntops = X(HIT, Tops)(heap);
        HIT_T val = *(*heap[0])[0];
        if (ntops >= nruns) {
            if ($empty(into)) return NOROOM;
            *into[0]++ = val;
        }
        X(HIT, AdvanceTops)(heap, ntops);
        // skip remaining duplicates of val
        while (!$empty(heap) && X(, KeyEq)((*heap[0])[0], &val))
            X(HIT, Step)(heap);
    }
    return OK;
}

// --- Seek: advance all entries until heap top >= key ---

fun ok64 X(HIT, Seek)(X(, css) runs, X(, cp) key) {
    a_pad(X(, csp), slots, ABC_HIT_MAX_RUNS);
    X(, csp) **heap = slots_data;
    ok64 lo = X(HIT, Load)(slots, runs);
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
    return $empty(heap) ? NONE : OK;  // DOG-027: the heap is the live set
}

// --- SeekRange: trim all entries to [lo, hi), eject empty; a drain (Load +
// Step) then emits them in order with no prefix checks ---

fun ok64 X(HIT, SeekRange)(X(, css) heap, X(, cp) lo, X(, cp) hi) {
    if ($len(heap) > ABC_HIT_MAX_RUNS) return FULL;  // DOG-027: uniform cap
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
    if ($empty(heap)) return NONE;
    return OK;
}

// --- SkipValue: advance inner HIT past its current top merged value ---
// DOG-027: rebuilds the pointer heap per call; the state lives in the entries.

fun ok64 X(HIT, SkipValue)(X(, css) runs) {
    a_pad(X(, csp), slots, ABC_HIT_MAX_RUNS);
    X(, csp) **inner = slots_data;
    ok64 lo = X(HIT, Load)(slots, runs);
    if (lo != OK) return lo;
    if ($empty(inner)) return OK;
    HIT_T val = *(*inner[0])[0];
    size_t ntops = X(HIT, Tops)(inner);
    X(HIT, AdvanceTops)(inner, ntops);
    while (!$empty(inner) && X(, KeyEq)((*inner[0])[0], &val))
        X(HIT, Step)(inner);
    // DOG-027: re-Load to compact `runs` — sIntersectMerge's cssTop reads
    // the inner's entries directly, with no Load of its own to filter them.
    return X(HIT, Load)(slots, runs);
}

// --- IntersectMerge: intersect the merged outputs of N inner HITs (each a
// X(,css)) in lockstep, no intermediate allocation; usage in HIT.md ---

// Top value pointer of an inner HIT
// DOG-027: entries are no longer heap-ordered — scan for the min head.
fun X(, cp) X(HIT, _cssTop)(X(, css) *hit) {
    X(, cp) m = (*(*hit)[0])[0];
    for (X(, cs) *r = (*hit)[0] + 1; r < (*hit)[1]; r++)
        if (X(, Z)((*r)[0], m)) m = (*r)[0];
    return m;
}

fun void X(HIT, _cssSwap)(X(, css) *a, X(, css) *b) {
    X(, cs) *t0 = (*a)[0], *t1 = (*a)[1];
    (*a)[0] = (*b)[0]; (*a)[1] = (*b)[1];
    (*b)[0] = t0; (*b)[1] = t1;
}

fun void X(HIT, _cssDown)(X(, csss) oh, size_t at) {
    size_t n = (size_t)$len(oh);
    size_t i = at;
    for (;;) {
        size_t left = 2 * i + 1;
        if (left >= n || left < i) break;
        size_t right = left + 1;
        size_t j = left;
        if (right < n && X(, Z)(X(HIT, _cssTop)($atp(oh, right)),
                                 X(HIT, _cssTop)($atp(oh, left))))
            j = right;
        if (!X(, Z)(X(HIT, _cssTop)($atp(oh, j)),
                     X(HIT, _cssTop)($atp(oh, i)))) break;
        X(HIT, _cssSwap)($atp(oh, i), $atp(oh, j));
        i = j;
    }
}

fun size_t X(HIT, _cssTops)(X(, csss) oh) {
    size_t l = (size_t)$len(oh);
    if (l == 0) return 0;
    size_t eqlen = 1;
    size_t lim = 2;
    for (size_t i = 1; i < l && i <= lim; ++i) {
        if (X(, Z)(X(HIT, _cssTop)($atp(oh, 0)),
                    X(HIT, _cssTop)($atp(oh, i)))) continue;
        if (eqlen != i) {
            X(HIT, _cssSwap)($atp(oh, eqlen), $atp(oh, i));
            X(HIT, _cssDown)(oh, i);
            --i;
        } else {
            lim = i * 2 + 2;
        }
        eqlen++;
    }
    return eqlen;
}

fun ok64 X(HIT, sIntersectMerge)(X(, s) into, X(, csss) oheap) {
    // Filter empty inner HITs
    X(, css) *w = oheap[0];
    for (X(, css) *r = oheap[0]; r < oheap[1]; r++) {
        if (!$empty(*r)) {
            if (w != r) X(HIT, _cssSwap)(w, r);
            w++;
        }
    }
    oheap[1] = w;
    size_t nruns = (size_t)$len(oheap);
    if (nruns == 0) return OK;

    // Heapify outer
    for (size_t i = nruns / 2; i > 0; --i)
        X(HIT, _cssDown)(oheap, i - 1);

    while ((size_t)$len(oheap) == nruns) {
        size_t ntops = X(HIT, _cssTops)(oheap);
        HIT_T val = *X(HIT, _cssTop)($atp(oheap, 0));

        if (ntops >= nruns) {
            if ($empty(into)) return NOROOM;
            *into[0]++ = val;
        }

        // Advance top ntops inner HITs past val
        for (size_t j = ntops; j > 0; --j) {
            size_t i = j - 1;
            ok64 so = X(HIT, SkipValue)(*$atp(oheap, i));
            if (so != OK) return so;
            if ($empty(*$atp(oheap, i))) return OK;
            X(HIT, _cssDown)(oheap, i);
        }
    }
    return OK;
}

// --- LSM-stack compaction: the 1/8 size-tiered ladder over an oldest-first
// stack of sorted runs; IsCompact tests it, Compact merges to restore it ---

// DOG-027: HIT is now the ONE ladder home — MSET is deleted, so ABC-015's
// deferred "unify IsCompact/Compact into one home" follow-up is closed.
// The predicate itself is comparator-free (HIT.h); this measures the runs.
// Past ABC_HIT_MAX_RUNS the stack is damaged, hence never compact.
fun b8 X(HIT, IsCompact)(X(, css) stack) {
    a_pad(u64, lens, ABC_HIT_MAX_RUNS);
    $for(HIT_E, r, stack) if (u64bFeed1(lens, (u64)$len(*r)) != OK) return NO;
    return HITLadderOK(u64bDataC(lens));
}

// DOG-027: the cascade count alone: how many youngest runs the ladder must
// collapse to restore the 1/8 invariant (0 when it already holds).  The
// arithmetic is HITLadderOverRuns (HIT.h, no comparator); this only measures
// the runs, so a length-only caller like dog's Pup calls HIT.h directly.
fun size_t X(HIT, CompactRuns)(X(, css) stack) {
    a_pad(u64, lens, ABC_HIT_MAX_RUNS);
    $for(HIT_E, r, stack) if (u64bFeed1(lens, (u64)$len(*r)) != OK) return 0;
    return HITLadderOverRuns(u64bDataC(lens));
}

fun ok64 X(HIT, Compact)(X(, css) stack, X(, s) into) {
    size_t n = $len(stack);
    if (n > ABC_HIT_MAX_RUNS) return FULL;  // DOG-027: uniform cap
    size_t m = X(HIT, CompactRuns)(stack);
    if (m < 2) return OK;
    size_t total = 0;
    for (size_t i = n - m; i < n; i++) total += (size_t)$len(stack[0][i]);
    if ($len(into) < (i64)total) return NOROOM;
    HIT_T *base = *into;
    X(, css) sub = {stack[0] + (n - m), stack[0] + n};
    ok64 o = X(HIT, Merge)(into, sub);
    if (o != OK) return o;
    stack[0][n - m][0] = base;
    stack[0][n - m][1] = *into;
    stack[1] = stack[0] + (n - m + 1);
    return OK;
}

#undef HIT_H
#undef HIT_E
#undef HIT_T
#ifndef ABC_X_NEST
#undef ABC_X_MERGE
#endif
