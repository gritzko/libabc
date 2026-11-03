#include "PUPS.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "B.h"
#include "FILE.h"
#include "PRO.h"

// DOG-027: the ladder is length arithmetic with no comparator, so Pup calls
// the plain policy in HIT.h — no element-typed HIT is instantiated here.
#include "HIT.h"

// =============================================================
// --- Puppies: stack of <seqno>.<ext> files ---
// =============================================================

//  Compose `<dir>/<10-RON64-pup_key><ext>` into `out` (reset first).
static ok64 pup_path(path8b out, path8s dir, u64 pup_key, u8cs ext) {
    sane(u8bOK(out));
    call(PATHu8bDup, out, dir);
    a_pad(u8, name, PUP_SEQNO_W + 32);
    //  JAB-008: RONu8sFeedPad advances the idle cursor itself (ABC-008
    //  contract); the old manual advance here double-counted → PATHBAD.
    call(RONu8sFeedPad, u8bIdle(name), (ok64)pup_key, PUP_SEQNO_W);
    call(u8bFeed, name, ext);
    call(PATHu8bPush, out, u8bDataC(name));
    done;
}

//  Write lock: ONE advisory lock per DIRECTORY, the file `<dir>/.lock` (not a
//  run: a scan skips it).  A directory is one store's unit of update: a
//  forward stack and its reverse stack change together, so a lock per stack
//  could not keep the pair consistent.  flock(2): the kernel drops it when the
//  process dies, so a crash leaves no stale lock; reads never take it.  It
//  guards what a rescan cannot: picking a key, then renaming onto it, and a
//  ladder merge (two writers collapsing one tail).
//  Reentrant within a process (a depth count per directory, by dev/ino), so
//  a caller may hold it across several stacks (PUPLock) while the entry
//  points take it again.  Not thread-safe, and a forked child must not inherit a held lock.
//  Held locks: a u64 buffer of {dev, ino, fd, depth} quads keyed by the
//  directory's identity, so aliased paths share one entry; a slot is a quad
//  index.  Mapped once at FILE_MAX_OPEN quads (one fd each), never moves.
#define PUP_LQ 4
static Bu64 pup_held = {};

ok64 PUPLock(int *slot, path8s dir) {
    sane(slot != NULL && $ok(dir));
    *slot = -1;
    if (pup_held[0] == NULL) call(u64bMap, pup_held, FILE_MAX_OPEN * PUP_LQ);
    a_path(lp);
    call(PATHu8bDup, lp, dir);
    struct stat st;
    if (stat((char const *)u8bDataHead(lp), &st) != 0) return PUPFAIL;
    u64 *h = u64bDataHead(pup_held);
    size_t n = u64bDataLen(pup_held) / PUP_LQ;
    int free_slot = -1;
    for (size_t i = 0; i < n; i++) {
        u64 *q = h + i * PUP_LQ;
        if (q[3] == 0) { if (free_slot < 0) free_slot = (int)i; continue; }
        if (q[0] != (u64)st.st_dev || q[1] != (u64)st.st_ino) continue;
        q[3]++;
        *slot = (int)i;
        done;
    }
    if (free_slot < 0 && u64bIdleLen(pup_held) < PUP_LQ) return PUPFAIL;
    a_cstr(name, ".lock");
    call(PATHu8bPush, lp, name);
    int f = open((char const *)u8bDataHead(lp), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (f < 0) return PUPFAIL;
    try(FILELock, &f, YES);
    if (__ != OK) { close(f); fail(__); }
    if (free_slot < 0) {
        free_slot = (int)n;
        u64bFed(pup_held, PUP_LQ);
    }
    u64 *q = u64bDataHead(pup_held) + free_slot * PUP_LQ;
    q[0] = (u64)st.st_dev, q[1] = (u64)st.st_ino, q[2] = (u64)f, q[3] = 1;
    *slot = free_slot;
    done;
}

void PUPUnlock(int *slot) {
    if (*slot < 0) return;
    u64 *q = u64bDataHead(pup_held) + *slot * PUP_LQ;
    if (--q[3] == 0) close((int)q[2]);  //  drops the flock
    *slot = -1;
}

//  Parse "<10-RON64><ext>" filename → pup_key; 0 on bad fmt.
static u64 pup_parse_seqno(u8csc name, u8csc ext) {
    if (u8csLen(name) != PUP_SEQNO_W + u8csLen(ext)) return 0;
    a_dup(u8c, tail, name);
    u8csUsed(tail, PUP_SEQNO_W);
    if (!u8csEq(tail, ext)) return 0;
    u8cs seqno_s = {name[0], name[0] + PUP_SEQNO_W};
    ok64 v = 0;
    if (RONutf8sDrain(&v, seqno_s) != OK) return 0;
    return (u64)v;
}

// PUPS-26A276Xh2e: map the run on `path` read-only into `ent` and close the
// fd at once — a mapping outlives close, so a run holds no fd.  PUPEMPTY for a
// 0-byte file (mmap refuses it), PUPNOMAP for anything else.
// PUPS-26A309qbG9: PUPGONE for a file no longer there (unlinked since listed).
static ok64 pup_map(pup256 *ent, path8s path, u64 key) {
    sane(ent != NULL && $ok(path));
    int fd = FILE_CLOSED;
    ok64 op = FILEOpen(&fd, path, O_RDONLY);
    if (op != OK) return op == FILENONE ? PUPGONE : PUPNOMAP;
    struct stat st;
    ok64 o = fstat(fd, &st) == 0 ? OK : PUPNOMAP;
    if (o == OK && st.st_size == 0) o = PUPEMPTY;
    void *map = MAP_FAILED;
    if (o == OK) map = mmap(NULL, (size_t)st.st_size, PROT_READ,
                            MAP_FILE | MAP_SHARED, fd, 0);
    FILEClose(&fd);
    if (o != OK) return o;
    if (map == MAP_FAILED) return PUPNOMAP;
    ent->key = key;
    ent->val = (u64)st.st_ino;
    ent->data[0] = (u8c *)map;
    ent->data[1] = (u8c *)map + st.st_size;
    done;
}

static void pup_unmap(pup256 *ent) {
    if (ent->data[0] == NULL) return;
    //  the memtable maps `val` bytes; a run exactly its data
    size_t len = ent->key == PUP_MEM_KEY ? (size_t)ent->val
                                         : (size_t)u8csLen(ent->data);
    munmap((void *)ent->data[0], len);
    ent->data[0] = ent->data[1] = NULL;
}

//  PUPS-001: the dict entry for `key` maps an inode; when the file now on its
//  name is another inode (a merge from another writer replaced it), map the
//  new one in place.  A file that is gone keeps its old mapping, as before.
static void pup_remap(pup256b pups, path8s fpath, u64 key) {
    if (key == PUP_MEM_KEY) return;
    $for(pup256, e, pup256bData(pups)) {
        if (e->key != key) continue;
        struct stat cur;
        if (stat((char const *)fpath[0], &cur) != 0) return;
        if ((u64)cur.st_ino == e->val) return;
        pup256 fresh = {};
        if (pup_map(&fresh, fpath, key) != OK) return;
        pup_unmap(e);
        *e = fresh;
        return;
    }
}

// DOG-027: the ONE monotonic pup_key scheme — max(RONNow, max(DATA)+1),
// skipping the reserved ALL-ONES memtable entry.
// PUPS-26A309qbG9: and at least the caller's `floor`.
static u64 pup_next_key(pup256b pups, u64 floor) {
    u64 key = (u64)RONNow();
    if (key < floor) key = floor;
    $for(pup256, p, pup256bData(pups))
        if (p->key != PUP_MEM_KEY && p->key >= key) key = p->key + 1;
    if (key == 0) key = 1;
    return key;
}

// DOG-027: is `key` spoken for — by a live DATA entry, or by a file already
// sitting on the name it composes?
static ok64 pup_key_taken(b8 *out, pup256b pups, path8s dir, u8cs ext,
                              u64 key) {
    sane(out != NULL && pups != NULL && $ok(dir) && $ok(ext));
    *out = YES;
    if (key == PUP_MEM_KEY) done;       //  reserved for the memtable
    $for(pup256, p, pup256bData(pups))
        if (p->key == key) done;
    a_path(probe);
    call(pup_path, probe, dir, key, ext);
    ok64 e = FILEExists($path(probe));
    if (e == OK) done;                      //  a file is already on the name
    if (e != FILENONE) return e;            //  perms/IO — not ours to paper over
    *out = NO;
    done;
}

// DOG-027: the next key whose NAME is free — RON64 is case-sensitive, APFS is
// not, so two keys can compose one filename and a seal would eat a live run.
static ok64 pup_free_key(u64 *out, pup256b pups, path8s dir, u8cs ext,
                         u64 floor) {
    sane(out != NULL && pups != NULL && $ok(dir) && $ok(ext));
    u64 key = pup_next_key(pups, floor);
    for (b8 taken = YES; ; key++) {
        call(pup_key_taken, &taken, pups, dir, ext, key);
        if (!taken) break;
    }
    *out = key;
    done;
}

// PUPS-26A309qbG9: the caller's floor, and the counter past a key just used.
static u64 pup_floor(pupopt const *opt) { return opt ? opt->floor : 0; }
static void pup_took(pupopt *opt, u64 key) {
    if (opt && opt->floor <= key) opt->floor = key + 1;
}

// PUPS-26A309qbG9: is `key` on the caller's list of merged-away runs?
static b8 pup_dead(pupopt const *opt, u64 key) {
    if (!opt || !opt->dead) return NO;
    $for(u64, k, u64bData(opt->dead)) if (*k == key) return YES;
    return NO;
}

// DOG-027: restore the dict's "runs first, memtable last" invariant — a push
// lands PAST the memtable entry, so rotate it back to the end.  Stable for the
// runs, so their oldest-first order survives.
static void pup_tail(pup256b pups) {
    pup256 *base = pup256bDataHead(pups);
    u32 n = (u32)pup256bDataLen(pups);
    for (u32 i = 0; i < n;) {
        if (base[i].key != PUP_MEM_KEY) { i++; continue; }
        pup256 t = base[i];
        memmove(base + i, base + i + 1, (size_t)(n - i - 1) * sizeof(pup256));
        base[n - 1] = t;
        n--;  //  settled at the tail; the next entry slid into slot i
    }
}

// DOG-046: is `key` already mapped in DATA?  A refresh maps only what the
// dict lacks, so a foreign run is never dicted (and mmap'd) twice.
static b8 pup_dicted(pup256b pups, u64 key) {
    $for(pup256, p, pup256bData(pups))
        if (p->key == key) return YES;
    return NO;
}

// DOG-046: the ONE directory scan, shared by the cold open and the mid-flight
// refresh.  `dedup` skips a key DATA already holds — what a refresh needs and
// what PUPOpenAll must NOT do (the same basename in another dir is another
// file, see the header's "not idempotent" contract).
// PUPS-26A309qbG9: keys above `ceil` and the caller's dead runs are not runs.
static ok64 pup_scan(pup256b pups, path8sc dir, u8csc ext, b8 dedup, u64 ceil,
                     pupopt const *opt) {
    sane(pups != NULL && u8csOK(dir) && u8csOK(ext));

    //  Two-phase: walk the directory with FILEIter collecting pup_keys, sort,
    //  then map each file in order.  PUPS-26A276Xh2e: a name is derived from
    //  its key (the 10-char RON64 pad round-trips), so only keys are kept, in
    //  a map-max-once buffer: no FILE_MAX_OPEN cap.
    a_path(dpat, dir);
    fileit it = {};
    //  CODE-26A23CSStk: only an ABSENT dir is an empty stack; one that exists
    //  but cannot be listed (EMFILE, EACCES) is an unreadable one.
    ok64 o = FILEIterOpen(&it, dpat);
    if (o == FILENONE) done;
    if (o != OK) fail(o);

    Bu64 keys = {};
    try(u64bMap, keys, PUP_SCAN_MAX);
    nedo { FILEIterClose(&it); fail(__); }

    scan(FILENext, &it) {
        if (it.type != DT_REG) continue;
        u8cs base = {};
        PATHu8sBase(base, u8bDataC(it.path));
        u64 sq = pup_parse_seqno(base, ext);
        //  DOG-027: anything that is not a `<10-RON64><ext>` run (a pre-ron64
        //  name, a tmp, a stray file) is skipped, never unlinked.
        if (sq == 0 || sq > ceil || pup_dead(opt, sq)) continue;
        //  CODE-26A23CSStk: past PUP_SCAN_MAX runs the rest would go
        //  unscanned — fail, a stack is never read short.
        if (u64bFeed1(keys, sq) != OK) {
            __ = PUPNOROOM;
            break;
        }
    }
    //  Close the iterator UNCONDITIONALLY: a non-END FILENext error
    //  (e.g. PATHNOROOM on an over-long path) leaves `__ != END`, so a
    //  bare `seen(END)` would `fail()` and skip the close — leaking the
    //  opendir() DIR handle.  Close first, then propagate the error.
    FILEIterClose(&it);
    if (__ != END) { u64bUnMap(keys); fail(__); }
    __ = OK;

    a_dup(u64, ks, u64bData(keys));
    u64sSort(ks);

    //  Pup layer is not idempotent: every file in `dir` becomes a new
    //  entry in `pups`.  Avoiding double-scans of the same dir is the
    //  caller's job (e.g. skip a prefix already loaded from a shared
    //  parent directory).
    $for(u64, kp, ks) {
        a_path(fpath);
        try(pup_path, fpath, dir, *kp, ext);
        nedo break;
        if (dedup && pup_dicted(pups, *kp)) {
            //  A pre-PUPS-005 ladder merge renamed onto its youngest source's
            //  name, so a dicted key may hold another writer's merged run.
            pup_remap(pups, $path(fpath), *kp);
            continue;
        }
        //  CODE-26A23CSStk: a run on disk that cannot be mapped fails the
        //  open, never vanishes from it: PUPEMPTY for a 0-byte run (a crashed
        //  non-durable create), PUPNOMAP for anything else (fds, perms).
        //  PUPS-26A309qbG9: PUPGONE for one unlinked since the listing.
        pup256 ent = {};
        try(pup_map, &ent, $path(fpath), *kp);
        nedo break;
        //  On a `pups` overflow the just-mapped run is NOT yet recorded in
        //  `pups`, so PUPClose can never reclaim it — unmap it here.
        try(pup256bPush, pups, &ent);
        nedo { pup_unmap(&ent); break; }
    }
    u64bUnMap(keys);
    nedo fail(__);
    //  DOG-027: the runs landed past a live memtable entry — put it back at
    //  the tail, where PUPCount and pup_mem expect it.
    pup_tail(pups);
    done;
}

ok64 PUPOpenAll(pup256b pups, path8sc dir, u8csc ext) {
    return pup_scan(pups, dir, ext, NO, ~0ULL, NULL);
}

ok64 PUPOpenTo(pup256b pups, path8sc dir, u8csc ext, u64 ceil) {
    return pup_scan(pups, dir, ext, NO, ceil, NULL);
}

// DOG-046: map in the runs another writer sealed into `dir` since this handle
// opened.  Runs BEFORE every seal and every ladder, so the key pick and the
// 1/8 policy both weigh the REAL stack, not this handle's stale view.
// PUPS-26A309qbG9: the runs `opt` holds dead stay out — they were merged away.
static ok64 pup_refresh(pup256b pups, path8s dir, u8cs ext,
                        pupopt const *opt) {
    sane(pups != NULL && $ok(dir) && $ok(ext));
    call(pup_scan, pups, dir, ext, YES, ~0ULL, opt);
    //  A foreign run lands past what was dicted, so re-sort DATA oldest-first;
    //  the ALL-ONES memtable key sorts itself back to the tail.
    a_dup(pup256, all, pup256bData(pups));
    pup256sSort(all);
    done;
}

ok64 PUPOpenAside(pup256b pups, path8sc dir, u8csc ext) {
    sane(pups != NULL && u8csOK(dir) && u8csOK(ext));
    //  Collapse the current DATA into PAST: the previously-active leaf
    //  becomes part of the read-only context, leaving DATA empty and
    //  ready for the next branch's entries.  pup256bUsedAll advances the
    //  PAST/DATA boundary to the DATA/IDLE boundary (no buffer-poke).
    if (pup256bDataLen(pups) > 0)
        pup256bUsedAll(pups);
    return PUPOpenAll(pups, dir, ext);
}

// PUPS-26A24FhuO9: an empty run on disk, an empty ToC block's trailer.
static u8c const PUP_NIL_RUN[PUP_TOC_TAIL_W] = {0, 0, 0, 0, 0, 'T', 'C'};

// PUPS-004: tmp + atomic rename of `bytes` onto `<key><ext>`.
// PUPS-005: callers pick a free key; nothing renames onto a live run.  With
// `durable` the tmp is fsync'd before the rename and the directory after it,
// so the run is on disk before a ladder merge unlinks its sources.
static ok64 pup_write(path8s dir, u8cs ext, u8cs bytes, u64 pup_key,
                      b8 durable) {
    sane($ok(dir) && $ok(ext));
    a_path(idxpath);
    call(pup_path, idxpath, dir, pup_key, ext);
    a_path(tmppath);
    call(pup_path, tmppath, dir, pup_key, ext);
    a_cstr(tmpsuf, ".tmp");
    call(u8bFeed, tmppath, tmpsuf);
    call(PATHu8bTerm, tmppath);

    //  PUPS-26A24FhuO9: an empty run is the bare trailer of an empty ToC, so
    //  it maps, and a 0-byte file stays what a crashed create leaves.
    u8cs body = {bytes[0], bytes[1]};
    if (u8csEmpty(body)) {
        body[0] = PUP_NIL_RUN;
        body[1] = PUP_NIL_RUN + PUP_TOC_TAIL_W;
    }
    int wfd = -1;
    call(FILECreate, &wfd, $path(tmppath));
    if (wfd >= 0) {
        callsafe(FILEFeedAll(wfd, body), close(wfd));
        if (durable) callsafe(FILESync(&wfd), close(wfd));
        close(wfd);
    }
    call(FILERename, $path(tmppath), $path(idxpath));
    if (durable) {
        int dfd = -1;
        call(FILEOpenDir, &dfd, dir);
        callsafe(FILESync(&dfd), close(dfd));
        close(dfd);
    }
    done;
}

// PUPS-004: map the run on `<key><ext>` and push it into the dict.
static ok64 pup_book(pup256b pups, path8s dir, u8cs ext, u64 pup_key) {
    sane(pups != NULL && $ok(dir) && $ok(ext));
    a_path(idxpath);
    call(pup_path, idxpath, dir, pup_key, ext);
    pup256 ent = {};
    call(pup_map, &ent, $path(idxpath), pup_key);
    //  On a `pups` overflow the run just mapped above is not yet
    //  recorded, so PUPClose can never reclaim it — unmap it before
    //  propagating the push error.
    try(pup256bPush, pups, &ent);
    if (__ != OK) { pup_unmap(&ent); fail(__); }
    //  DOG-027: a push lands past a live memtable entry — put that entry back
    //  at the tail (a no-op for a stack with no memtable).
    pup_tail(pups);
    done;
}

static ok64 pup_create_at(pup256b pups, path8s dir, u8cs ext, u8cs bytes,
                          u64 pup_key, b8 durable) {
    sane(pups != NULL && $ok(dir) && $ok(ext) && pup_key > 0);

    //  DOG-027: refuse a taken key — DATA-side (ThinTail first) or a file on
    //  its name.  The key is the CALLER's, so dog refuses; it cannot bump it.
    {
        b8 taken = NO;
        call(pup_key_taken, &taken, pups, dir, ext, pup_key);
        if (taken) return PUPFAIL;
    }
    call(pup_write, dir, ext, bytes, pup_key, durable);
    call(pup_book, pups, dir, ext, pup_key);
    done;
}

ok64 PUPCreateAtAs(pup256b pups, path8s dir, u8cs ext, u8cs bytes,
                   u64 pup_key, b8 durable) {
    sane(pups != NULL && $ok(dir) && $ok(ext) && pup_key > 0);
    int lk = -1;
    call(PUPLock, &lk, dir);
    try(pup_create_at, pups, dir, ext, bytes, pup_key, durable);
    PUPUnlock(&lk);
    nedo fail(__);
    done;
}

ok64 PUPCreateAt(pup256b pups, path8s dir, u8cs ext, u8cs bytes,
                 u64 pup_key) {
    return PUPCreateAtAs(pups, dir, ext, bytes, pup_key, NO);
}

ok64 PUPCreateOpt(pup256b pups, path8s dir, u8cs ext, u8cs bytes, b8 durable,
                  pupopt *opt) {
    sane(pups != NULL && $ok(dir) && $ok(ext));
    //  Pick `max(RONNow, max(DATA)+1)` so two writers / two parallel
    //  sub-shards collapsed into one project never accidentally pick
    //  the same key.  RONNow has 1ms granularity; tight writer loops
    //  within the same millisecond fall back to local max+1.  Both
    //  paths preserve strict global ordering and 60-bit fit (RONNow
    //  is ron60-bounded; the +1 bump only triggers when we're already
    //  ron60-shaped).
    //  DOG-027: hoisted into pup_next_key — flush/commit share it; then
    //  past any name a case-folding filesystem already has a file on.
    //  The lock spans the pick AND the rename onto it.
    int lk = -1;
    call(PUPLock, &lk, dir);
    u64 new_seqno = 0;
    try(pup_free_key, &new_seqno, pups, dir, ext, pup_floor(opt));
    then try(pup_create_at, pups, dir, ext, bytes, new_seqno, durable);
    PUPUnlock(&lk);
    nedo fail(__);
    pup_took(opt, new_seqno);
    done;
}

ok64 PUPCreateAs(pup256b pups, path8s dir, u8cs ext, u8cs bytes, b8 durable) {
    return PUPCreateOpt(pups, dir, ext, bytes, durable, NULL);
}

ok64 PUPCreate(pup256b pups, path8s dir, u8cs ext, u8cs bytes) {
    return PUPCreateAs(pups, dir, ext, bytes, NO);
}

// PUPS-004: unmap the i-th run and close the dict gap; `rm` NO leaves the file.
static ok64 pup_drop(pup256b pups, path8s dir, u8cs ext, u32 i, b8 rm) {
    sane(pups != NULL && $ok(dir) && $ok(ext));
    //  DOG-027: index over RUNS, not entries — the memtable's key would unlink
    //  a file named `~0`; the memmove below keeps it last.
    u32 n = PUPCount(pups), all = (u32)pup256bDataLen(pups);
    if (i >= n) return PUPFAIL;
    pup256 *base = pup256bDataHead(pups);
    pup_unmap(base + i);
    if (rm) {
        a_path(ulpath);
        call(pup_path, ulpath, dir, base[i].key, ext);
        unlink((char *)u8bDataHead(ulpath));
    }
    memmove(base + i, base + i + 1, (size_t)(all - i - 1) * sizeof(pup256));
    //  Trim DATA end back by one record (pup256bShed rolls the DATA/IDLE
    //  boundary back into DATA — no buffer-poke).
    pup256bShed(pups, 1);
    done;
}

ok64 PUPDropAt(pup256b pups, path8s dir, u8cs ext, u32 i) {
    return pup_drop(pups, dir, ext, i, YES);
}

ok64 PUPThinTail(pup256b pups, path8s dir, u8cs ext, u32 m) {
    sane(pups != NULL && $ok(dir) && $ok(ext));
    u32 n = PUPCount(pups);
    if (m > n) m = n;
    //  DOG-027: the youngest run is the last one PUPCount admits.
    while (m-- > 0) call(PUPDropAt, pups, dir, ext, PUPCount(pups) - 1);
    done;
}

ok64 PUPClose(pup256b pups) {
    sane(pups != NULL);
    if (BNULL(pups)) done;
    //  PUPS-26A276Xh2e: every entry is a mapping, the memtable's included; no
    //  fd is left to close.  The memtable's unsealed puts go with it.
    $for(pup256, e, pup256bData(pups)) pup_unmap(e);
    pup256bFree(pups);
    done;
}

// =============================================================
// --- DOG-035: the run ToC — [data][toc][len:5]["TC"] ---
// =============================================================

size_t PUPTocRows(u32 row) {
    if (row == 0) return 0;
    size_t page = FILESysPage();
    //  DOG-035: a straddling record would make cell `i` a mid-row slice — the
    //  caller simply keeps today's shape instead.
    return page % row ? 0 : page / row;
}

// DOG-035: how many cells `len` data bytes take — 0 when `row` is 0, a
// record straddles a page, or the run is under two pages.
static size_t pup_toc_cells(size_t len, u32 row) {
    size_t per = PUPTocRows(row);
    if (per == 0 || len == 0 || len % row) return 0;
    size_t page = per * row, n = (len + page - 1) / page;
    return n < 2 ? 0 : n;
}

// DOG-035: feed `data`'s ToC block plus the 7-byte trailer.  Feeds NOTHING when
// the run takes no ToC, so a plain run keeps today's shape byte for byte.
static ok64 pup_toc_feed(u8s into, u8csc data, u32 row) {
    sane($ok(into));
    size_t len = (size_t)u8csLen(data);
    size_t cells = pup_toc_cells(len, row);
    if (cells == 0) done;
    size_t page = PUPTocRows(row) * row;
    for (size_t i = 0; i < cells; i++) {
        a_part(u8c, cell, data, i * page, row);
        call(u8sFeed, into, cell);
    }
    size_t bytes = cells * row;
    for (u32 k = 0; k < PUP_TOC_LEN_W; k++)
        call(u8sFeed1, into, (u8)(bytes >> (8 * k)));
    a_cstr(magic, "TC");
    call(u8sFeed, into, magic);
    done;
}

// DOG-035: split a run's mapped bytes into [data][toc].  Presence is CHECKED,
// never inferred — magic, the length fits, the data residue is 0 mod row.
static void pup_toc_split(u8csp data, u8csp toc, u8csc raw, u32 row) {
    u8csMv(data, raw);
    toc[0] = toc[1] = raw[1];
    if ((size_t)u8csLen(raw) == PUP_TOC_TAIL_W &&
        memcmp(raw[0], PUP_NIL_RUN, PUP_TOC_TAIL_W) == 0) {
        data[1] = raw[0];  //  an empty run, at every row width
        return;
    }
    if (row == 0) return;
    size_t size = (size_t)u8csLen(raw);
    if (size < PUP_TOC_TAIL_W) return;
    a_tail(u8c, tail, raw, PUP_TOC_TAIL_W);
    if (tail[0][PUP_TOC_LEN_W] != 'T' ||
        tail[0][PUP_TOC_LEN_W + 1] != 'C') return;
    size_t bytes = 0;
    for (u32 k = 0; k < PUP_TOC_LEN_W; k++)
        bytes |= (size_t)tail[0][k] << (8 * k);
    if (bytes + PUP_TOC_TAIL_W > size) return;
    if ((size - PUP_TOC_TAIL_W - bytes) % row) return;
    data[1] = tail[0] - bytes;
    toc[0] = data[1];
    toc[1] = tail[0];
}

// DOG-035: cut the i-th RUN's mapped bytes into both halves — one slot lookup,
// the ONE place the ToC block is separated from the rows.
static void pup_slot(u8csp data, u8csp toc, pup256b pups, u32 i, u32 row) {
    data[0] = data[1] = NULL;
    toc[0] = toc[1] = NULL;
    if (i >= PUPCount(pups)) return;
    pup256 const *base = pup256bDataHead(pups);
    if (base[i].data[0] == NULL) return;
    pup_toc_split(data, toc, base[i].data, row);
}

void PUPDataRow(u8csp out, pup256b pups, u32 i, u32 row) {
    u8cs toc = {};
    pup_slot(out, toc, pups, i, row);
}

void PUPToc(u8csp out, pup256b pups, u32 i, u32 row) {
    u8cs data = {};
    pup_slot(data, out, pups, i, row);
}

void PUPData(u8csp out, pup256b pups, u32 i) {
    PUPDataRow(out, pups, i, 0);
}

void PUPDataAllRow(u8csp out, pup256b pups, u32 i, u32 row) {
    out[0] = NULL; out[1] = NULL;
    pup256s all = {};
    pup256PastDataS(pups, all);
    //  DOG-027: the memtable entry sits at the tail of PastData too.
    size_t n = (size_t)PUPCountAll(pups);
    if ((size_t)i >= n) return;
    if (all[0][i].data[0] == NULL) return;
    u8cs toc = {};
    pup_toc_split(out, toc, all[0][i].data, row);
}

void PUPDataAll(u8csp out, pup256b pups, u32 i) {
    PUPDataAllRow(out, pups, i, 0);
}

ok64 PUPAllDataRow(u8csb out, pup256b pups, u32 row) {
    sane(out && pups);
    u8csbReset(out);
    u32 n = PUPCount(pups);
    for (u32 i = 0; i < n; i++) {
        u8cs s = {};
        PUPDataRow(s, pups, i, row);
        if (s[0] == NULL) continue;
        call(u8csbFeed1, out, s);
    }
    done;
}

ok64 PUPAllData(u8csb out, pup256b pups) {
    return PUPAllDataRow(out, pups, 0);
}

// =============================================================
// --- PUPS-26A276Xh2e: the memtable — an anonymous BOX the puts land in ---
// =============================================================

// DOG-027: the live memtable is the dict's trailing ALL-ONES entry, if any.
static pup256 *pup_mem(pup256b pups) {
    u32 n = (u32)pup256bDataLen(pups);
    if (n == 0) return NULL;
    pup256 *last = pup256bDataHead(pups) + (n - 1);
    return last->key == PUP_MEM_KEY ? last : NULL;
}

static b8 pup_mem_empty(pup256 const *e) {
    return e == NULL || e->data[1] == e->data[0];
}

// PUPS-26A276Xh2e: the entry as BOX's record buffer — base, PAST end (where
// the caller's youngest run ends), DATA end, and the mapping's term.
static void pup_mem_buf(Bu8 mem, pup256 const *e, u8csb runs) {
    mem[0] = (u8 *)e->data[0];
    mem[2] = (u8 *)e->data[1];
    mem[3] = mem[0] + e->val;
    size_t n = u8csbDataLen(runs);
    mem[1] = n ? (u8 *)u8csbDataHead(runs)[n - 1][1] : mem[0];
}

// DOG-032: an absent memtable is mapped `pages` WHOLE OS pages (0 = one), once
// per handle; an empty one of another size is remapped.  Whenever it starts
// empty, the caller's runs are reset.
static ok64 pup_mem_open(pup256b pups, u8csb runs, u32 pages) {
    sane(pups != NULL && runs != NULL);
    pup256 *e = pup_mem(pups);
    if (!pup_mem_empty(e)) done;
    u8csbReset(runs);
    size_t size = (size_t)(pages ? pages : PUP_MEM_PAGES) * FILESysPage();
    if (e && e->val == size) done;
    if (e) {
        pup_unmap(e);
        pup256bShed(pups, 1);
    }
    Bu8 m = {};
    call(u8bMap, m, size);
    pup256 ent = {.key = PUP_MEM_KEY, .val = size, .data = {m[0], m[0]}};
    try(pup256bPush, pups, &ent);
    nedo {
        u8bUnMap(m);
        fail(__);
    }
    done;
}

// DOG-027: INTERNAL sync.  BOX-26A26EbjLj: a non-empty DATA segment becomes a
// run, then the 1/8 ladder over the memtable's runs.
static ok64 pup_sync(pup256 *e, u8csb runs, pupsync sync) {
    sane(e != NULL && runs != NULL && sync != NULL);
    Bu8 mem = {};
    pup_mem_buf(mem, e, runs);
    if (u8bDataLen(mem) == 0) done;
    call(sync, mem, runs);
    e->data[1] = mem[2];
    done;
}

// DOG-027: the caller's typed merge (or, with no merge, the copy of the ONE
// source) straight into the output run's mapping.
// PUPS-26A25EfKtr: the tmp file is ftruncate'd to the bound (the OS hands out
// zero pages lazily), mapped RW, merged + ToC'd in place, then trimmed to what
// was written.  No heap scratch, no write() copy.
static ok64 pup_collapse_into(u8bp out, u8css srcs, u32 row, b8 bottom,
                              pupmerge merge) {
    sane(u8bOK(out));
    u8s into = {u8bIdleHead(out), u8bTerm(out)};
    if (merge != NULL) {
        call(merge, into, srcs, bottom);
    } else {
        test($len(srcs) == 1, PUPFAIL);
        call(u8sFeed, into, srcs[0][0]);
    }
    a_dup(u8c, merged, ((u8cs){u8bIdleHead(out), into[0]}));
    //  PUPS-001: nothing survived — no ToC either; the caller sees DATA empty.
    if (!u8csEmpty(merged)) call(pup_toc_feed, into, merged, row);
    ((u8c **)out)[2] = into[0];
    done;
}

// PUPS-26A276Xh2e: the ONE run writer, shared by the memtable seal and the
// ladder: merge `srcs` into `<key><ext>.tmp`, ToC, trim, optional fsync, atomic
// rename onto `<key><ext>`, book.  The tmp's fd lives only for this call.  A
// merge that leaves nothing writes no run.
static ok64 pup_merge_run(pup256b pups, path8s dir, u8cs ext, u64 key,
                          u8css srcs, u32 row, b8 bottom, pupmerge merge,
                          b8 durable) {
    sane(pups != NULL && $ok(dir) && $ok(ext));
    size_t total = 0;
    $for(u8cs, r, srcs) total += (size_t)u8csLen(*r);
    //  DOG-035: room for the merged run's own ToC block + trailer on top —
    //  one cell per page the merge can fill, and dedup only shortens it.
    size_t page = PUPTocRows(row) * row;
    size_t bound = total + (page ? (total / page + 2) * row : 0) +
                   PUP_TOC_TAIL_W;
    a_path(runpath);
    call(pup_path, runpath, dir, key, ext);
    a_path(tmppath);
    call(pup_path, tmppath, dir, key, ext);
    a_cstr(tmpsuf, ".tmp");
    call(u8bFeed, tmppath, tmpsuf);
    call(PATHu8bTerm, tmppath);
    u8bp out = NULL;
    call(FILEMapCreate, &out, $path(tmppath), bound);
    u8 *base = out[0];
    size_t full = (size_t)(out[3] - out[0]);
    //  PUPS-26B034pwDe: no MADV_POPULATE_WRITE — the bound is the sum of the
    //  sources, dedup and dropped tombstones leave most of it unwritten; the
    //  merge faults in only the pages it fills.
    try(pup_collapse_into, out, srcs, row, bottom, merge);
    b8 empty = __ == OK && u8bDataLen(out) == 0;
    //  The output is trimmed to its written length, then fsync'd, before the
    //  rename makes it a run.
    if (__ == OK && !empty) {
        int fd = FILEBookedFD(out);
        try(FILETrimMap, out);
        then if (durable) try(FILESync, &fd);
        //  the trim shortened the record, not the mapping: release its tail
        size_t pg = FILESysPage();
        size_t kept = ((size_t)(out[3] - out[0]) + pg - 1) / pg * pg;
        if (kept < full) munmap((void *)(base + kept), full - kept);
    }
    FILEUnMap(out);
    if (__ != OK || empty) FILEUnLink($path(tmppath));
    nedo fail(__);
    if (empty) done;
    call(FILERename, $path(tmppath), $path(runpath));
    if (durable) {
        int dfd = -1;
        call(FILEOpenDir, &dfd, dir);
        callsafe(FILESync(&dfd), close(dfd));
        close(dfd);
    }
    call(pup_book, pups, dir, ext, key);
    done;
}

// PUPS-26A276Xh2e: seal the memtable as a run — its segment sealed, its runs
// merged by the caller's `merge` into the new run file in one streamed write
// (that write IS the save), then the memtable starts empty.
// DOG-035: `row` (the record width, 0 = none) decides whether a ToC block
// follows the data.
static ok64 pup_seal_run(pup256b pups, u8csb runs, path8s dir, u8cs ext,
                         pupsync sync, pupmerge merge, u32 row, b8 durable,
                         pupopt *opt) {
    sane(pups != NULL && runs != NULL && $ok(dir) && $ok(ext));
    //  DOG-046: pick the fresh key against the runs OTHER writers sealed
    //  meanwhile, and leave the dict a true oldest-first stack for the ladder.
    call(pup_refresh, pups, dir, ext, opt);
    pup256 *e = pup_mem(pups);
    if (!e) return PUPFAIL;
    call(pup_sync, e, runs, sync);
    a_pad(u8cs, srcs, ABC_HIT_MAX_RUNS);
    $for(u8cs, r, u8csbData(runs)) {
        if (u8csEmpty(*r)) continue;
        call(u8csbFeed1, srcs, *r);
    }
    u8cs *sl = u8csbDataHead(srcs);
    u8css ss = {sl, sl + u8csbDataLen(srcs)};
    if (!$empty(ss)) {
        u64 key = 0;
        call(pup_free_key, &key, pups, dir, ext, pup_floor(opt));
        //  never `bottom`: the dead rows of a seal still shadow older runs
        call(pup_merge_run, pups, dir, ext, key, ss, row, NO, merge, durable);
        pup_took(opt, key);
    }
    //  the booked run went under the memtable entry: fetch it again
    e = pup_mem(pups);
    e->data[1] = e->data[0];
    u8csbReset(runs);
    done;
}

// DOG-027: merge the youngest `m` runs into one fresh run — the caller's
// `merge` does the typed merge, Pup owns the tmp + rename seal and the
// source unlinks.
// PUPS-26A309qbG9: or, with `opt->dead`, hands the sources' keys back instead.
static ok64 pup_collapse(pup256b pups, path8s dir, u8cs ext, pupmerge merge,
                         u32 row, u8css stack, size_t m, pupopt *opt) {
    sane(pups != NULL && $ok(dir) && $ok(ext) && merge != NULL && m > 1);
    u64bp dead = opt ? opt->dead : NULL;
    //  room for every source first: a half-returned list would leak files
    if (dead && u64bIdleLen(dead) < m) return PUPNOROOM;
    size_t n = (size_t)$len(stack);
    u8css tail = {stack[0] + (n - m), stack[0] + n};
    //  PUPS-001: the merge spans the whole stack only when it takes every run
    //  and nothing older is inherited; only then may a tombstone be dropped.
    b8 bottom = m == n && PUPCountAll(pups) == PUPCount(pups);
    //  PUPS-005: a run name is never reused — renaming onto the youngest
    //  source (PUPS-004) let a crash keep older sources whose tombstones the
    //  bottom merge had dropped.  The merged run takes the next free key past
    //  the youngest source (sequential; the ladder holds the write lock), is
    //  made durable, booked, and only then are the sources unlinked.
    u64 key = PUPSeqno(pups, PUPCount(pups) - 1) + 1;
    if (key < pup_floor(opt)) key = pup_floor(opt);
    for (b8 taken = YES;; key++) {
        call(pup_key_taken, &taken, pups, dir, ext, key);
        if (!taken) break;
    }
    //  PUPS-005: the sources go OLDEST first.  A crash part way leaves the
    //  merged run (if any) plus a YOUNGEST suffix of the sources: every
    //  tombstone outlives the rows it shadows, so the stack reads as before.
    u32 oldest = PUPCount(pups) - (u32)m;
    //  PUPS-001: nothing survived (every row was dead): no empty run — the
    //  sources just go.
    call(pup_merge_run, pups, dir, ext, key, tail, row, bottom, merge, YES);
    pup_took(opt, key);
    for (size_t k = 0; k < m; k++) {
        if (dead) call(u64bFeed1, dead, PUPSeqno(pups, oldest));
        call(pup_drop, pups, dir, ext, oldest, dead == NULL);
    }
    pup_tail(pups);
    done;
}

//  `fresh`: the caller refreshed under the lock it still holds, no rescan.
static ok64 pup_ladder(pup256b pups, path8s dir, u8cs ext, pupmerge merge,
                       u32 row, pupopt *opt, b8 fresh) {
    sane(pups != NULL && $ok(dir) && $ok(ext));
    //  DOG-046: a ladder over a stale view collapses the wrong tail — map in
    //  what other writers sealed, then weigh.  Only merged runs are unlinked.
    if (!fresh) call(pup_refresh, pups, dir, ext, opt);
    for (;;) {
        //  DOG-035: clipped lengths — a ToC block is not run bytes, so the
        //  ladder must not weigh it and the merge must never see a cell.
        a_pad(u8cs, srcs, ABC_HIT_MAX_RUNS);
        call(PUPAllDataRow, srcs, pups, row);
        u8cs *sl = u8csbDataHead(srcs);
        u8css stack = {sl, sl + u8csbDataLen(srcs)};
        //  The ladder reads run LENGTHS only — byte lengths here, element
        //  counts in a HIT stack; HIT.h stays the one home of the 1/8 policy.
        a_pad(u64, lens, ABC_HIT_MAX_RUNS);
        $for(u8cs, r, stack) call(u64bFeed1, lens, (u64)u8csLen(*r));
        size_t m = HITLadderOverRuns(u64bDataC(lens));
        if (m < 2) done;
        call(pup_collapse, pups, dir, ext, merge, row, stack, m, opt);
    }
}

ok64 PUPLadderOpt(pup256b pups, path8s dir, u8cs ext, pupmerge merge, u32 row,
                  pupopt *opt) {
    sane(pups != NULL && $ok(dir) && $ok(ext));
    if (merge == NULL) done;
    //  One merger per stack: the lock covers the rescan and every collapse.
    int lk = -1;
    call(PUPLock, &lk, dir);
    try(pup_ladder, pups, dir, ext, merge, row, opt, NO);
    PUPUnlock(&lk);
    nedo fail(__);
    done;
}

ok64 PUPLadder(pup256b pups, path8s dir, u8cs ext, pupmerge merge, u32 row) {
    return PUPLadderOpt(pups, dir, ext, merge, row, NULL);
}

//  The seal and the ladder under ONE hold of the write lock: the key pick and
//  the rename onto it are one critical section, and the seal's rescan is the
//  ladder's too (PUPS-26B034dToa: one directory scan per commit).
static ok64 pup_mem_seal(pup256b pups, u8csb runs, path8s dir, u8cs ext,
                         pupsync sync, pupmerge merge, u32 row, b8 durable,
                         pupopt *opt) {
    sane(pups != NULL && $ok(dir) && $ok(ext));
    int lk = -1;
    call(PUPLock, &lk, dir);
    try(pup_seal_run, pups, runs, dir, ext, sync, merge, row, durable, opt);
    then if (merge != NULL)
        try(pup_ladder, pups, dir, ext, merge, row, opt, YES);
    PUPUnlock(&lk);
    nedo fail(__);
    done;
}

ok64 PUPUnlink(path8s dir, u8cs ext, u64s keys) {
    sane($ok(dir) && $ok(ext) && $ok(keys));
    u64sSort(keys);
    $for(u64, k, keys) {
        a_path(fpath);
        call(pup_path, fpath, dir, *k, ext);
        ok64 o = FILEUnLink($path(fpath));
        if (o != OK && o != FILENONE) fail(o);
    }
    done;
}

ok64 PUPPutOpt(pup256b pups, u8csb runs, path8s dir, u8cs ext, u8csc rec,
               pupsync sync, pupmerge merge, u32 row, u32 mem_pages,
               pupopt *opt) {
    sane(pups != NULL && runs != NULL && $ok(dir) && $ok(ext) &&
         u8csOK(rec));
    if (sync == NULL) return PUPFAIL;
    call(pup_mem_open, pups, runs, mem_pages);
    pup256 *e = pup_mem(pups);
    Bu8 mem = {};
    pup_mem_buf(mem, e, runs);
    if (u8bIdleLen(mem) < (size_t)u8csLen(rec)) {
        //  DOG-027: full buffer — seal it into a run, run the ladder, start
        //  the memtable empty again.
        call(pup_mem_seal, pups, runs, dir, ext, sync, merge, row, NO, opt);
        e = pup_mem(pups);
        pup_mem_buf(mem, e, runs);
    }
    call(u8bFeed, mem, rec);
    e->data[1] = mem[2];
    //  BOX-26A26EbjLj: past c (PUP_COLLAPSE, PUPx.h's ABC_BOX_SEG) the segment becomes a run
    if (u8bDataLen(mem) > PUP_COLLAPSE) call(pup_sync, e, runs, sync);
    done;
}

ok64 PUPPutMem(pup256b pups, u8csb runs, path8s dir, u8cs ext, u8csc rec,
               pupsync sync, pupmerge merge, u32 row, u32 mem_pages) {
    return PUPPutOpt(pups, runs, dir, ext, rec, sync, merge, row, mem_pages,
                     NULL);
}

ok64 PUPPut(pup256b pups, u8csb runs, path8s dir, u8cs ext, u8csc rec,
            pupsync sync, pupmerge merge, u32 row) {
    return PUPPutMem(pups, runs, dir, ext, rec, sync, merge, row,
                     PUP_MEM_PAGES);
}

ok64 PUPCommitOpt(pup256b pups, u8csb runs, path8s dir, u8cs ext,
                  pupsync sync, pupmerge merge, u32 row, b8 durable,
                  pupopt *opt) {
    sane(pups != NULL && runs != NULL && $ok(dir) && $ok(ext));
    if (sync == NULL) return PUPFAIL;
    if (pup_mem_empty(pup_mem(pups))) done;
    call(pup_mem_seal, pups, runs, dir, ext, sync, merge, row, durable, opt);
    done;
}

ok64 PUPCommitAs(pup256b pups, u8csb runs, path8s dir, u8cs ext,
                 pupsync sync, pupmerge merge, u32 row, b8 durable) {
    return PUPCommitOpt(pups, runs, dir, ext, sync, merge, row, durable, NULL);
}

ok64 PUPCommit(pup256b pups, u8csb runs, path8s dir, u8cs ext, pupsync sync,
               pupmerge merge, u32 row) {
    return PUPCommitAs(pups, runs, dir, ext, sync, merge, row, YES);
}

ok64 PUPAllRunsToc(u8csb out, u8csb tocs, pup256b pups, u8csb runs,
                   pupsync sync, u32 row) {
    sane(out && pups != NULL);
    if (sync == NULL) return PUPFAIL;
    u8csbReset(out);
    if (tocs) u8csbReset(tocs);
    u8cs none = {};
    u32 n = PUPCount(pups);
    for (u32 i = 0; i < n; i++) {
        u8cs s = {}, t = {};
        //  DOG-035: clipped — a ToC cell is a REAL key, so an unclipped source
        //  would feed the keep-last dedup duplicate rows and eat data.
        pup_slot(s, t, pups, i, row);
        if (s[0] == NULL) continue;
        call(u8csbFeed1, out, s);
        if (tocs) call(u8csbFeed1, tocs, t);
    }
    pup256 *e = pup_mem(pups);
    if (pup_mem_empty(e)) done;
    test(runs != NULL, PUPFAIL);
    //  BOX-26A26EbjLj: the segment sealed, the memtable's runs follow as
    //  they are, oldest first.
    call(pup_sync, e, runs, sync);
    $for(u8cs, r, u8csbData(runs)) {
        if (u8csEmpty(*r)) continue;
        call(u8csbFeed1, out, *r);
        if (tocs) call(u8csbFeed1, tocs, none);
    }
    done;
}

ok64 PUPAllRuns(u8csb out, pup256b pups, u8csb runs, pupsync sync, u32 row) {
    return PUPAllRunsToc(out, NULL, pups, runs, sync, row);
}
