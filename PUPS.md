#   libpups: a basic LSM-ish store for small fixed-size records

A very generic, low-level utility library. It stores **small records of a
fixed size and a fixed bit layout**, ordered by a **comparator the caller
supplies**. It sits *below* key-value stores: it knows no keys, values or
schemas, only bytes, a record width and an ordering. It is part of abc
and uses only abc's own pieces (`HIT`, `QSORTx`, `RON`, `FILE`).

### How it works

 -  **Runs.** A stack of immutable files in one directory, each named
    `<seqno><ext>` (`seqno`: 10-char zero-padded RON64). Every file is a
    sorted array of records, mmap'd read-only. `ext` is the caller's, and it
    also names the on-disk format, so one directory can hold several stacks.
 -  **Memtable.** `PUPPut` appends a record to a small in-memory page
    (`PUPPutMem` sizes it in OS pages for bulk loads). It is never sorted
    on the way in.
 -  **Commit.** `PUPCommit` sorts and dedups the memtable (keep-last),
    writes it to a temp file and renames it to `<seqno><ext>`. The rename is
    atomic: a crash leaves a dot-file or a complete run, never a half-run.
    `PUPCommitAs` leaves the fsync to the caller.
 -  **Ladder.** Every seal ends in `PUPLadder`, which keeps run sizes in a
    1/8 ladder by merging runs that fall out of proportion, so the run count
    stays small. A merged run never reuses a name: it takes the next free key
    past its youngest source (fsync'd tmp, atomic rename), then the sources
    are unlinked oldest first, so a crash at any step reads the same.
 -  **Reads.** `PUPAllRuns` lists the committed runs oldest to newest, then
    the memtable; the caller merges them (newest wins). The stack is
    re-scanned on commit, so concurrent writers on one directory see each
    other's runs and never collide on a name.
 -  **ToC (optional).** A sparse page index at the end of a run
    (`[data][toc][len:5]["TC"]`): one cell per OS page, a copy of that page's
    first record. A lookup searches the cells with the comparator, then only
    the page the cell names. Built only when a record does not straddle a
    page and the run spans at least two pages. Derived data: dropping it
    leaves a valid run.

### The record type

The core (`PUPS.c`) is compiled once and sees only bytes and a record width.
The type-dependent parts come from a template, `PUPx.h`, instantiated per
record type `T` the way abc's `HITx.h` is:

    #define X(M, name) M##u64##name
    #include "abc/PUPx.h"
    #undef X

This generates, using `T`'s comparator `<T>Z` and abc's `QSORTx` and `HITx`
for `T` (instantiated beforehand):

 -  `PUP<T>Sync`: sort the memtable and drop duplicates (newest wins); on
    `collapse`, fold the older part into the newer.
 -  `PUP<T>Merge`: merge sorted run slices into one; `bottom` says the slices
    are the whole stack (see Deletes).
 -  Typed entry points `PUP<T>Put`, `PutMem`, `Commit`, `CommitAs`, `Ladder`,
    `AllRuns`, `AllRunsToc`. Each calls the core with `Sync`, `Merge` and
    `sizeof(T)` filled in, e.g. `PUPu64Commit(pups, dir, ext)`.

Both generated functions work on the whole range at once, so the core calls
them through a pointer once per operation. The core functions (`PUPCommit` and
friends) are public and take `(sync, merge, row)` directly, for byte stacks or
a record width of 0.

### Write lock

Writers on one directory take an advisory `flock` on `<dir>/.lock` around what a
rescan cannot make safe: a seal (key pick, then rename onto it), a create, and a
whole ladder. One lock per DIRECTORY, not per stack: a forward and a reverse
stack are one update, so `PUPLock`/`PUPUnlock` let a caller hold the lock across
several stacks (DAT does, in `DATCommit`). The entry points take it themselves;
it is reentrant within a process (not thread-safe), the kernel drops it when
the process dies, and reads never take it. `.lock` is not a run: a scan skips it.

A rescan also remaps a run whose file another writer replaced (an older ladder
renamed its merge onto the youngest source's name; since PUPS-005 none does). Known limit (DOG-046): a long-lived handle keeps the mappings of
runs other writers unlinked, so after more than `HIT_MAX_RUNS` of them a view
overflows (`SNOROOM`); reopen the stack.

### Deletes

The core has no delete; a row says "this key is gone" through a tombstone the
record type defines. Define `PUPx_DEAD(row)` (a macro over `T const *`) before
including `PUPx.h`. Then:

 -  `PUP<T>Read` returns the merged view of the stack, newest row per key,
    without dead rows.
 -  A ladder merge that takes every run, with no inherited PAST (`bottom`),
    drops dead rows and, by keep-last, the rows they shadowed. A partial merge
    keeps them: an older run may still hold the key. A merge that leaves
    nothing writes no run at all.
 -  Without `PUPx_DEAD` nothing is ever dead and `bottom` changes nothing.

A hand-written `pupmerge` takes the extra `bottom` argument and may ignore it.
Dropping is safe while new runs of other writers carry younger keys than the
runs they follow; the ladder rescans the directory first (DOG-046).

Define `PUPx_WHOLE_ROW 1` before the include when `<T>Z` compares the whole
row (equal rows are then bit-identical, so any sort will do). Leave it unset
when `<T>Z` looks at a key only: the memtable is then sorted stably.

### State

A stack is one `kv64b` of `(pup_key, fd)` entries, with the mapped bytes in
abc's file table. `PUPOpenAll` fills it from a directory, `PUPClose`
releases it. Path and `ext` are passed on every call, so one primitive serves
many stacks.

### Build and test

libpups is part of abc (`PUPS.c` is in `abc-core`; `PUPS.h` and `PUPx.h`
are installed with the other headers). Tests are in `test/PUPS.c`
(`PUPStest`): error-path fd leaks, memtable round trips, collapse, flush,
commit, crash-invisibility, the ladder, ladder crash points, drop, name
clashes, two writers,
foreign runs and the ToC.

