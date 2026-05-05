//  abc/BOX.h — staging memtable for puppy-style sorted runs.
//
//  A BOX is a fixed-capacity byte range partitioned into one unsorted
//  *dirty* region followed by a 1/8-ladder of sorted runs.  New
//  entries land in dirty; when dirty fills, it is sorted and cascade-
//  merged into the lowest sorted level whose chunk holds the
//  (post-dedup) merged result.  When even the top level cannot
//  absorb, BOXFULL is returned — the caller is expected to Flush.
//
//  Layout inside the caller-provided byte range:
//
//    [ dirty 1 KB | L0 1 KB | L1 8 KB | L2 64 KB | ... | L_top: rest ]
//
//  Number of sorted levels is derived from the range length so the
//  template never hardwires a layer count.  The top level absorbs
//  any slack.  The default 1 KB dirty / 8× ratio can be overridden
//  per-instantiation by defining BOX_DIRTY_BYTES / BOX_RATIO before
//  including this header.  Both macros are #undef'd at the end so
//  the next instantiation gets a clean slate.
//
//  Empty-slot sentinel is the all-zeros entry: chunks past their
//  current data are zero, and a level's data extent is recovered on
//  Open by scanning forward for the first zero — same convention as
//  HASHx.  Real entries must satisfy `!X(,IsZero)(rec)`; the
//  instantiating type is responsible for upholding that invariant.
//
//  The descriptor buffer is a `Tsb` on the caller's stack laid out
//  per ABC PAST/DATA/IDLE convention, with one twist: BOX uses both
//  PAST and IDLE for one populated slice each.
//
//    PAST  = box[0..1] : the dirty level's Ts (one descriptor)
//    DATA  = box[1..2] : sorted levels' Ts entries, oldest-first
//    IDLE  = box[2..3] : starts with a *fence* Ts {range_end,range_end}
//                        followed by any unused descriptor slots
//
//  The fence in IDLE makes `data[i+1].start` work uniformly to read
//  level `i`'s chunk end, including the top level whose chunk end is
//  the byte range's end.  HIT's filter-empty pass skips the fence
//  naturally, so callers can still feed `X(,sDataC)(box)` straight
//  to HITx for lookup, range scan, or full sweep without a wrapper.
//
//  Like HASHx and HITx, BOX is a pure data-structure template: it
//  knows nothing about allocation, mmap, or files.  The caller
//  supplies the byte range, decides where it lives, and chooses
//  what to do with the sorted output of Flush/Close.
//
//  Instantiation prerequisites: Bx, QSORTx, and HITx must already
//  be instantiated for the same type before BOX.h is included.
//
//      #define X(M, name) M##u64##name
//      #include "abc/Bx.h"
//      #include "abc/QSORTx.h"
//      #include "abc/HITx.h"
//      #include "abc/BOX.h"
//      #undef X
//
//  Required environment: X(,Z), X(,IsZero), X(,sSort), X(HIT,Start),
//  X(HIT,Merge).

#include "B.h"
#include "BOX.h"
#include "OK.h"
#include "S.h"

#define BOX_T X(, )

#ifndef BOX_DIRTY_BYTES
#define BOX_DIRTY_BYTES 1024
#endif

#ifndef BOX_RATIO
#define BOX_RATIO 8
#endif

#ifndef BOX_MAX_LEVELS
#define BOX_MAX_LEVELS 32
#endif

#define BOX_DIRTY_N (BOX_DIRTY_BYTES / sizeof(BOX_T))

// --- Geometry ----------------------------------------------------------
//
//  Walk the ladder forward from the dirty chunk; stop when the next
//  step would overflow the byte budget.  The top level absorbs any
//  remaining slack so the entire range is partitioned.  When `caps`
//  is non-NULL it must hold at least the returned count of size_t
//  slots; on return, `caps[i]` is level `i`'s capacity in T entries.

fun size_t X(BOX, Levels)(size_t range_bytes, size_t *caps) {
    if (range_bytes < BOX_DIRTY_BYTES * 2) return 0;
    size_t budget = range_bytes - BOX_DIRTY_BYTES;
    size_t step = BOX_DIRTY_BYTES;
    size_t n = 0;
    while (step <= budget && n < BOX_MAX_LEVELS) {
        if (caps) caps[n] = step / sizeof(BOX_T);
        budget -= step;
        n++;
        if (step > ((size_t)-1) / BOX_RATIO) break;
        step *= BOX_RATIO;
    }
    if (n == 0) return 0;
    if (caps) caps[n - 1] += budget / sizeof(BOX_T);
    return n;
}

// --- Open --------------------------------------------------------------
//
//  Carve `range` into `dirty | L0 | L1 | ... | L_top` and populate
//  `box` with one Ts per region.  Each Ts's `start` is fixed at its
//  chunk's start; `end` is recovered by scanning forward for the
//  first zero so re-attaching to a previously-used range restores
//  the BOX's logical state.  An additional fence Ts
//  {range_end, range_end} is written at IDLE's head.
//
//  The caller's `box` must have capacity for at least `2 + n_levels`
//  Ts slots.  Returns BOXFAIL when range is too small or box too
//  short.

fun ok64 X(BOX, Open)(X(, sb) box, u8s range) {
    sane(box != NULL && range != NULL);
    size_t total = (size_t)((u8c *)range[1] - (u8c *)range[0]);
    size_t caps[BOX_MAX_LEVELS];
    size_t n = X(BOX, Levels)(total, caps);
    if (n == 0) return BOXFAIL;

    //  Capacity check: dirty + sorted levels + fence.
    X($, ) **bb = (X($, ) **)box;
    size_t descs = (size_t)(bb[3] - bb[0]);
    if (descs < 2 + n) return BOXFAIL;

    BOX_T *base = (BOX_T *)range[0];
    X($, ) *slot = bb[0];

    //  PAST: dirty.
    BOX_T *cs = base;
    BOX_T *ce = cs + BOX_DIRTY_N;
    BOX_T *q = cs;
    while (q < ce && !X(, IsZero)(q)) q++;
    (*slot)[0] = cs;
    (*slot)[1] = q;
    slot++;

    //  DATA: each sorted level.
    BOX_T *cursor = ce;
    for (size_t i = 0; i < n; i++) {
        cs = cursor;
        ce = cs + caps[i];
        q = cs;
        while (q < ce && !X(, IsZero)(q)) q++;
        (*slot)[0] = cs;
        (*slot)[1] = q;
        slot++;
        cursor = ce;
    }

    //  IDLE head: fence.  cursor now equals range_end (in T units).
    (*slot)[0] = cursor;
    (*slot)[1] = cursor;

    bb[1] = bb[0] + 1;
    bb[2] = bb[0] + 1 + n;
    return OK;
}

// --- Feed1 -------------------------------------------------------------
//
//  Append one entry into the dirty level.  When dirty fills, sort it
//  and cascade-merge dirty + every populated level below the chosen
//  target into the lowest empty target whose chunk capacity holds
//  the merged result.  If even the top level can't absorb, returns
//  BOXFULL — caller must Flush, then retry.

fun ok64 X(BOX, Feed1)(X(, sb) box, BOX_T const *rec) {
    //  zero is reserved as empty-slot sentinel; caller must not feed it
    sane(box != NULL && rec != NULL && !X(, IsZero)(rec));

    X($, ) **bb = (X($, ) **)box;
    X($, ) *dirty = bb[0];
    BOX_T *de = (*dirty)[1];
    BOX_T *dterm = (*dirty)[0] + BOX_DIRTY_N;

    if (de < dterm) {
        *de = *rec;
        (*dirty)[1] = de + 1;
        return OK;
    }

    //  Dirty full: sort.
    {
        X(, s) ds = {(*dirty)[0], (*dirty)[1]};
        X(, sSort)(ds);
    }

    X($, ) *data = bb[1];
    X($, ) *idle = bb[2];   // first IDLE slot is the fence
    size_t n = (size_t)(idle - data);

    //  Walk DATA forward.  Accumulate every populated level we pass
    //  through; the first empty level whose chunk capacity holds the
    //  accumulated total (dirty + lower populated levels) is the
    //  cascade target.
    size_t merged = (size_t)((*dirty)[1] - (*dirty)[0]);
    size_t target = n;
    for (size_t i = 0; i < n; i++) {
        BOX_T *lcs = data[i][0];
        BOX_T *lce = data[i + 1][0];   // fence guarantees i+1 is valid
        size_t cap = (size_t)(lce - lcs);
        size_t cur = (size_t)(data[i][1] - lcs);
        if (cur == 0 && cap >= merged) { target = i; break; }
        merged += cur;
    }
    if (target == n) return BOXFULL;

    //  Build a HIT input from dirty + every level below target.  Do
    //  the merge into the target chunk, then zero each source
    //  chunk's data tail and reset its Ts.
    X(, cs) runs[BOX_MAX_LEVELS + 1];
    size_t nruns = 0;
    runs[nruns][0] = (*dirty)[0];
    runs[nruns][1] = (*dirty)[1];
    nruns++;
    for (size_t i = 0; i < target; i++) {
        runs[nruns][0] = data[i][0];
        runs[nruns][1] = data[i][1];
        nruns++;
    }

    //  Merge into target's chunk.  HITMerge writes through `out`
    //  pointer-to-pointer; it advances `*out` past the produced
    //  output.  We then read back the new data extent.
    X(, css) heap = {runs, runs + nruns};
    X(HIT, Start)(heap);
    BOX_T *out = data[target][0];
    X(HIT, Merge)(heap, &out);
    data[target][1] = out;

    //  Zero the dirty buffer and the source levels' data.  Required
    //  so a future Open's first-zero scan recovers the right extent
    //  (after this cascade those chunks are logically empty).
    {
        size_t z = (size_t)((*dirty)[1] - (*dirty)[0]);
        for (size_t i = 0; i < z; i++) (*dirty)[0][i] = (BOX_T){0};
        (*dirty)[1] = (*dirty)[0];
    }
    for (size_t i = 0; i < target; i++) {
        size_t z = (size_t)(data[i][1] - data[i][0]);
        for (size_t k = 0; k < z; k++) data[i][0][k] = (BOX_T){0};
        data[i][1] = data[i][0];
    }

    //  Append the triggering record to the now-empty dirty.
    *((*dirty)[1]) = *rec;
    (*dirty)[1] += 1;
    return OK;
}

// --- Flush -------------------------------------------------------------
//
//  Drain dirty + every sorted level into `save` as one merged
//  deduplicated sorted run.  Advances `save[0]` past the produced
//  entries (caller reads off the produced length via the slice
//  difference).  Resets every level Ts to empty and zeros each
//  chunk's data tail, so the BOX is ready for fresh inserts.
//
//  Returns BOXNOROOM when `save` is too small to hold the merged
//  output (caller can size `save` >= total_entries upfront).

fun ok64 X(BOX, Flush)(X(, sb) box, X(, s) save) {
    sane(box != NULL && save != NULL);
    X($, ) **bb = (X($, ) **)box;
    X($, ) *dirty = bb[0];
    X($, ) *data = bb[1];
    X($, ) *idle = bb[2];
    size_t n = (size_t)(idle - data);

    //  Sort dirty before merging (HIT requires sorted runs).
    if ((*dirty)[1] > (*dirty)[0]) {
        X(, s) ds = {(*dirty)[0], (*dirty)[1]};
        X(, sSort)(ds);
    }

    //  Build runs: dirty + every non-empty DATA level.
    X(, cs) runs[BOX_MAX_LEVELS + 1];
    size_t nruns = 0;
    if ((*dirty)[1] > (*dirty)[0]) {
        runs[nruns][0] = (*dirty)[0];
        runs[nruns][1] = (*dirty)[1];
        nruns++;
    }
    for (size_t i = 0; i < n; i++) {
        if (data[i][1] > data[i][0]) {
            runs[nruns][0] = data[i][0];
            runs[nruns][1] = data[i][1];
            nruns++;
        }
    }
    if (nruns == 0) return OK;   // empty box

    //  Conservative pre-flight: ensure save has room for the sum of
    //  all source lengths (dedup may produce less, never more).
    size_t total = 0;
    for (size_t i = 0; i < nruns; i++)
        total += (size_t)(runs[i][1] - runs[i][0]);
    if ((size_t)(save[1] - save[0]) < total) return BOXNOROOM;

    X(, css) heap = {runs, runs + nruns};
    X(HIT, Start)(heap);
    X(HIT, Merge)(heap, save);

    //  Zero everything and reset Ts.
    {
        size_t z = (size_t)((*dirty)[1] - (*dirty)[0]);
        for (size_t i = 0; i < z; i++) (*dirty)[0][i] = (BOX_T){0};
        (*dirty)[1] = (*dirty)[0];
    }
    for (size_t i = 0; i < n; i++) {
        size_t z = (size_t)(data[i][1] - data[i][0]);
        for (size_t k = 0; k < z; k++) data[i][0][k] = (BOX_T){0};
        data[i][1] = data[i][0];
    }
    return OK;
}

// --- Close -------------------------------------------------------------
//
//  Flush, then collapse the descriptor buffer: DATA becomes empty,
//  PAST stays one slot (dirty), IDLE absorbs every prior DATA slot
//  along with the fence.  Caller must Open again before reuse.

fun ok64 X(BOX, Close)(X(, sb) box, X(, s) save) {
    ok64 o = X(BOX, Flush)(box, save);
    if (o != OK) return o;
    X($, ) **bb = (X($, ) **)box;
    bb[2] = bb[1];   // collapse DATA to empty
    return OK;
}

#undef BOX_DIRTY_N
#undef BOX_MAX_LEVELS
#undef BOX_RATIO
#undef BOX_DIRTY_BYTES
#undef BOX_T
