# HITx.h — Heap of ITerators

A HIT is a min-heap of sorted slices (iterators). Instantiate with the
**element** type; all slice/heap types are derived automatically.

```c
#define X(M, name) M##u64##name
#include "HITx.h"
#undef X
```

There are no prerequisites beyond Sx (DOG-027: the heap permutes entry
POINTERS, so `X(,csSwap)` is no longer needed).

The entry points take an oldest-first `X(,css)` of runs, whose entries
never move; each builds its own `HIT_MAX_RUNS`-slot `X(,csps)` pointer
heap.  Equal head elements (a keyed lane's genuine ties) resolve to the
highest entry pointer — the youngest run — in Merge, Seek, SeekRange's
drain and Compact alike.

## Policy defines (DOG-027)

The policy lives in non-templated `HIT.h`: the ladder reads run LENGTHS
only, so it takes no element type and no comparator.  A caller that needs
just the ladder — dog's fs-level Pup, whose "runs" are files of packed
rows — includes `HIT.h` and passes byte lengths; it must NOT instantiate
an element-typed HIT it would never merge with.

| Define           | Value | Meaning                                   |
|------------------|-------|-------------------------------------------|
| `HIT_MAX_RUNS`   | 64    | runs per merge; ONE cap for every entry point and both JS leaves |
| `HIT_LADDER_DIV` | 8     | the 1/8 size-tiered ladder ratio          |
| `HITTOOMANY`     | —     | the `ok64` every entry point returns above the cap |

| Function (HIT.h)      | Description                                  |
|-----------------------|----------------------------------------------|
| `HITLadderOK(lens)`   | 1/8 predicate over oldest-first run lengths  |
| `HITLadderOverRuns(lens)` | how many youngest runs to collapse (0 = none) |

`runs[]` is OLDEST-FIRST and that is positional, not checkable: a
newest-first array silently inverts newest-wins on a keyed lane.  The
caller owns the convention (in JS, the `abc.index` handle, which holds
each run's pup key).

Above `HIT_MAX_RUNS` an entry point just refuses with `HITTOOMANY` — no
windowing, no repair cascade, no youngest-N batching.  A stack that deep
is damaged or foreign; the cure is to drop the runs and re-derive.

## Types

| Derived type | Example   | Meaning                          |
|-------------|-----------|----------------------------------|
| X(,cs)      | u64cs     | Entry = const slice (iterator)   |
| X(,css)     | u64css    | Oldest-first slice of entries    |
| X(,csps)    | u64csps   | Heap = slice of entry pointers (internal) |
| X(,csss)    | u64csss   | Slice of heaps (for IntersectMerge) |

## Functions

### Core

| Function            | Description                            |
|--------------------|----------------------------------------|
| `HITTStep(heap)`   | Advance top, eject if done, sift down  |
| `HITTMerge(heap, &out)` | Sorted deduplicated drain         |
| `HITTIntersect(heap, &out, n)` | Emit only values in all N entries |
| `HITTSeek(heap, &key)` | Binary-search advance to key       |
| `HITTIsCompact(stack)` | measures the stack, then `HITLadderOK` (NO above the cap) |
| `HITTCompactRuns(stack)` | measures the stack, then `HITLadderOverRuns` (0 above the cap) |
| `HITTCompact(stack, into)` | merge that tail, splice the result back |

### IntersectMerge (HIT-of-HITs)

| Function                        | Description                       |
|--------------------------------|-----------------------------------|
| `HITTsIntersectMerge(oh, &out)` | Intersect merged outputs of N inner HITs |
| `HITTSkipValue(inner)`          | Advance inner HIT past current top value (`ok64`) |

## Merge

Drains the heap producing a sorted, deduplicated sequence. Each step
pops the minimum, advances all entries that had that value, skips
remaining duplicates.

```c
u64cs runs[3] = {{a, a+4}, {b, b+3}, {c, c+5}};
u64css heap = {runs, runs + 3};
u64 buf[64]; u64p out = buf;
HITu64Merge(heap, &out);
// buf[0..out-buf) contains sorted deduplicated merge
```

## Intersect

Like Merge, but only emits values present in ALL N iterators.

```c
u64 buf[64]; u64p out = buf;
HITu64Intersect(heap, &out, 3);  // 3 = number of runs
```

## IntersectMerge (HIT-of-HITs)

Intersects the merged outputs of N inner HITs without intermediate
buffers. Each inner HIT is a heap of sorted runs (e.g. mmapped index
files). The outer heap walks them in lockstep: emit a value only when
all inner HITs produce it.

```c
u64cs *outers[N][2];   // N inner HITs
outers[i][0] = runs_i; outers[i][1] = runs_i + nruns_i;

u64csss oh = {outers, outers + N};
u64 buf[...]; u64p out = buf;
HITu64sIntersectMerge(oh, &out);
```

## Seek

Advances all entries until the heap top is >= key, using binary search
within each entry.

```c
u64 key = 42;
ok64 o = HITu64Seek(heap, &key);  // NODATA if all exhausted
```
