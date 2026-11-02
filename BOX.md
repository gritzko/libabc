#   BOX container

`BOXx.h` is an in-memory sorted container of fixed-size records: one
record buffer plus one buffer of run slices, an LSM in two buffers.  It
never allocates; the caller maps both, the BOX fills them.  The PUPS
memtable runs on it (`PUPx.h`).

    #define X(M, name) M##kv64##name
    #include "HITx.h"              // BOX feeds its runs to HIT as they are
    #define ABC_BOX_SEG 512        // optional: the segment, BYTES
    #include "BOXx.h"
    #undef X

##  Layout

 -  `recs` (`X(,b)`): PAST holds the sorted runs back to back, oldest
    first; DATA the unsorted segment the puts append to; IDLE the room.
    `bMap` the max range once.
 -  `runs` (`X(,csb)`): DATA is one slice per run, oldest first, each
    run ascending by `X(,Z)` with at most one row per key.  The slots are
    the caller's array, e.g. `a_pad(kv64cs, runs, ABC_HIT_MAX_RUNS)`.

A zeroed pair of buffers is an empty BOX.

##  The record type

The key view of the depot's wiki/Containers.mkd: `X(,Z)` the key order,
`X(,KeyEq)` key equality, `X(,Max)` the greatest key under Z (all-ones
unsigned, INT_MAX signed, +INF float; NaN keys are banned).  A row with
the Max key is the tombstone: no real record carries that key, Put
refuses it (`BADARG`), tested by key via Z, never by memcmp.  Bx, QSORTx
and HITx are instantiated for T first.

##  API

Codes are the shared ones (01.h).  A failed write stores nothing.

  - `Put(recs, runs, &rec)`: append to the segment; past `ABC_BOX_SEG`
    bytes the segment becomes a run (`Sort`).  `BADARG` a Max-key row,
    `NOROOM` recs has no IDLE (the owner grows or moves it), `FULL` the
    seal finds no free run slot or `ABC_HIT_MAX_RUNS` runs.
  - `PutBatch(recs, runs, rows, n)`: seal the segment, then the batch is
    ONE run (qsort, equal keys folded in no particular order); codes as Put.
  - `Sort(recs, runs, all)`: seal the segment into a run, collapse the
    tailing runs the 1/8 ladder selects, or all of them with `all`.
    `FULL` as Put, checked before anything changes.
  - `Get(&out, recs, runs, &key)`: point lookup over the runs and the
    segment, the youngest row wins (or the `X(,Merge)` fold); `NONE`.
  - `Range(into, recs, runs, &lo, &hi)`: seal, then fill `into` (a slice
    of run slices) with every run trimmed to `[lo, hi)`, a HITx input as
    it is; `NOROOM` if `into` is shorter than the run count.

Equal keys: the younger row wins, unless `ABC_BOX_MERGE` is defined; then
`X(,Merge)(acc, younger)` folds them in age order (in Sort, Get and the
segment fold alike).  Use the same hook for HIT (`ABC_HIT_MERGE`) when
the runs get merged downstream.

##  Knobs

Per instantiation, undefined at the end of BOXx.h:

  - `ABC_BOX_SEG` the unsorted segment's length in bytes, default 512;
    a static_assert keeps it at one record or more.
  - `ABC_BOX_MERGE` (defined: on) fold equal keys by `X(,Merge)`.

The retired `BOXx_C`, `BOXx_MERGE` are a compile error.

##  Load and complexity

A Put is O(1) amortized plus a segment sort every `ABC_BOX_SEG` bytes;
the 1/8 ladder keeps O(log n) runs (base 8), each collapse a parallel
in-place pass, a qsort of the selected range and a trim of the Max tail.
Runs already in order (ascending puts) are joined without a pass.  A
Get is a binary search per run plus a scan of the segment.

##  Deletion and iteration

There is no Del: a "this key is gone" row is a value the caller encodes
(PUPS `PUPx_DEAD`), dropped by a merge that spans the whole stack.  Rows
folded away in a collapse become Max rows and are trimmed.  Iterate by
`Range` plus a HITx drain; nothing moves under a reader until the next
Put or Sort.
