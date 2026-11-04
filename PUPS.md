#   libpups: a basic LSM-ish store for small fixed-size records

A very generic, low-level utility library. It stores **small records of a
fixed size and a fixed bit layout**, ordered by a **comparator the caller
supplies**. It sits *below* key-value stores: it knows no keys, values or
schemas, only bytes, a record width and an ordering. It is part of abc
and uses only abc's own pieces (`BOX`, `HIT`, `QSORTx`, `RON`, `FILE`).

### How it works

 -  **Runs.** A stack of immutable files in one directory, each named
    `<seqno><ext>` (`seqno`: 10-char zero-padded RON64). Every file is a
    sorted array of records, mmap'd read-only; the fd is closed right after
    the mmap, so a run holds no fd. `ext` is the caller's, and it also names
    the on-disk format, so one directory can hold several stacks.
 -  **Memtable.** `PUP<T>Put` is a BOX put (`BOXx.h`) into an anonymous
    buffer the owner maps: sorted runs plus an unsorted segment that becomes
    a run past 512 bytes. It is no file and holds no fd; unsealed puts die
    with the handle. A full memtable says `NOROOM`: the owner commits.
 -  **Commit.** `PUP<T>Commit` sorts the memtable into one run, writes it
    straight into a temp file, adds the ToC and renames it to
    `<seqno><ext>`. The rename is atomic: a crash leaves a tmp file or a
    complete run, never a half-run. `durable` NO leaves out the fsyncs.
 -  **Ladder.** Every commit ends in the ladder, which keeps run sizes in a
    1/8 ladder by merging runs that fall out of proportion, so the run count
    stays small. A merged run never reuses a name: it takes the next free key
    past its youngest source (fsync'd tmp, atomic rename), then the sources
    are unlinked oldest first, so a crash at any step reads the same.
 -  **Reads.** `Get` (a point lookup, youngest first, ToC-guided), `Range`
    (every run trimmed to a key range, for a HIT drain), `Read` (the whole
    merged view) and `Runs` (the runs oldest first, the caller merges). A
    commit re-scans the directory, so concurrent writers on one directory
    see each other's runs and never collide on a name.
 -  **ToC.** A sparse page index at the end of a run
    (`[data][toc][len:5]["TC"]`): one cell per OS page, a copy of that page's
    first record. A lookup searches the cells with the comparator, then only
    the page the cell names. Built when a record does not straddle a page
    and the run spans at least two pages. Derived data: dropping it leaves a
    valid run.

### State: the house (PUPS-26B045Ffpb)

All state is one `X(,csb)` of 64 cells, 1 KB on the owner's stack, plus the
file keys, a `u64b` index-parallel to the house's PAST:

    a_pup(kv64, h);                          // h and h_keys
    kv64bMap(BOXkv64Mem(h), rows);           // D-2: the owner maps the memtable
    PUPkv64Open(h, h_keys, dir, ext, u64max);
    ...
    PUPkv64Close(h, h_keys);
    kv64bUnMap(BOXkv64Mem(h));

 -  PAST: the mapped file runs, oldest first. A cell is the whole mapping
    (its exact munmap length, ToC and trailer included, so its end is not
    T-aligned); `PUP<T>Split` cuts it into rows and ToC.
 -  DATA: BOX's sorted memory runs; the last two cells are the memtable
    (`BOX.md`). At most 62 runs live; the ladder compacts well before that.
 -  Keys go only to the calls that create or unlink files: Open, Close,
    Commit, Ladder, Create, Drop. A struct owner keeps the cells and the
    keys in the struct and inits the header with `PUP_HOUSE(cells)`.

File names are derived from the key, `dir` and `ext`, never stored.

### The record type

The core (`PUPS.c`) is compiled once and sees only bytes and keys: the
directory scan, key to path, the write lock, map/unmap, the tmp file and its
seal (fsync, rename, dir fsync), unlink, the ToC by row size. It takes and
returns single `u8cs` slices, never a slice list. Everything typed comes from
a template, `PUPx.h`, instantiated per record type `T` the way `HITx.h` is:

    #define X(M, name) M##u64##name
    #include "abc/PUPx.h"
    #undef X

`T` needs `Z`, `KeyEq`, `Max` and its Bx, QSORTx instantiated first; PUPx.h
brings its own BOXx and HITx. `ABC_X_MERGE` plus `X(,Merge)` (HIT.h) folds
equal keys oldest first everywhere: the memtable, the ladder, `Read`, and
`Get`, which then folds every version in the house instead of stopping at
the youngest.

### API (`PUP<T>…`, all over the house `h`)

 -  `Open(h, keys, dir, ext, ceil)`: map `dir`'s runs keyed at or below
    `ceil` (u64max: all) into PAST. A run it cannot map fails the open and
    stays on disk; a misnamed file is skipped. `Close(h, keys)`: unmap them.
 -  `Put(h, &rec)`: BOX Put. `Commit(h, keys, dir, ext, durable, opt)`:
    seal the memtable as a run, then the ladder; an empty memtable is a no-op.
 -  `Ladder(h, keys, dir, ext, opt)`: the ladder alone, after a rescan.
 -  `Create(h, keys, dir, ext, rows, key, durable, opt)`: `rows` as a run
    under `key` (0: the next free one); `PUPTAKEN` when the key is taken.
 -  `Drop(h, keys, dir, ext, i)`: unmap and unlink PAST run `i`.
 -  `Get(&out, h, &key)`, `Range(into, h, &lo, &hi)`, `Read(into, h)`,
    `Runs(out, h)`, `Split(rows, toc, cell)`: the reads above.
 -  `Merge(into, runs, bottom)`: the typed HIT merge the ladder uses, into
    the IDLE of a typed buffer; `bottom` drops dead rows.

### Write lock

Writers on one directory take an advisory `flock` on `<dir>/.lock` around what a
rescan cannot make safe: a commit (key pick, then rename onto it), a create, and
a whole ladder. One lock per DIRECTORY, not per stack: a forward and a reverse
stack are one update, so `PUPLock`/`PUPUnlock` let a caller hold the lock across
several stacks. The entry points take it themselves; it is reentrant within a
process (not thread-safe), the kernel drops it when the process dies, and reads
never take it. `.lock` is not a run: a scan skips it.

Known limit (DOG-046): a long-lived handle keeps the mappings of runs other
writers unlinked, so past 62 of them the house is full (`PUPNOROOM`); reopen.

### Caller-published runs (PUPS-26A309qbG9)

A caller that makes runs visible by its own protocol, a key ceiling W
("a reader sees a run iff its key <= W"), needs three things:

 -  `Open(h, keys, dir, ext, W)` maps only runs keyed at or below W. A run
    unlinked between the listing and its mapping fails the open with
    `PUPGONE`, so the caller re-reads W and retries.
 -  `pupopt.floor`: Commit, Ladder and Create key every run they create at
    or above it and leave it past the last key used: one `pupopt` across
    many stacks is one counter.
 -  `pupopt.dead`: a ladder merge unmaps its sources and appends their keys
    instead of unlinking them; later rescans through the same `pupopt` skip
    them. `PUPUnlink` retires them oldest first once W covers the merge.

### Deletes and errors

The core has no delete; a row says "this key is gone" through a tombstone the
record type defines. Define `PUPx_DEAD(row)` (a macro over `T const *`) before
including `PUPx.h`. Then:

 -  `Get` says `NONE` and `Read` skips a dead row; a `Range` drain drops it.
 -  A ladder merge that takes every run (`bottom`) drops dead rows and, by
    keep-last, the rows they shadowed. A partial merge keeps them: an older
    run may still hold the key. A merge that leaves nothing writes no run.
 -  A commit never drops them: its tombstones still shadow older files.

`PUPx_ERR`, an `ok64` expression, is read after each merge and before each
seal; non-OK drops the tmp and fails the call (a comparator that can fail,
as neoclear's, reports through it).

Every `PUP*` call returns `OK`, a BOX code from the memtable, or a `PUP*` code
(PUPS-26B03JQWPP): `PUPBADARG` an index past the stack; `PUPTAKEN` a run already
holds the caller's key; `PUPNOROOM` the house, its keys or `pupopt.dead` is out
of room; `PUPEMPTY`/`PUPNOMAP`/`PUPGONE` a run of 0 bytes, unmappable, or
unlinked since listed; `PUPSTAT`/`PUPNOOPEN`/`PUPLCKFULL` `PUPLock` could not
stat the directory, open `.lock`, or its held-lock table is full.

### Build and test

libpups is part of abc (`PUPS.c` is in `abc-core`; `PUPS.h` and `PUPx.h`
are installed with the other headers). Tests are in `test/PUPS.c`
(`PUPStest`): error-path fd leaks, memtable round trips, collapse, commit,
crash-invisibility, the ladder and its crash points, drop, name clashes,
two writers, foreign runs, a full house, a memtable that holds no fd, the
ToC, Get and Range against the merged view; `test/PUPKEY.c` the key floor,
the returned merge sources and the ceiling-bounded open; `test/PUPNIL.c`
the empty run; `test/PUPDUR.c` fsyncs and loud opens.
