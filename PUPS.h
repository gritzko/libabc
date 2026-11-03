//  libpups: a basic, generic LSM-ish store of small fixed-size records with a
//  fixed bit layout and a caller-supplied comparator.  It sits BELOW key-value
//  stores: sorted immutable runs on disk (mmap'd), a small memtable, and a
//  compaction ladder.  No row schema and no consumer is known here.
//  The row type enters only through two whole-range functions (`pupsync`,
//  `pupmerge`) and the record width.  Callers do not write them: PUPx.h
//  generates both, plus typed entry points, from the type name and `<T>Z`.
//  Depends on libabc only.  PUPS.md tells the whole story.
#ifndef PUPS_PUPS_H
#define PUPS_PUPS_H
#include "KV.h"
#include "PATH.h"
#include "RON.h"

// A "puppy" is one immutable mmap'd file of sorted records, `<dir>/<10-RON64
// pup_key><ext>`; a stack is a pup256b of them (PUPS-26A276Xh2e, PUPS.md).

// Pup keys are 60-bit ron60 values.  Callers that write several stacks in
// lockstep pass a globally monotonic value of their own, so they never clash.
#define PUP_SEQNO_W 10

// PUPS-26A276Xh2e: one stack entry, 32 bytes (64-bit only).  A run: key =
// pup_key, val = st_ino of the mapped file (the rescan's replaced-file check),
// data = the mapping.  The memtable: key = PUP_MEM_KEY, val = its mapped bytes,
// data = [base, end of DATA).  One record, so an entry never loses its mapping.
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
//  CODE-26A23CSStk: PUPOpenAll met a run it cannot read: 0 bytes (a crashed
//  non-durable create), unmappable (fds, perms), or more than PUP_SCAN_MAX.
con ok64 PUPEMPTY = 0x65e64e599762;
con ok64 PUPNOMAP = 0x65e657616299;
con ok64 PUPNOROOM = 0x197995d86d8616;
//  PUPS-26A309qbG9: PUPGONE: a run listed by the scan was gone by the time it
//  was mapped (another writer unlinked it); retry the open.
con ok64 PUPGONE = 0x197994185ce;

// PUPS-26A276Xh2e: the most run files one directory scan collects (a
// map-max-once key buffer; idle pages cost nothing).
#define PUP_SCAN_MAX (1UL << 24)

// Map `dir`'s `<10-RON64><ext>` runs read-only into `pups` (the caller's),
// sorted by key; an absent or empty dir is OK.  A run it cannot map fails the
// open (CODE-26A23CSStk); a 0-byte one is the owner's to judge, never unlinked
// (PUPS-26A24FhuO9); a misnamed file is skipped (DOG-027).  Not idempotent.
ok64 PUPOpenAll(pup256b pups, path8sc dir, u8csc ext);

// PUPS-26A309qbG9: PUPOpenAll bounded by a key ceiling — a run keyed above
// `ceil` is not mapped, as if absent (a newer writer's, not yet committed by
// the caller's own protocol).  PUPOpenAll is this with ceil ~0.  A run that
// vanishes between the listing and its mapping fails with PUPGONE (both).
ok64 PUPOpenTo(pup256b pups, path8sc dir, u8csc ext, u64 ceil);

// PUPS-26A309qbG9: how a caller that publishes runs by its own protocol keys
// and retires them (PUPS.md "Caller-published runs").  `floor`: every run a
// call creates takes a key at or above it, and it is left past the last key
// used.  `dead`: NULL unlinks merged-away runs at once, else their keys go here
typedef struct pupopt {
    u64 floor;
    u64bp dead;
} pupopt;

// PUPS-26A309qbG9: unlink the runs `keys` names, OLDEST first (sorts `keys`
// in place), so a crash part way leaves a youngest suffix: every tombstone
// outlives the rows it shadows.  A file already gone is fine (a retry).
ok64 PUPUnlink(path8s dir, u8cs ext, u64s keys);

// Cross-branch load: flip DATA into PAST (the branch loaded so far becomes the
// read-only context), then open `dir` into the now-empty DATA, which later
// PUPCreate writes target.  One helper for "open, `pups[1]=pups[2]`, open".
// Like PUPOpenAll, no dedup against entries already loaded.
ok64 PUPOpenAside(pup256b pups, path8sc dir, u8csc ext);

// Atomically write `bytes` as a new run (tmp + rename), map it, push it onto
// `pups`.  The key is max(RONNow(), max(DATA)+1): monotonic, so independent
// writers converge on disjoint key ranges; DOG-027: then bumped past any key
// whose NAME is on disk already (RON64 is case-sensitive, APFS/NTFS are not).
ok64 PUPCreate(pup256b pups, path8s dir, u8cs ext, u8cs bytes);
// CODE-26A23C8gS9: PUPCreate is the FAST path: no fsync, so after a power loss
// the renamed run may be 0 bytes (see PUPOpenAll).  A caller that commits
// anything on top of the run passes `durable`: the run is fsync'd before the
// rename and the directory after it, so it is on disk when the call returns.
ok64 PUPCreateAs(pup256b pups, path8s dir, u8cs ext, u8cs bytes, b8 durable);
// PUPS-26A309qbG9: PUPCreateAs with the key at or above `opt->floor`.
ok64 PUPCreateOpt(pup256b pups, path8s dir, u8cs ext, u8cs bytes, b8 durable,
                  pupopt *opt);

// PUPCreate with the caller's own pup_key, for callers that keep several
// stacks' file-ids in lockstep.  PUPFAIL when a live DATA entry already holds
// that key (PUPThinTail first when replacing the tail) or, DOG-027, when a
// file already has that name: the key is the caller's, so no bump.
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

// DOG-027: the ONE reserved dict key, ALL-ONES, so the memtable sorts past
// every ron60 pup_key and sits as the tail entry of an rw stack's dict.
#define PUP_MEM_KEY   (~0ULL)

// Write lock on `dir` (flock on `<dir>/.lock`): ONE per directory, since a
// forward and a reverse stack change together.  Writing entry points take it
// themselves; hold it around several calls to make them one update.  Reentrant
// per process, dropped on death, never read-side; `*slot` is PUPUnlock's token.
ok64 PUPLock(int *slot, path8s dir);
void PUPUnlock(int *slot);

// Live puppies in DATA only (the active leaf under the PAST/DATA partition,
// abc/Bx.h §PastDataS); every loaded run is `PUPCountAll`.  DOG-027: the
// memtable is not a run, and this bound is what keeps the legacy readers
// (Data, AllData, ThinTail) off it.
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

// DOG-035: a run may end in a sparse page index, `[data][toc][len:5]["TC"]`:
// cell i is page i's FIRST RECORD verbatim, searched by the type's own Z.
#define PUP_TOC_LEN_W  5
// The 5 length bytes count the block's BYTES, so the clip needs no row width;
// 7 is ODD, so the residue check rejects a plain run at any width but 1 and 7.
#define PUP_TOC_TAIL_W 7

// DOG-035: records per ToC page for a `row`-byte record — the ONE place the cell
// geometry lives, shared by the seal that builds a block and the search that
// walks it.  0 when `row` is 0 or a record straddles a page.
size_t PUPTocRows(u32 row);

// Byte slice of the i-th puppy's mapping, DATA only (see PUPCount; every
// loaded run is `PUPDataAll`).  `out` is empty when `i` is out of range or
// the mapping has been released.
void PUPData(u8csp out, pup256b pups, u32 i);
// Like PUPData but indexes into PastData — every loaded run.
void PUPDataAll(u8csp out, pup256b pups, u32 i);

// DOG-035: the row-aware twins: with `row`, the record width, the trailer is
// CHECKED per file (magic, length, residue 0 mod row) and the ToC block clipped
// OFF.  The clip is the correctness core: a cell carries a REAL key, so an
// unclipped source feeds the keep-last dedup duplicates.  Bare names: row 0.
void PUPDataRow(u8csp out, pup256b pups, u32 i, u32 row);
void PUPDataAllRow(u8csp out, pup256b pups, u32 i, u32 row);

// DOG-035: the i-th run's ToC block — the ONE way a reader reaches it, empty
// when the run carries none.  Cells are records, so search them with the
// record type's comparator, then search the page they name.
void PUPToc(u8csp out, pup256b pups, u32 i, u32 row);

// Fill `out` (reset first) with one slice per live puppy, oldest to newest as
// PUPOpenAll keyed them, skipping released slots, so it may hold fewer than
// PUPCount.  The LSM merge and lookup loops view the stack as one `u8css`.
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

// PUPS-26A276Xh2e: the memtable is no file but an anonymous BOX (BOXx.h) mapped
// once per handle under PUP_MEM_KEY; a seal streams it into one new run file.
#define PUP_MEM_BYTES 4096
// PUPS-26A275OQK0: c, the memtable segment limit in bytes (PUPx.h: ABC_BOX_SEG)
#define PUP_COLLAPSE  512

// DOG-032: the memtable's size is an OPEN-TIME parameter counted in WHOLE OS
// pages (`FILESysPage`, never a hardcoded 4096); 0 means one page, the default.
#define PUP_MEM_PAGES 1

// PUPS-26A276Xh2e: `runs`, the memtable's run slices, is the CALLER's typed
// X(,csb), held as long as the pups and passed to every memtable entry point
// (PUPS.md "State"); the core passes it through.  DOG-027: the type's hook;
// BOX-26A26EbjLj: it seals DATA into a run, then ladders the tail (BOX Sort).
typedef ok64 (*pupsync)(u8bp mem, u8csb runs);

// DOG-027: the type's compaction merge, HIT<T>Merge over the oldest-first
// slices `srcs`; `into`'s head advances past the merged bytes (ABC-015 drain).
// PUPS-001: `bottom` is YES when `srcs` is the WHOLE stack, so a row that
// marks its key dead may be dropped; a partial merge passes NO and keeps it.
typedef ok64 (*pupmerge)(u8s into, u8css srcs, b8 bottom);

// The entry points below take what the record type contributes: `sync`,
// `merge` and `row`, its width in BYTES and the ToC opt-in (0: raw, unprobed).

// DOG-027: append one record to the memtable's DATA (created lazily); a full
// buffer seals into a run under a fresh pup_key, and the seal ends in the
// ladder.  Typed callers use the PUPx.h wrappers (PUPu64Put etc.), which fill
// `sync`, `merge` and `row` in.  A NULL `sync` fails; no `merge`, no ladder.
ok64 PUPPut(pup256b pups, u8csb runs, path8s dir, u8cs ext, u8csc rec,
            pupsync sync, pupmerge merge, u32 row);

// DOG-032: the same put with a SIZED memtable: `mem_pages` whole OS pages (Pup
// knows no row size), so a bulk run seals once per megabyte, not per 4 KB; 0 is
// the one-page default `PUPPut` passes.  An EMPTY memtable of another size is
// remapped; a non-empty one keeps its size until sealed.
ok64 PUPPutMem(pup256b pups, u8csb runs, path8s dir, u8cs ext, u8csc rec,
               pupsync sync, pupmerge merge, u32 row, u32 mem_pages);

// PUPS-26A309qbG9: PUPPutMem keyed and retired by `opt` (NULL: as PUPPutMem).
ok64 PUPPutOpt(pup256b pups, u8csb runs, path8s dir, u8cs ext, u8csc rec,
               pupsync sync, pupmerge merge, u32 row, u32 mem_pages,
               pupopt *opt);

// DOG-027: seal the memtable into `<10-RON64><ext>`: its runs merged through
// `merge` into a tmp file, ToC'd, renamed (atomic: a crash leaves a tmp or a
// whole run), then the ladder.  A NULL `merge` seals a one-run memtable only.
// DOG-046: re-scans `dir` first, so the key clears other writers' fresh runs.
ok64 PUPCommit(pup256b pups, u8csb runs, path8s dir, u8cs ext, pupsync sync,
               pupmerge merge, u32 row);

// DOG-032: the same commit with the fsync under the caller's control — a bulk
// run seals with `durable` NO and ends in ONE durable commit.  The rename stays
// atomic either way, so a crash still leaves a prefix, never a half-run.
ok64 PUPCommitAs(pup256b pups, u8csb runs, path8s dir, u8cs ext,
                 pupsync sync, pupmerge merge, u32 row, b8 durable);

// PUPS-26A309qbG9: PUPCommitAs keyed and retired by `opt` (NULL: as PUPCommitAs).
ok64 PUPCommitOpt(pup256b pups, u8csb runs, path8s dir, u8cs ext,
                  pupsync sync, pupmerge merge, u32 row, b8 durable,
                  pupopt *opt);

// DOG-027: the 1/8 size-tiered ladder (ABC_HIT_LADDER_DIV) over the committed
// runs, where every seal ends: the youngest violating tail is merged through
// `merge` into ONE fresh run and the sources unlinked, until HITIsCompact
// holds; no `merge`, no-op.  DOG-046: re-scans `dir` first (see PUPS.md).
ok64 PUPLadder(pup256b pups, path8s dir, u8cs ext, pupmerge merge, u32 row);
// PUPS-26A309qbG9: the ladder keyed and retired by `opt` (NULL: as PUPLadder):
// the merged run takes a key at or above `opt->floor`, and with `opt->dead`
// its sources stay on disk for the caller to PUPUnlink.
ok64 PUPLadderOpt(pup256b pups, path8s dir, u8cs ext, pupmerge merge, u32 row,
                  pupopt *opt);

// DOG-027: query view — committed runs oldest→newest, then the memtable's
// PAST and DATA (sync'd first) as one or two more HIT sources.  Resets `out`.
ok64 PUPAllRuns(u8csb out, pup256b pups, u8csb runs, pupsync sync, u32 row);

// DOG-035: the same view with the runs' ToC blocks alongside — `tocs` gets one
// entry per `out` entry, empty where the run carries none (the memtable's PAST
// and DATA never do).  `PUPAllRuns` is this with `tocs` NULL.
ok64 PUPAllRunsToc(u8csb out, u8csb tocs, pup256b pups, u8csb runs,
                   pupsync sync, u32 row);


#endif
