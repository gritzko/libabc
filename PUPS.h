//  libpups: a basic, generic LSM-ish store of small fixed-size records with a
//  fixed bit layout and a caller-supplied comparator.  It sits BELOW key-value
//  stores: sorted immutable runs on disk (mmap'd), a small memtable, and a
//  compaction ladder, all held in one house (PUPS-26B045Ffpb, BOX.h).  This
//  core is untyped: keys, paths, maps, tmp files, the ToC; one u8cs at a time.
//  The typed half (Open, Commit, Ladder, Get...) is PUPx.h.  PUPS.md tells
//  the whole story.  Depends on libabc only.
#ifndef PUPS_PUPS_H
#define PUPS_PUPS_H
#include "BOX.h"
#include "KV.h"
#include "PATH.h"
#include "RON.h"

// A "puppy" is one immutable mmap'd file of sorted records, `<dir>/<10-RON64
// pup_key><ext>`.  Pup keys are 60-bit ron60 values.
#define PUP_SEQNO_W 10

//  PUPS-26B045Ffpb: a house plus its file keys, index-parallel to its PAST.
#define a_pup(T, h) \
    a_box(T, h);    \
    a_pad(u64, h##_keys, ABC_HIT_MAX_RUNS - 2)

//  PUPS-26B03JQWPP: one code per failure; PUP* calls return these, or the
//  shared BOX codes from the memtable (Put, Commit, Runs, Range, Read).
con ok64 PUPBADARG = 0x197992ca34a6d0;    //  a bad index
con ok64 PUPTAKEN = 0x65e65d294397;       //  a live run holds the caller's key
con ok64 PUPSTAT = 0x1979971d29d;         //  PUPLock: stat(dir) failed
con ok64 PUPNOOPEN = 0x197995d8619397;    //  PUPLock: open(.lock) failed
con ok64 PUPLCKFULL = 0x65e6553143de555;  //  PUPLock: held-lock table full
//  CODE-26A23CSStk: a run it cannot read: 0 bytes (a crashed non-durable
//  create), unmappable (fds, perms); past 62 runs or keys is PUPNOROOM.
con ok64 PUPEMPTY = 0x65e64e599762;
con ok64 PUPNOMAP = 0x65e657616299;
con ok64 PUPNOROOM = 0x197995d86d8616;
//  PUPS-26A309qbG9: PUPGONE: a run listed by the scan was gone by the time it
//  was mapped (another writer unlinked it); retry the open.
con ok64 PUPGONE = 0x197994185ce;

// PUPS-26A309qbG9: how a caller that publishes runs by its own protocol keys
// and retires them (PUPS.md "Caller-published runs").  `floor`: every run a
// call creates takes a key at or above it, and it is left past the last key
// used.  `dead`: NULL unlinks merged-away runs at once, else their keys go here
typedef struct pupopt {
    u64 floor;
    u64bp dead;
} pupopt;

// Feed `dir`'s `<10-RON64><ext>` run keys at or below `ceil`, but not in
// `dead`, into `keys`, sorted; a misnamed file is skipped (DOG-027), an absent
// dir is OK.  PUPNOROOM when `keys` is out of IDLE.
ok64 PUPScan(u64b keys, path8sc dir, u8csc ext, u64 ceil, u64csc dead);

// PUPS-26A276Xh2e: map run `key` whole, read-only, the fd closed at once.
// PUPEMPTY 0 bytes (never unlinked, PUPS-26A24FhuO9), PUPGONE no such file
// (PUPS-26A309qbG9), PUPNOMAP the rest.
ok64 PUPMap(u8csp run, path8s dir, u8cs ext, u64 key);
void PUPUnMap(u8csc run);

// DOG-027: max(RONNow, the last key + 1, `floor`), then past any NAME on disk
// (RON64 is case-sensitive, APFS/NTFS are not).  `keys` sorted, held runs.
ok64 PUPNextKey(u64 *out, u64csc keys, path8s dir, u8cs ext, u64 floor);
// YES when a held run or a file on its name has `key`.
ok64 PUPKeyTaken(b8 *out, u64csc keys, path8s dir, u8cs ext, u64 key);

// PUPS-26A25EfKtr: `<key><ext>.tmp` sized `bound` and mapped rw, so the typed
// merge writes in place (no scratch, no write()); its DATA is the run.
ok64 PUPTmpMap(u8bp *tmp, path8s dir, u8cs ext, u64 key, size_t bound);
// Trim, fsync if `durable`, rename onto `<key><ext>` (atomic), fsync the dir,
// map it read-only into `run`.  An empty tmp seals the 7-byte nil run.
ok64 PUPTmpSeal(u8csp run, u8bp tmp, path8s dir, u8cs ext, u64 key,
                b8 durable);
// Unmap and unlink a tmp that left nothing, or failed.
ok64 PUPTmpDrop(u8bp tmp, path8s dir, u8cs ext, u64 key);

// PUPS-26A309qbG9: unlink the runs `keys` names, OLDEST first (sorts `keys`
// in place), so a crash part way leaves a youngest suffix: every tombstone
// outlives the rows it shadows.  A file already gone is fine (a retry).
ok64 PUPUnlink(path8s dir, u8cs ext, u64s keys);

// Write lock on `dir` (flock on `<dir>/.lock`): ONE per directory, since a
// forward and a reverse stack change together.  Writing entry points take it
// themselves; hold it around several calls to make them one update.  Reentrant
// per process, dropped on death, never read-side; `*slot` is PUPUnlock's token.
ok64 PUPLock(int *slot, path8s dir);
void PUPUnlock(int *slot);

// DOG-035: a run may end in a sparse page index, `[data][toc][len:5]["TC"]`:
// cell i is page i's FIRST RECORD verbatim, searched by the type's own Z.
#define PUP_TOC_LEN_W  5
// The 5 length bytes count the block's BYTES, so the clip needs no row width;
// 7 is ODD, so the residue check rejects a plain run at any width but 1 and 7.
#define PUP_TOC_TAIL_W 7

// DOG-035: records per ToC page for a `row`-byte record; 0 when `row` is 0 or
// a record straddles a page.
size_t PUPTocRows(u32 row);
// DOG-035: feed `data`'s ToC block and trailer into `into`; nothing when the
// run takes none (under two pages), so a short run stays plain.
ok64 PUPTocFeed(u8bp into, u8csc data, u32 row);
// DOG-035: cut a mapped run into [data][toc]; presence is CHECKED (magic,
// length, geometry), never inferred.  The nil run is empty data.
void PUPTocSplit(u8csp data, u8csp toc, u8csc run, u32 row);

#endif
