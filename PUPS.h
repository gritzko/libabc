#ifndef PUPS_PUPS_H
#define PUPS_PUPS_H
//  libpups: a basic, generic LSM-ish store of small fixed-size records with a
//  fixed bit layout and a caller-supplied comparator.  It sits BELOW key-value
//  stores: sorted immutable runs on disk (mmap'd), a small memtable, and a
//  compaction ladder.  No row schema and no consumer is known here.
//  The row type enters only through two whole-range functions (`pupsync`,
//  `pupmerge`) and the record width.  Callers do not write them: PUPx.h
//  generates both, plus typed entry points, from the type name and `<T>Z`.
//  Depends on libabc only.
#include "KV.h"
#include "PATH.h"
#include "RON.h"

// --- Puppies: stack of `<seqno>.<ext>` files ---
//
// A "puppy" is one immutable, mmap'd file of fixed-size sorted records.
// The record type is the caller's: the library moves bytes and file names
// and never looks inside a row, except through the `pupsync` and
// `pupmerge` functions it is handed for sorting, dedup and merging.  A stack of puppies lives in
// one directory, each named `<seqno><ext>`, where `<seqno>` is a 10-char
// zero-padded RON64 integer and `<ext>` is chosen by the caller (it also
// names the on-disk format, so one directory can hold several stacks).
//
// PUPS-26A276Xh2e: state is one `pup256b`, a buffer of fixed-layout records
// (pup256, below), one per run: the pup_key plus the run's read-only mapping
// itself.  A run holds NO fd: it is closed right after mmap (a mapping
// outlives close), so the number of open runs is bounded by address space,
// not by fds or FILE_MAX_OPEN.  File names are derived, never stored:
// `<dir>/<10-RON64 pup_key><ext>`, dir and ext caller-owned and re-passed
// per call, so the same primitive serves multiple stacks per directory, told
// apart by `ext`.  The typed merge during compaction stays caller-side
// (HIT*Compact); this API only does fs-level housekeeping.
//
// Pup keys are 60-bit ron60-style values (10-char RON64 filename).
// Callers that write several stacks in lockstep pass a globally monotonic
// value of their own so two writers / collapsed sub-shards can't collide.
#define PUP_SEQNO_W 10

// PUPS-26A276Xh2e: one stack entry — 32 bytes, fixed layout (64-bit only):
//   run       key = pup_key, val = st_ino of the mapped file (the rescan's
//             "another writer replaced it" check), data = the mapped bytes;
//   memtable  key = PUP_MEM_KEY, val = its mapped bytes, data = [base, end
//             of DATA) of the anonymous memtable buffer (PAST ends where the
//             caller's last memtable run ends).
// One record instead of kv64 + a parallel slice buffer: one buffer to size,
// push, drop and sort, and an entry can never lose its mapping.
typedef struct pup256 {
    u64 key;
    u64 val;
    u8cs data;
} pup256;

fun b8 pup256Z(pup256 const *a, pup256 const *b) { return a->key < b->key; }

#define X(M, name) M##pup256##name
#include "Bx.h"
#include "QSORTx.h"
#undef X

con ok64 PUPFAIL = 0x197993ca495;
//  CODE-26A23CSStk: PUPOpenAll found a run it cannot read.  PUPEMPTY: the run
//  is 0 bytes, what a crash leaves of a non-durable create.  PUPNOMAP: it could
//  not be opened or mapped (fds, perms).  PUPNOROOM: more runs than one scan
//  holds (PUP_SCAN_MAX).
con ok64 PUPEMPTY = 0x65e64e599762;
con ok64 PUPNOMAP = 0x65e657616299;
con ok64 PUPNOROOM = 0x197995d86d8616;

// PUPS-26A276Xh2e: the most run files one directory scan collects (a
// map-max-once key buffer; idle pages cost nothing).
#define PUP_SCAN_MAX (1UL << 24)

// Scan `dir` for files matching `<10-RON64><ext>`, sort by pup_key,
// map each read-only (fd closed at once), push (pup_key, mapping) into `pups`.
// Empty or absent dir → OK with pups left empty.  Caller must have allocated `pups` first.
// CODE-26A23CSStk: never OK with a run missing.  A run it cannot map fails the
// open (PUPNOMAP; PUPEMPTY for a 0-byte one), more than PUP_SCAN_MAX runs
// fail with PUPNOROOM; entries already pushed stay for PUPClose.
// PUPS-26A276Xh2e: a mapped run holds no fd and no FILE_MAX_OPEN slot.
// Crash policy: a 0-byte run is NOT unlinked nor skipped — PUPS cannot tell a
// crashed create from a committed run that lost its bytes, so the owner, who
// knows what it committed on top, decides (re-derive, unlink, or refuse).  A
// durable create (PUPCreateAs) never leaves one.
// PUPS-26A24FhuO9: an EMPTY run is legal: PUPCreate* of no bytes writes the
// 7-byte trailer of an empty ToC (`len` 0, "TC"), which maps and reads as no
// rows at every row width, so a 0-byte file still means a crashed create.  A
// ladder merge whose output is empty still books no run (PUPS-001).
// A run truncated to a non-zero length is not detected here (no row width); row-aware readers
// check the residue.
// Not idempotent: every file in `dir` becomes a new entry, even if a
// matching pup_key already sits in PAST/DATA — same basename in a
// different dir is a different file.  Avoiding double-scans is the
// caller's responsibility (e.g. skip a prefix of the stack already loaded
// from a shared parent directory).
// DOG-027: a file that ends in `<ext>` but carries no 10-char seqno is NOT a
// run — jab's retired JS writer padded to 8 — so it is SKIPPED, never
// unlinked; an open leaves whatever it cannot read on disk.
ok64 PUPOpenAll(pup256b pups, path8sc dir, u8csc ext);

// Cross-branch load: flip the current DATA into PAST (collapse the
// previously-loaded branch into the read-only context), then open
// `dir`'s pup files into the now-empty DATA.  Mirror of "open first
// branch, then `pups[1]=pups[2]`, then open the second" — the call
// sequence becomes one helper.  The active leaf becomes the dir just
// loaded; subsequent PUPCreate writes target this
// new DATA.  Like PUPOpenAll, this primitive does not dedup against
// already-loaded entries.
ok64 PUPOpenAside(pup256b pups, path8sc dir, u8csc ext);

// Atomically write `bytes` to `<max(now_ron60, max(DATA)+1)>.<ext>`
// (tmp+rename), map it, push (new_pup_key, mapping) onto `pups`.
// New key picks `RONNow()` (60-bit ron60) when it dominates the local
// max, else `max(DATA)+1` — a monotonic invariant, so independent writers
// across sub-shards converge to non-overlapping key ranges.
// DOG-027: then bumped past any key whose NAME is already on disk — RON64 is
// case-sensitive, APFS/HFS+/NTFS are not, so two keys compose one filename.
ok64 PUPCreate(pup256b pups, path8s dir, u8cs ext, u8cs bytes);
// CODE-26A23C8gS9: PUPCreate is the FAST path: no fsync, so after a power loss
// the renamed run may be 0 bytes (see PUPOpenAll).  A caller that commits
// anything on top of the run passes `durable`: the run is fsync'd before the
// rename and the directory after it, so it is on disk when the call returns.
ok64 PUPCreateAs(pup256b pups, path8s dir, u8cs ext, u8cs bytes, b8 durable);

// Same as PUPCreate but the caller picks the pup_key explicitly.
// Refuses with PUPFAIL when a live entry with the same pup_key
// already sits in DATA (caller should PUPThinTail first when
// replacing the tail).  For callers that keep several stacks' file-ids in
// lockstep.
// DOG-027: refuses the same way when a FILE already holds that key's name —
// the key is the caller's, so the library cannot bump it the way PUPCreate does.
ok64 PUPCreateAt(pup256b pups, path8s dir, u8cs ext, u8cs bytes,
                    u64 pup_key);
// CODE-26A23C8gS9: PUPCreateAt with PUPCreateAs's `durable`; the bare name is
// the non-durable fast path.
ok64 PUPCreateAtAs(pup256b pups, path8s dir, u8cs ext, u8cs bytes,
                   u64 pup_key, b8 durable);

// Unmap and unlink the youngest `m` puppies; trim `pups` by `m`.
ok64 PUPThinTail(pup256b pups, path8s dir, u8cs ext, u32 m);

// DOG-027: unmap + unlink the i-th run (oldest-first), close the dict gap.
// The audit path for a run whose family found no marker; out-of-range fails.
ok64 PUPDropAt(pup256b pups, path8s dir, u8cs ext, u32 i);

// Unmap every puppy and the memtable (unsealed puts are dropped), free `pups`.
ok64 PUPClose(pup256b pups);

// DOG-027: the ONE reserved dict key — ALL-ONES, so it sorts past every ron60
// pup_key and lives as the dict's tail.  NOTHING about the record type is ever dicted:
// the dict holds committed runs and, on an rw stack, the memtable last.
#define PUP_MEM_KEY   (~0ULL)

// Write lock on `dir` (the file `<dir>/.lock`, flock): ONE per directory, since
// a forward and a reverse stack change together.  Every entry point that writes
// (seal, create, ladder) takes it itself; hold it around several calls to keep
// them one update.  Reentrant per process, blocking, dropped by the kernel on
// death; reads never take it.  `*slot` is the token for PUPUnlock.
ok64 PUPLock(int *slot, path8s dir);
void PUPUnlock(int *slot);

// Number of live puppies in the stack — DATA only (the active leaf,
// when callers use the PAST/DATA partition; see abc/Bx.h §PastDataS).  For "every loaded
// run" reads (cross-branch lookups, LSM merges), use `PUPCountAll`.
// DOG-027: the memtable is not a run — this bound is what keeps the
// legacy readers (Data, AllData, ThinTail) off it.
fun u32 PUPCount(pup256b pups) {
    u32 n = (u32)pup256bDataLen(pups);
    pup256 const *base = (pup256 const *)pup256bDataHead(pups);
    while (n > 0 && base[n - 1].key == PUP_MEM_KEY) n--;
    return n;
}
// Number of live puppies across PastData — every loaded run including
// inherited / read-only PAST entries.  Used by readers that
// need the inherited runs too (cross-stack lookups, enumeration).
fun u32 PUPCountAll(pup256b pups) {
    return (u32)pup256bPastDataLen(pups) -
           ((u32)pup256bDataLen(pups) - PUPCount(pups));
}

// DOG-035: a run may carry a sparse page index — `[data][toc][len:5]["TC"]`.
// The 5 little-endian length bytes count the ToC block's BYTES, so the clip
// needs no row width; 7 is ODD, so the residue check below refuses a plain run
// at every row width but 1 and 7.  A ToC cell is that page's FIRST RECORD
// verbatim and cell `i` covers page `i` — no page number, no offset, and both
// search steps share the record type's own comparator.  Derived, droppable, never
// load-bearing: dropping the block leaves a valid run.
#define PUP_TOC_LEN_W  5
#define PUP_TOC_TAIL_W 7

// DOG-035: records per ToC page for a `row`-byte record — the ONE place the cell
// geometry lives, shared by the seal that builds a block and the search that
// walks it.  0 when `row` is 0 or a record straddles a page.
size_t PUPTocRows(u32 row);

// Byte slice of the i-th puppy's contents (the entry's mapping).
// Indexes into DATA only — see `PUPCount` caveats.  For an
// "every loaded run" indexer, use `PUPDataAll`.
// Writes [data_start, data_end) into `out`; out becomes empty when
// `i` is out-of-range or the file's mapping has been released.
void PUPData(u8csp out, pup256b pups, u32 i);
// Like PUPData but indexes into PastData — every loaded run.
void PUPDataAll(u8csp out, pup256b pups, u32 i);

// DOG-035: the row-aware twins — `row` is the record width, so the
// trailer is CHECKED per FILE (magic, the length fits, the data residue is 0
// mod row) and the ToC block clipped OFF.  This clip is the correctness core:
// a cell carries a REAL key, so an unclipped source would feed the keep-last
// dedup duplicate rows.  The bare names above are these at row 0 — no probe,
// no clip — which is what a caller that never opts in gets.
void PUPDataRow(u8csp out, pup256b pups, u32 i, u32 row);
void PUPDataAllRow(u8csp out, pup256b pups, u32 i, u32 row);

// DOG-035: the i-th run's ToC block — the ONE way a reader reaches it, empty
// when the run carries none.  Cells are records, so search them with the record type's
// comparator, then search the page they name.
void PUPToc(u8csp out, pup256b pups, u32 i, u32 row);

// Fill `out` with one [data_start, data_end) slice per live puppy
// (oldest → newest, matching the stack's pup_key order from
// PUPOpenAll).  Resets `out` first.  Skips puppies whose slot has
// been released — the resulting slice count may be less than
// PUPCount.  Used by the LSM merge / lookup loops
// to view the whole stack as one `u8css`.
ok64 PUPAllData(u8csb out, pup256b pups);
// DOG-035: the row-aware twin — every slice clipped past its ToC block.
ok64 PUPAllDataRow(u8csb out, pup256b pups, u32 row);

// Pup key of the i-th puppy.  Returns 0 when out-of-range.
// DOG-027: the one reader that still reaches the memtable entry, on purpose.
fun u64 PUPSeqno(pup256b pups, u32 i) {
    if (i >= pup256bDataLen(pups)) return 0;
    pup256 const *base = (pup256 const *)pup256bDataHead(pups);
    return base[i].key;
}

// PUPS-26A276Xh2e: the memtable is NOT a file: an anonymous buffer mapped once
// per handle (bMap), dicted under the ALL-ONES key.  PAST holds its sorted runs,
// DATA the unsorted segment the puts append to (BOX, BOXx.h).  A seal merges
// its runs straight into a new run file (tmp, ToC, optional fsync, rename):
// that one streamed write IS the save.  Unsealed puts die with the process.
#define PUP_MEM_BYTES 4096
// PUPS-26A275OQK0: c, the memtable segment limit in bytes; PUPx.h sets BOXx_C to it.
#define PUP_COLLAPSE  512

// DOG-032: the memtable's size is an OPEN-TIME parameter counted in WHOLE OS
// pages (`FILESysPage`, never a hardcoded 4096); 0 means one page, the default.
#define PUP_MEM_PAGES 1

// PUPS-26A276Xh2e: the memtable's run slices are the CALLER's — a typed
// X(,csb) (slot array + buffer header, DATA = one slice per run, oldest first)
// held next to its pups, on its stack or in its struct, as long as the pups,
// and passed to every entry point that touches the memtable.  The core sees it
// as a u8csb and passes it through; it resets it whenever the memtable starts
// empty.  HIT_MAX_RUNS slots are plenty: the BOX ladder keeps runs few.
//   kv64cs slots[HIT_MAX_RUNS];
//   kv64csb runs = {slots, slots, slots, slots + HIT_MAX_RUNS};
//
// DOG-027: family-side typed hook over the memtable.
// BOX-26A26EbjLj: the hook seals DATA into a run and collapses the tailing
// runs by the 1/8 ladder (X(BOX,Sort)).
typedef ok64 (*pupsync)(u8bp mem, u8csb runs);

// DOG-027: family-side typed compaction merge — HIT<type>Merge over the
// oldest-first byte slices `srcs`; `into`'s head advances past the merged
// bytes (the ABC-015 drain convention).  Pup is generic, the record type is not.
// PUPS-001: `bottom` is YES when `srcs` is the WHOLE stack — no older run, no
// inherited PAST — so a row that marks its key dead may be dropped, there being
// nothing left for it to shadow.  A partial merge passes NO and keeps it.
typedef ok64 (*pupmerge)(u8s into, u8css srcs, b8 bottom);

// The entry points below take the three things the record type contributes:
// `sync`, `merge`, and `row` — the fixed record width in BYTES, which is also
// the ToC opt-in (0 for none).  Raw byte stacks and unsorted runs pass 0, never
// build a block, and a run of theirs that merely ends in "TC" is never probed.
// Typed callers use the PUPx.h wrappers (e.g. PUPu64Commit), which fill all
// three in.  A NULL `sync` fails; a NULL `merge` turns the ladder off.

// DOG-027: append one record's bytes to the memtable's DATA (created lazily);
// a full buffer seals into a run under a fresh pup_key, and that seal ends in
// the ladder.
ok64 PUPPut(pup256b pups, u8csb runs, path8s dir, u8cs ext, u8csc rec,
            pupsync sync, pupmerge merge, u32 row);

// DOG-032: the same put with a SIZED memtable — `mem_pages` whole OS pages, so
// a bulk run seals once per megabyte, not once per 4 KB.  Pup is generic over
// bytes and knows no row size, so its unit is PAGES; 0 = the one-page default,
// which is what `PUPPut` passes.  An EMPTY memtable of another size is
// remapped to the new one; a non-empty one keeps its size until sealed.
ok64 PUPPutMem(pup256b pups, u8csb runs, path8s dir, u8cs ext, u8csc rec,
               pupsync sync, pupmerge merge, u32 row, u32 mem_pages);

// DOG-027: seal the memtable into `<10-RON64><ext>`: its runs merged through
// `merge` into a tmp file, ToC, rename — atomic: a crash leaves a tmp file or
// a complete run.  Ends, like every seal, with the ladder (below).  A NULL
// `merge` seals only a memtable that holds one run.
// DOG-046: the seal re-scans `dir` first, so the fresh key clears the runs
// other writers sealed meanwhile and the read view afterwards includes them.
ok64 PUPCommit(pup256b pups, u8csb runs, path8s dir, u8cs ext, pupsync sync,
               pupmerge merge, u32 row);

// DOG-032: the same commit with the fsync under the caller's control — a bulk
// run seals with `durable` NO and ends in ONE durable commit.  The rename stays
// atomic either way, so a crash still leaves a prefix, never a half-run.
ok64 PUPCommitAs(pup256b pups, u8csb runs, path8s dir, u8cs ext,
                 pupsync sync, pupmerge merge, u32 row, b8 durable);

// DOG-027: the 1/8 size-tiered ladder (HIT_LADDER_DIV) over the committed
// runs — every seal ends here, so a caller never sees the ladder's state.
// Merges the youngest violating tail through `merge` into ONE
// fresh run (same tmp + rename path, fresh pup_key), unlinks the sources,
// and cascades until HITIsCompact holds.  No `merge` → no-op.
// DOG-046: re-scans `dir` first so it weighs the REAL stack; only the runs it
// merged are unlinked, and a dict entry whose file another writer already
// unlinked keeps its mapping until PUPClose.
ok64 PUPLadder(pup256b pups, path8s dir, u8cs ext, pupmerge merge, u32 row);

// DOG-027: query view — committed runs oldest→newest, then the memtable's
// PAST and DATA (sync'd first) as one or two more HIT sources.  Resets `out`.
ok64 PUPAllRuns(u8csb out, pup256b pups, u8csb runs, pupsync sync, u32 row);

// DOG-035: the same view with the runs' ToC blocks alongside — `tocs` gets one
// entry per `out` entry, empty where the run carries none (the memtable's PAST
// and DATA never do).  `PUPAllRuns` is this with `tocs` NULL.
ok64 PUPAllRunsToc(u8csb out, u8csb tocs, pup256b pups, u8csb runs,
                   pupsync sync, u32 row);


#endif
