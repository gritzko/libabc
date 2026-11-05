#   BOX container

`BOXx.h` is an in-memory sorted container of fixed-size records: an LSM
held in one "house", a buffer of 64 run slices (PUPS-26B045Ffpb).  It
never allocates; the caller maps the memtable, the BOX fills it.  The
PUPS memtable runs on it (`PUPx.h`).

    #define X(M, name) M##kv64##name
    #define ABC_BOX_SEG 512        // optional: the segment, BYTES
    #include "BOXx.h"
    #undef X

    a_box(kv64, h);                            // 1 KB on the stack, zeroed
    kv64bMap(BOXkv64Mem(h), rows);             // the owner maps the memtable
    BOXkv64Put(h, &rec);
    kv64bUnMap(BOXkv64Mem(h));

##  Layout

The house `h` is an `X(,csb)` over `ABC_HIT_MAX_RUNS` cells (one cell is
an `X(,cs)`, two pointers).  Its term is cell 62, so cells 62..63 are
the memtable, an `X(,b)` that never moves; `X(BOX, Mem)(h)` reads it.

 -  PAST: file runs, PUPS's; BOX ignores them.
 -  DATA: BOX's sorted runs, oldest first, each ascending by `X(,Z)`
    with one row per key, slices into the memtable's PAST.
 -  IDLE: free cells.
 -  The memtable: PAST the sorted runs back to back, DATA the sorted
    segment the puts insert into, IDLE the room.  `bMap` the max range once.

`a_box(T, h)` declares the cells and the header; `PUP_HOUSE(cells)` is
the header initializer for an owner that keeps the cells in a struct.
A zeroed house is an empty BOX.

##  The record type

The key view of the depot's wiki/Containers.mkd: `X(,Z)` the key order,
`X(,KeyEq)` key equality, `X(,Max)` the greatest key under Z (all-ones
unsigned, INT_MAX signed, +INF float; NaN keys are banned).  A row with
the Max key is the tombstone: no real record carries that key, Put
refuses it (`BADARG`), tested by key via Z, never by memcmp.  Bx, QSORTx
are instantiated for T first; BOXx.h includes its own HITx.h.

##  API

Codes are the shared ones (01.h).  A failed write stores nothing.

  - `Put(h, &rec)`: insert into the sorted segment, an equal key folded
    in place; past `ABC_BOX_SEG` bytes the segment becomes a run
    (`Sort`).  `BADARG` a Max-key row, `NOROOM`
    the memtable has no IDLE (the owner commits, grows or moves it),
    `FULL` the seal finds no free cell.
  - `PutBatch(h, rows)`: seal the segment, then the `X(,csc)` batch is
    ONE run (qsort, equal keys folded in no particular order); codes as Put.
  - `Sort(h, all)`: seal the segment into a run, collapse the tailing
    runs the 1/8 ladder selects, or all of them with `all`.  `FULL` as
    Put, checked before anything changes.
  - `Get(&out, h, &key)`: point lookup over the DATA runs and the
    segment, the youngest row wins (or the `X(,Merge)` fold); `NONE`.
  - `Range(into, h, &lo, &hi)`: seal, then fill `into` (a slice of run
    slices) with every DATA run trimmed to `[lo, hi)`, a HITx input as it
    is; `NOROOM` if `into` is shorter than the run count.
  - `Mem(h)`: the memtable buffer, for the owner's `bMap`/`bUnMap`.

Equal keys: the younger row wins, unless `ABC_X_MERGE` is defined; then
`X(,Merge)(acc, younger)` folds them in age order (in Sort, Get and the
segment insert alike), and the same knob makes the HIT merge of the runs
fold too.

##  Knobs

Per instantiation, undefined at the end of BOXx.h:

  - `ABC_BOX_SEG` the sorted segment's length in bytes, default 512;
    a static_assert keeps it at one record or more.
  - `ABC_X_MERGE` (defined: on) fold equal keys by `X(,Merge)`; one knob
    for the instance's HITx, BOXx and PUPx (HIT.h), undefined by the
    outermost header.

The retired `BOXx_C`, `BOXx_MERGE`, `ABC_BOX_MERGE` are a compile error.

##  Load and complexity

A Put is an insertion step into the `ABC_BOX_SEG` segment;
the 1/8 ladder keeps O(log n) runs (base 8), each collapse a HIT merge
into IDLE and a copy back, or with no room for it a parallel in-place
pass, a qsort of the selected range and a trim of the Max tail.
Runs already in order (ascending puts) are joined without a pass.  A
Get is a binary search per run and one of the segment.  The house
caps the runs at 62 (64 binary searches per Get is already a lot).

##  Deletion and iteration

There is no Del: a "this key is gone" row is a value the caller encodes
(PUPS `PUPx_DEAD`), dropped by a merge that spans the whole stack.  Rows
folded away in a collapse become Max rows and are trimmed.  Iterate by
`Range` plus a HITx drain; nothing moves under a reader until the next
Put or Sort.
