#   SWISS container

`SWISSx.h` is a Swiss table (absl's flat_hash_map layout) over a record
type, chosen explicitly next to `HASHx.h`: same record conventions
(`X(,Hash)`, `X(,KeyEq)` read the key only), same call names and codes
(`Get At Put Probe Unprobe Del DelAt Next Grow Move PutGrow ProbeGrow`),
no CONVERGE.  Records are 16 B and up (a static_assert).  `SWISS.h`
holds the control codes and the 16-byte group scans (SSE2, NEON, else
SWAR; `ABC_SWISS_SWAR` forces SWAR).  SWISS-26B05A2Jxv.

    #define X(M, name) M##kv64##name
    #include "SWISSx.h"

    Bkv64 tab = {};
    call(kv64bMap, tab, 1 << 26);       // the max, idle pages cost nothing
    call(SWISSkv64Init, tab, 16);       // the table: 16 slots, pow2
    call(SWISSkv64PutGrow, tab, &rec);  // every call takes the buffer

##  Layout

One mapped buffer, touched pages only.  PAST: a u64 (the room left)
and a pad, then one control byte per slot for the max slots the range
fits; DATA: the slots (a power of 2, at least 16); IDLE: the room Grow
doubles into.  Control bytes: `0x00` empty (so zeroed memory is an
empty table), `0x01` tomb, `0x80|tag` live, the tag being the low 7
hash bits; the slot is `(hash >> 7) & (len-1)`.  The all-zero record is
a valid record here: the control byte says what is live.

##  Probing

Groups are 16 aligned control bytes, one SSE2 load each, probed in
triangular steps (all groups visited at a pow2 length).  A lookup
prefetches the home slot's line, then scans the home group's tags and
reads a slot only on a tag match; an empty byte ends a miss.  Put takes
the first free (empty or tomb) slot from the home offset on within the
group, so a resident mostly sits in its home slot and a hit waits for
one miss only: control line and slot line load in parallel.

##  API

Codes are the shared ones (01.h), as in HASH.md:

  - `Init(buf, len)`: an empty table of len slots on a mapped buffer;
    `SWISBADARG` for a bad len, `SWISNOROOM` past the max.
  - A lookup or insert on an un-Init-ed (empty DATA) buffer fails
    `sane()`; Next/DelAt see no slots.
  - `Get(&rec, buf)`, `At(&slot, buf, &rec)`: OK or SWISNONE.
  - `Put(buf, &rec)`: OK (overwrite, or `X(,Merge)` with
    `ABC_SWISS_MERGE`) or `SWISFULL`: 7/8 of the slots are live or tombs.
  - `Probe(&slot, buf, &rec)`: OK, the resident; SWISNONE, a zeroed slot to
    fill, or `Unprobe(buf, slot)`; SWISFULL.
  - `Del(buf, &rec)`: OK or SWISNONE.  `DelAt(buf, ndx)`: always SWISNONE, the
    slot is empty now; nothing ever moves, so a scan erases in place:

        for (size_t i = 0; OK == SWISSkv64Next(&i, tab); ++i)
            if (match(tab[1] + i)) SWISSkv64DelAt(tab, i);

  - `Grow(buf)`: doubles in place (rehash), `SWISNOROOM` if IDLE or the
    control array is short.  `PutGrow`, `ProbeGrow`: on SWISFULL, rehash at
    the same length if under 7/16 is live (tombs purged), else Grow.
  - `Move(into, from)`: Put every resident of from into into, SWISFULL
    leaves the prefix in; `SWISBADARG` if the ranges overlap.

##  Deletion and rehash

A deleted slot becomes empty if its group has an empty byte (no probe
path ever passed that group), else a tomb.  Rehash (Grow or same
length) is absl's drop-deletes in place: live bytes turn MOVE, then each
resident goes to its first free slot under the new length, swapping with
a MOVE one found there; a resident whose target is in its own group
stays.  The intermediate states are not valid tables.
