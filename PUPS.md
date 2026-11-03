#   libpups: a basic LSM-ish store for small fixed-size records

A very generic, low-level utility library. It stores **small records of a
fixed size and a fixed bit layout**, ordered by a **comparator the caller
supplies**. It sits *below* key-value stores: it knows no keys, values or
schemas, only bytes, a record width and an ordering. It is part of abc
and uses only abc's own pieces (`HIT`, `QSORTx`, `RON`, `FILE`).

### How it works

 -  **Runs.** A stack of immutable files in one directory, each named
    `<seqno><ext>` (`seqno`: 10-char zero-padded RON64). Every file is a
    sorted array of records, mmap'd read-only; the fd is closed right after
    the mmap, so a run holds no fd and the number of open runs is not capped
    by `FILE_MAX_OPEN`. `ext` is the caller's, and it also names the on-disk
    format, so one directory can hold several stacks.
 -  **Memtable.** `PUPPut` appends a record to an anonymous in-memory buffer
    (`PUPPutMem` sizes it in OS pages for bulk loads), a BOX (`BOXx.h`):
    sorted runs plus an unsorted segment that becomes a run past 512 bytes.
    It is no file and holds no fd; unsealed puts die with the handle.
 -  **Commit.** `PUPCommit` merges the memtable's runs (keep-last) straight
    into a temp file, one sequential write, adds the ToC and renames it to
    `<seqno><ext>`. The rename is atomic: a crash leaves a tmp file or a
    complete run, never a half-run. `PUPCommitAs` leaves the fsync to the
    caller.
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

 -  `PUP<T>Sync`: seal the memtable's segment into a run and keep its runs
    in the 1/8 ladder (`BOX<T>Sort`), newest wins.
 -  `PUP<T>Merge`: merge sorted run slices into one; `bottom` says the slices
    are the whole stack (see Deletes).
 -  Typed entry points `PUP<T>Put`, `PutMem`, `Commit`, `CommitAs`, `Ladder`,
    `AllRuns`, `AllRunsToc`, `Read`. Each calls the core with `Sync`, `Merge`
    and `sizeof(T)` filled in, e.g. `PUPu64Commit(pups, runs, dir, ext)`.

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
runs other writers unlinked, so after more than `ABC_HIT_MAX_RUNS` of them a view
overflows (`SNOROOM`); reopen the stack.

### Caller-published runs (PUPS-26A309qbG9)

A caller that makes runs visible by its own protocol, a key ceiling W
("a reader sees a run iff its key <= W"), needs three things:

 -  `PUPOpenTo(pups, dir, ext, ceil)` maps only runs keyed at or below
    `ceil`. A run unlinked between the listing and its mapping fails the open
    with `PUPGONE` (any open), so the caller re-reads W and retries.
 -  `pupopt.floor`: the `*Opt` writes (`PUPCreateOpt`, `PUPPutOpt`,
    `PUPCommitOpt`, `PUPLadderOpt`, typed `PUP<T>PutOpt` etc.) key every run
    they create, ladder output included, at or above it, and leave it past
    the last key used: one `pupopt` across many stacks is one counter.
 -  `pupopt.dead`: a ladder merge unmaps its sources and appends their keys
    instead of unlinking them; later scans through the same `pupopt` skip
    them. `PUPUnlink` retires them oldest first once W covers the merge.

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

### State

A stack is one `pup256b`: a buffer of 32-byte records `{key, val, data}`, one
per run — the pup_key, the file's inode (for the rescan's replaced-file
check) and the mapped bytes themselves — plus, last, the memtable under the
all-ones key (its mapped size, its `[base, end of DATA)`). File names are
derived from the key, `dir` and `ext`, never stored. `PUPOpenAll` fills the
stack from a directory, `PUPClose` releases it. Path and `ext` are passed on
every call, so one primitive serves many stacks.

The memtable's run slices belong to the caller: a typed `X(,csb)` held next
to the stack for as long as the stack, and passed to `Put`, `Commit`,
`AllRuns` and `Read` (NULL is fine where the stack never had a memtable):

    kv64cs slots[ABC_HIT_MAX_RUNS];
    kv64csb runs = {slots, slots, slots, slots + ABC_HIT_MAX_RUNS};
    PUPkv64Put(pups, runs, dir, ext, rec);

The core sees it as a `u8csb` and passes it through; `PUP<T>Sync` casts it
back once.

### Build and test

### Errors (PUPS-26B03JQWPP)

Every `PUP*` call returns `OK` or a `PUP*` code, one meaning each:
`PUPBADARG` a NULL `sync`/`merge`/`runs` or an index past the stack;
`PUPTAKEN` a live run already holds the caller's key; `PUPNOMEM` no memtable;
`PUPNOROOM` a key buffer (`PUP_SCAN_MAX`) or `pupopt.dead` is out of room;
`PUPEMPTY`/`PUPNOMAP`/`PUPGONE` a run of 0 bytes, unmappable, or unlinked
since listed; `PUPSTAT`/`PUPNOOPEN`/`PUPLCKFULL` `PUPLock` could not stat the
directory, open `.lock`, or its held-lock table is full.

libpups is part of abc (`PUPS.c` is in `abc-core`; `PUPS.h` and `PUPx.h`
are installed with the other headers). Tests are in `test/PUPS.c`
(`PUPStest`): error-path fd leaks, memtable round trips, collapse, flush,
commit, crash-invisibility, the ladder, ladder crash points, drop, name
clashes, two writers, foreign runs, more runs than `FILE_MAX_OPEN`, a
memtable that holds no fd, and the ToC; `test/PUPKEY.c` the key floor,
the returned merge sources and the ceiling-bounded open.

