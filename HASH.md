#   HASH container

`HASHx.h` is a very basic open-addressed hash table template, linear
probing with backward-shift deletion.  The table is a slice of records,
its length a power of 2; use it standalone or as a part of some bigger
construct.  The record type supplies the key view of
the key only: for kv-like records the key field, for plain scalars the
whole value (see `INT.h`, `KV.h`).  `Xmv`, `XSwap`, `XIsZero` come from
the usual Bx.h instantiation.  The element macro inside is `HASH_T`.

    #define X(M, name) M##kv64##name
    #define ABC_HASH_LINE 16
    #include "HASHx.h"

##  The zero record

The all-zero record is the empty slot: never stored, never found.  A
key 0 needs a nonzero payload (kv `{0,7}` works, u64 key 0 does not).

##  API

Return codes are the shared ones (01.h): `OK`, `NONE` (absent), `FULL`
(the record's line is full), `NOROOM` (Grow: IDLE out), `BADARG` (Grow,
Move: a bad length, overlapping tables).  On a `$` slice (the table):

  - `Get(&rec, data)`: rec carries the key in, the stored record out.
  - `At(&slot, data, &rec)`: read-only lookup, a const pointer to the
    resident; no copy, no mutation.
  - `Put(data, &rec)`: insert or overwrite (with `ABC_HASH_MERGE`,
    fold into the resident by `XMerge(resident, &rec)`); FULL leaves the
    table untouched, also under CONVERGE (nothing is evicted).
  - `Probe(&slot, data, &rec)`: get-or-insert in one probe, rec hashed
    once, never copied.  OK: `*slot` is the resident.  NONE: `*slot` is
    the zeroed slot Put would use; fill it (a nonzero record with rec's
    key) or `Unprobe(data, slot)` it before any other op on the table.
    Under CONVERGE an unfilled slot hides displaced residents.  Put is
    Probe + mv, so either way the layout is the same.
  - `Del(data, &rec)`: OK or NONE if absent.  `DelAt(data, ndx)`: delete
    the entry at a slot, see below.
  - `Next(&ndx, data)`: OK with ndx at the first live slot at or after
    ndx, NONE at the end; the way to iterate, never a raw slot scan.

On a `B` buffer (a growable table, see below): `PutGrow`, `ProbeGrow`
(Grow while the line is full), `Grow` (`NOROOM` if out of room) and
`Move(into, from)`.  A table shorter than its line is not guarded,
except Grow/Move fail `BADARG` on it.

##  Knobs

Macro parameters, per instantiation (undefined at the end of HASHx.h):

  - `ABC_HASH_LINE` the length of a line, default 64.  Cells can only
    be displaced within their line; if a line is full, `Put` returns
    `FULL`.  The line length (a static_assert) and the table length MUST
    be powers of 2 (the slot is `hash & (len-1)`; anything else corrupts
    the table and may loop the probe).  The retired `lineX` is an error.
  - `ABC_HASH_CONVERGE` whether we want the hash table to be convergent.
    A "bit state" of a convergent table does not depend on the order of
    operations, if only the resulting "API state" is the same.
    Operations that commute can arrive in any order!
    For example, "put 1, put 2, remove 1, put 3" and "put 3, put 2" would
    result in the same bit-state of the *buffer*.  Within a probe path
    residents go in ascending hash order: Put displaces the first one
    hashing above the newcomer, so it hashes the residents it passes.
  - `ABC_HASH_MERGE` (defined: on) Put folds an equal key by `XMerge`.
  - `ABC_HASH_MISSSTOP` (CONVERGE only, a static_assert; default 0) a Get miss stops at
    the first resident hashing above the key.  A hash per probed
    resident: hits pay 1.5..5ns, misses at LINE 64, load >= 0.5 win
    10..20%; worth it for miss-heavy LINE 64 tables, no gain at 16.

##  Load

A line is full long before the table is: the load at the first
`FULL`, random u64 keys, and the load a `PutGrow` table keeps
while growing 16K..4M entries (avg, min..max):

    LINE  2^16  2^20  2^24   PutGrow avg (min..max)
      16  0.35  0.26  0.19   0.22 (0.12..0.39)
      32  0.39  0.45  0.40   0.34 (0.21..0.55)
      64  0.65  0.53  0.52   0.44 (0.30..0.65)

kv128 is the same within 0.05.  At an equal load (0.25) Get and Put
cost the same at any LINE; at its own max load LINE 64 misses scan
longer (2^24: 39ns vs 12ns at 16).  A Swiss table keeps 0.44..0.87.

##  Delete while iterating

`Del` refills the hole by a backward shift: a later entry of the line
may move into the slot just deleted.  To erase while scanning slots, use
`DelAt(data, ndx)`: OK means ndx got refilled, re-examine it; NONE
means ndx is empty now, step on.  An unvisited entry never moves before
ndx; a visited one may move forward and be seen again.

    for (size_t i = 0; OK == HASHkv64Next(&i, data);)
        if (!match(data[0] + i) || OK != HASHkv64DelAt(data, i)) ++i;

##  Growth in place

A growable table lives in a buffer: the table is the buffer's DATA (a
power-of-2 length, at least `ABC_HASH_LINE`), the IDLE behind it is the
room it grows into.  Map the max range once, set the initial length:

    Bkv64 tab = {};
    call(kv64bMap, tab, 1 << 26);       // the max, idle pages cost nothing
    call(kv64bFed, tab, 1024);          // the table: kv64bData(tab)
    call(HASHkv64PutGrow, tab, &rec);   // Get/Put/Del/Probe take kv64bData(tab)

`Grow(buf)` doubles DATA into the first half of IDLE; it never maps or
copies the table and fails `NOROOM` if IDLE is shorter than DATA (the
table is untouched then).  One doubling never overflows a line.  Phases,
`data` being the doubled view `{DATA head, DATA end + len}`:

 1. `_growCopy(data)`: for each lower line b, zero line b+len, then put
    there every entry of line b whose home under 2·len is in line b+len,
    each entry hashed once.  The upper half is invisible under the old
    length.
 2. Flip: `bFed(buf, len)`, a single word store, doubles DATA.
 3. `_growClean(data)`: delete the stale copies left in the lower half
    by backward shifts; a line is swept again only if a shift lapped it
    (and could carry an unchecked entry back).

Every state between steps is a valid table: stale duplicates are
invisible (their key's home is in the upper line, which holds the live
copy), there are no holes.  Both 1 and 3 are idempotent.  Under
`ABC_HASH_CONVERGE` the grown table is byte-equal to one Put-built at the
new length (tested).  Reset a table by zeroing DATA and `bShed`-ing it
back; the room may stay dirty, `_growCopy` zeroes what it uses.

##  Move into another table

`Move(into, from)` rehashes every entry of `from`'s DATA into `into`'s
DATA, a table the caller set up at any power-of-2 length (at least
`ABC_HASH_LINE`): bigger, the same, smaller.  It never maps; the caller
allocates, then unmaps `from` (left intact) if it likes.  `into`'s
residents stay, a key in both gets `from`'s record.  The tables must
not overlap (`BADARG`).  Use: the fallback when `Grow` fails
`NOROOM` (a new map, then Move), shrinking, another line size.

    Bkv64 big = {};
    call(kv64bMap, big, 1 << 28);
    call(kv64bFed, big, kv64bDataLen(tab) * 4);
    call(HASHkv64Move, big, tab);
    call(kv64bUnMap, tab);

`FULL`: some entry's line in `into` is full.  `into` is left
partially filled: the entries of `from` before that one (in slot order)
are in, none lost (a FULL Put evicts nothing); `from` is intact.
Under `ABC_HASH_CONVERGE` a Move into an empty table is byte-equal to
one Put-built at its length (tested).

Not done, kept possible: crash safety for a file-backed map with
word-sized records.  Missing: the length persisted in a header word,
the flip being that store; msync after phase 1 (before the flip) and
after the flip; on open, `_growClean` at the persisted length (a no-op on
a clean table: no lower-half entry has its home in the upper half),
which finishes a phase 3 cut short; a phase 1 cut short needs nothing,
it is invisible and the next Grow redoes it.  Records must be stored by
single words (a torn multi-word record is a corrupt entry).

