#include "PUPS.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
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
//  Reentrant within a process (a depth count per directory), so a caller may
//  hold it across several stacks (PUPLock) while the entry points take it
//  again.  Not thread-safe, and a forked child must not inherit a held lock.
#define PUP_LOCKS 8
static struct {
    char path[FILE_PATH_MAX_LEN];
    int fd, depth;
} pup_held[PUP_LOCKS];

ok64 PUPLock(int *slot, path8s dir) {
    sane(slot != NULL && $ok(dir));
    *slot = -1;
    a_path(lp);
    call(PATHu8bDup, lp, dir);
    a_cstr(name, ".lock");
    call(PATHu8bPush, lp, name);
    char const *path = (char const *)u8bDataHead(lp);
    int free_slot = -1;
    for (int i = 0; i < PUP_LOCKS; i++) {
        if (pup_held[i].depth == 0) { if (free_slot < 0) free_slot = i; continue; }
        if (strcmp(pup_held[i].path, path) != 0) continue;
        pup_held[i].depth++;
        *slot = i;
        done;
    }
    if (free_slot < 0 || strlen(path) >= sizeof pup_held[0].path) return PUPFAIL;
    int f = open(path, O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (f < 0) return PUPFAIL;
    try(FILELock, &f, YES);
    if (__ != OK) { close(f); fail(__); }
    strcpy(pup_held[free_slot].path, path);
    pup_held[free_slot].fd = f;
    pup_held[free_slot].depth = 1;
    *slot = free_slot;
    done;
}

void PUPUnlock(int *slot) {
    if (*slot < 0) return;
    if (--pup_held[*slot].depth == 0) close(pup_held[*slot].fd);  //  drops the flock
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

//  PUPS-001: the dict entry for `key` maps an inode; when the file now on its
//  name is another inode (a merge from another writer replaced it), map the
//  new one in place.  A file that is gone keeps its old mapping, as before.
static void pup_remap(kv64b pups, path8s fpath, u64 key) {
    kv64 *base = (kv64 *)kv64bDataHead(pups);
    u32 n = (u32)kv64bDataLen(pups);
    for (u32 i = 0; i < n; i++) {
        if (base[i].key != key || key == PUP_MEM_KEY) continue;
        struct stat cur, mapped;
        if (stat((char const *)fpath[0], &cur) != 0) return;
        if (fstat((int)base[i].val, &mapped) == 0 && mapped.st_ino == cur.st_ino &&
            mapped.st_dev == cur.st_dev)
            return;
        u8bp buf = NULL;
        if (FILEMapRO(&buf, fpath) != OK) return;
        int fd = FILEBookedFD(buf);
        if (fd < 0) { FILEUnMap(buf); return; }
        u8bp old = FILE_WANT_BUFS[base[i].val];
        if (old && old[0]) FILEUnMap(old);
        base[i].val = (u64)fd;
        return;
    }
}

// DOG-027: the ONE monotonic pup_key scheme — max(RONNow, max(DATA)+1),
// skipping the reserved ALL-ONES memtable entry.
static u64 pup_next_key(kv64b pups) {
    u64 key = (u64)RONNow();
    kv64 const *db = (kv64 const *)kv64bDataHead(pups);
    kv64 const *de = (kv64 const *)kv64bIdleHead(pups);
    for (kv64 const *p = db; p < de; p++)
        if (p->key != PUP_MEM_KEY && p->key >= key) key = p->key + 1;
    if (key == 0) key = 1;
    return key;
}

// DOG-027: is `key` spoken for — by a live DATA entry, or by a file already
// sitting on the name it composes?
static ok64 pup_key_taken(b8 *out, kv64b pups, path8s dir, u8cs ext,
                              u64 key) {
    sane(out != NULL && pups != NULL && $ok(dir) && $ok(ext));
    *out = YES;
    if (key == PUP_MEM_KEY) done;       //  reserved for the memtable
    kv64 const *db = (kv64 const *)kv64bDataHead(pups);
    kv64 const *de = (kv64 const *)kv64bIdleHead(pups);
    for (kv64 const *p = db; p < de; p++)
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
static ok64 pup_free_key(u64 *out, kv64b pups, path8s dir, u8cs ext) {
    sane(out != NULL && pups != NULL && $ok(dir) && $ok(ext));
    u64 key = pup_next_key(pups);
    for (b8 taken = YES; ; key++) {
        call(pup_key_taken, &taken, pups, dir, ext, key);
        if (!taken) break;
    }
    *out = key;
    done;
}

// DOG-027: restore the dict's "runs first, memtable last" invariant — a push
// lands PAST the memtable entry, so rotate it back to the end.  Stable for the
// runs, so their oldest-first order survives.
static void pup_tail(kv64b pups) {
    kv64 *base = (kv64 *)kv64bDataHead(pups);
    u32 n = (u32)kv64bDataLen(pups);
    for (u32 i = 0; i < n;) {
        if (base[i].key != PUP_MEM_KEY) { i++; continue; }
        kv64 t = base[i];
        memmove(base + i, base + i + 1, (size_t)(n - i - 1) * sizeof(kv64));
        base[n - 1] = t;
        n--;  //  settled at the tail; the next entry slid into slot i
    }
}

// DOG-046: is `key` already mapped in DATA?  A refresh maps only what the
// dict lacks, so a foreign run is never dicted (and mmap'd) twice.
static b8 pup_dicted(kv64b pups, u64 key) {
    kv64 const *db = (kv64 const *)kv64bDataHead(pups);
    kv64 const *de = (kv64 const *)kv64bIdleHead(pups);
    for (kv64 const *p = db; p < de; p++)
        if (p->key == key) return YES;
    return NO;
}

// DOG-046: the ONE directory scan, shared by the cold open and the mid-flight
// refresh.  `dedup` skips a key DATA already holds — what a refresh needs and
// what PUPOpenAll must NOT do (the same basename in another dir is another
// file, see the header's "not idempotent" contract).
static ok64 pup_scan(kv64b pups, path8sc dir, u8csc ext, b8 dedup) {
    sane(pups != NULL && u8csOK(dir) && u8csOK(ext));

    //  Two-phase: walk directory with FILEIter to collect (pup_key → name)
    //  pairs in a Bkv64, sort by key, then map each file in order.
    //  Keeping the iterator and FILEMapRO disjoint avoids any aliasing
    //  between the iterator's path buffer and FILEMapRO's bookkeeping.
    a_path(dpat, dir);
    fileit it = {};
    if (FILEIterOpen(&it, dpat) != OK) done;  // dir absent → empty stack

    Bkv64 seqnos = {};
    call(kv64bAllocate, seqnos, FILE_MAX_OPEN);
    //  names are <=21 bytes (10 seqno + 11 ext); 32 is plenty.
    a_pad(u8, namebuf, FILE_MAX_OPEN * 32);

    scan(FILENext, &it) {
        if (it.type != DT_REG) continue;
        u8cs base = {};
        PATHu8sBase(base, u8bDataC(it.path));
        u64 sq = pup_parse_seqno(base, ext);
        //  DOG-027: anything that is not a `<10-RON64><ext>` run (a pre-ron64
        //  name, the memtable, a stray file) is skipped, never unlinked.
        if (sq == 0) continue;
        if ($len(u8bIdleC(namebuf)) < u8csLen(base) + 1) continue;
        u64 name_off = (u64)u8bDataLen(namebuf);
        u8bFeed(namebuf, base);
        u8bFeed1(namebuf, 0);
        kv64 kv = {.key = sq, .val = name_off};
        if (kv64bPush(seqnos, &kv) != OK) break;
    }
    //  Close the iterator UNCONDITIONALLY: a non-END FILENext error
    //  (e.g. PATHNOROOM on an over-long path) leaves `__ != END`, so a
    //  bare `seen(END)` would `fail()` and skip the close — leaking the
    //  opendir() DIR handle.  Close first, then propagate the error.
    FILEIterClose(&it);
    if (__ != END) { kv64bFree(seqnos); fail(__); }
    __ = OK;

    a_dup(kv64, ks, kv64bData(seqnos));
    kv64sSort(ks);

    //  Pup layer is not idempotent: every file in `dir` becomes a new
    //  entry in `pups`.  Avoiding double-scans of the same dir is the
    //  caller's job (e.g. skip a prefix already loaded from a shared
    //  parent directory).
    $for(kv64, kp, ks) {
        if (dedup && pup_dicted(pups, kp->key)) {
            //  A pre-PUPS-005 ladder merge renamed onto its youngest source's
            //  name, so a dicted key may hold another writer's merged run.
            u8cp nm1 = u8bDataHead(namebuf) + kp->val;
            u8cs fn1 = {nm1, nm1 + strlen((char const *)nm1)};
            a_path(fp1, dir, fn1);
            pup_remap(pups, $path(fp1), kp->key);
            continue;
        }
        u8cp nm0 = u8bDataHead(namebuf) + kp->val;
        u8cs fn  = {nm0, nm0 + strlen((char const *)nm0)};
        a_path(fpath, dir, fn);
        u8bp buf = NULL;
        if (FILEMapRO(&buf, $path(fpath)) != OK) continue;
        int fd = FILEBookedFD(buf);
        if (fd < 0) { FILEUnMap(buf); continue; }
        kv64 kv = {.key = kp->key, .val = (u64)fd};
        //  On a `pups` overflow the just-mapped file is NOT yet recorded
        //  in `pups`, so PUPClose can never reclaim it — release the
        //  fd+mmap here before propagating, and free `seqnos` too.
        try(kv64bPush, pups, &kv);
        if (__ != OK) { FILEUnMap(buf); kv64bFree(seqnos); fail(__); }
    }

    kv64bFree(seqnos);
    //  DOG-027: the runs landed past a live memtable entry — put it back at
    //  the tail, where PUPCount and pup_mem expect it.
    pup_tail(pups);
    done;
}

ok64 PUPOpenAll(kv64b pups, path8sc dir, u8csc ext) {
    return pup_scan(pups, dir, ext, NO);
}

// DOG-046: map in the runs another writer sealed into `dir` since this handle
// opened.  Runs BEFORE every seal and every ladder, so the key pick and the
// 1/8 policy both weigh the REAL stack, not this handle's stale view.
static ok64 pup_refresh(kv64b pups, path8s dir, u8cs ext) {
    sane(pups != NULL && $ok(dir) && $ok(ext));
    call(pup_scan, pups, dir, ext, YES);
    //  A foreign run lands past what was dicted, so re-sort DATA oldest-first;
    //  the ALL-ONES memtable key sorts itself back to the tail.
    a_dup(kv64, all, kv64bData(pups));
    kv64sSort(all);
    done;
}

ok64 PUPOpenAside(kv64b pups, path8sc dir, u8csc ext) {
    sane(pups != NULL && u8csOK(dir) && u8csOK(ext));
    //  Collapse the current DATA into PAST: the previously-active leaf
    //  becomes part of the read-only context, leaving DATA empty and
    //  ready for the next branch's entries.  kv64bUsedAll advances the
    //  PAST/DATA boundary to the DATA/IDLE boundary (no buffer-poke).
    if (kv64bDataLen(pups) > 0)
        kv64bUsedAll(pups);
    return PUPOpenAll(pups, dir, ext);
}

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

    int wfd = -1;
    call(FILECreate, &wfd, $path(tmppath));
    if (wfd >= 0) {
        callsafe(FILEFeedAll(wfd, bytes), close(wfd));
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
static ok64 pup_book(kv64b pups, path8s dir, u8cs ext, u64 pup_key) {
    sane(pups != NULL && $ok(dir) && $ok(ext));
    a_path(idxpath);
    call(pup_path, idxpath, dir, pup_key, ext);
    u8bp buf = NULL;
    call(FILEMapRO, &buf, $path(idxpath));
    int fd = FILEBookedFD(buf);
    if (fd < 0) { FILEUnMap(buf); return PUPFAIL; }
    kv64 kv = {.key = pup_key, .val = (u64)fd};
    //  On a `pups` overflow the file just mapped above is not yet
    //  recorded, so PUPClose can never reclaim it — release the
    //  fd+mmap before propagating the push error.
    try(kv64bPush, pups, &kv);
    if (__ != OK) { FILEUnMap(buf); fail(__); }
    //  DOG-027: a push lands past a live memtable entry — put that entry back
    //  at the tail (a no-op for a stack with no memtable).
    pup_tail(pups);
    done;
}

static ok64 pup_create_at(kv64b pups, path8s dir, u8cs ext, u8cs bytes,
                          u64 pup_key) {
    sane(pups != NULL && $ok(dir) && $ok(ext) && pup_key > 0);

    //  DOG-027: refuse a taken key — DATA-side (ThinTail first) or a file on
    //  its name.  The key is the CALLER's, so dog refuses; it cannot bump it.
    {
        b8 taken = NO;
        call(pup_key_taken, &taken, pups, dir, ext, pup_key);
        if (taken) return PUPFAIL;
    }
    call(pup_write, dir, ext, bytes, pup_key, NO);
    call(pup_book, pups, dir, ext, pup_key);
    done;
}

ok64 PUPCreateAt(kv64b pups, path8s dir, u8cs ext, u8cs bytes,
                    u64 pup_key) {
    sane(pups != NULL && $ok(dir) && $ok(ext) && pup_key > 0);
    int lk = -1;
    call(PUPLock, &lk, dir);
    try(pup_create_at, pups, dir, ext, bytes, pup_key);
    PUPUnlock(&lk);
    nedo fail(__);
    done;
}

ok64 PUPCreate(kv64b pups, path8s dir, u8cs ext, u8cs bytes) {
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
    try(pup_free_key, &new_seqno, pups, dir, ext);
    then try(pup_create_at, pups, dir, ext, bytes, new_seqno);
    PUPUnlock(&lk);
    nedo fail(__);
    done;
}

// PUPS-004: unmap the i-th run and close the dict gap; `rm` NO leaves the file.
static ok64 pup_drop(kv64b pups, path8s dir, u8cs ext, u32 i, b8 rm) {
    sane(pups != NULL && $ok(dir) && $ok(ext));
    //  DOG-027: index over RUNS, not entries — the memtable's key would unlink
    //  a file named `~0`; the memmove below keeps it last.
    u32 n = PUPCount(pups), all = (u32)kv64bDataLen(pups);
    if (i >= n) return PUPFAIL;
    kv64 *base = (kv64 *)kv64bDataHead(pups);
    u8bp slot = FILE_WANT_BUFS[base[i].val];
    if (slot && slot[0]) FILEUnMap(slot);
    if (rm) {
        a_path(ulpath);
        call(pup_path, ulpath, dir, base[i].key, ext);
        unlink((char *)u8bDataHead(ulpath));
    }
    memmove(base + i, base + i + 1, (size_t)(all - i - 1) * sizeof(kv64));
    //  Trim DATA end back by one record (kv64bShed rolls the DATA/IDLE
    //  boundary back into DATA — no buffer-poke).
    kv64bShed(pups, 1);
    done;
}

ok64 PUPDropAt(kv64b pups, path8s dir, u8cs ext, u32 i) {
    return pup_drop(pups, dir, ext, i, YES);
}

ok64 PUPThinTail(kv64b pups, path8s dir, u8cs ext, u32 m) {
    sane(pups != NULL && $ok(dir) && $ok(ext));
    u32 n = PUPCount(pups);
    if (m > n) m = n;
    //  DOG-027: the youngest run is the last one PUPCount admits.
    while (m-- > 0) call(PUPDropAt, pups, dir, ext, PUPCount(pups) - 1);
    done;
}

ok64 PUPClose(kv64b pups) {
    sane(pups != NULL);
    if (BNULL(pups)) done;
    kv64 *base = (kv64 *)kv64bDataHead(pups);
    u32 n = (u32)kv64bDataLen(pups);
    for (u32 i = 0; i < n; i++) {
        //  DOG-027: every entry is an fd, the memtable's included.
        u64 fd = base[i].val;
        u8bp slot = FILE_WANT_BUFS[fd];
        if (slot && slot[0]) FILEUnMap(slot);
    }
    kv64bFree(pups);
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
static void pup_slot(u8csp data, u8csp toc, kv64b pups, u32 i, u32 row) {
    data[0] = data[1] = NULL;
    toc[0] = toc[1] = NULL;
    if (i >= PUPCount(pups)) return;
    kv64 const *base = (kv64 const *)kv64bDataHead(pups);
    u8bp slot = FILE_WANT_BUFS[base[i].val];
    if (!slot || !slot[0]) return;
    u8cs raw = {slot[0], slot[2]};
    pup_toc_split(data, toc, raw, row);
}

void PUPDataRow(u8csp out, kv64b pups, u32 i, u32 row) {
    u8cs toc = {};
    pup_slot(out, toc, pups, i, row);
}

void PUPToc(u8csp out, kv64b pups, u32 i, u32 row) {
    u8cs data = {};
    pup_slot(data, out, pups, i, row);
}

void PUPData(u8csp out, kv64b pups, u32 i) {
    PUPDataRow(out, pups, i, 0);
}

void PUPDataAllRow(u8csp out, kv64b pups, u32 i, u32 row) {
    out[0] = NULL; out[1] = NULL;
    kv64s all = {};
    kv64PastDataS(pups, all);
    //  DOG-027: the memtable entry sits at the tail of PastData too.
    size_t n = (size_t)PUPCountAll(pups);
    if ((size_t)i >= n) return;
    u8bp slot = FILE_WANT_BUFS[all[0][i].val];
    if (!slot || !slot[0]) return;
    u8cs raw = {slot[0], slot[2]}, toc = {};
    pup_toc_split(out, toc, raw, row);
}

void PUPDataAll(u8csp out, kv64b pups, u32 i) {
    PUPDataAllRow(out, pups, i, 0);
}

ok64 PUPAllDataRow(u8csb out, kv64b pups, u32 row) {
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

ok64 PUPAllData(u8csb out, kv64b pups) {
    return PUPAllDataRow(out, pups, 0);
}

// =============================================================
// --- DOG-027: the memtable — a sized pup the puts land in ---
// =============================================================

// DOG-027: the live memtable is the dict's trailing ALL-ONES entry, if any.
static u8bp pup_mem(kv64b pups) {
    u32 n = (u32)kv64bDataLen(pups);
    if (n == 0) return NULL;
    kv64 const *last = (kv64 const *)kv64bDataHead(pups) + (n - 1);
    if (last->key != PUP_MEM_KEY) return NULL;
    u8bp slot = FILE_WANT_BUFS[last->val];
    if (!slot || !slot[0]) return NULL;
    return slot;
}

// DOG-046: this handle's memtable owner id — the pid in the top 22 bits, the
// dict's own address below it, so two stacks in ONE process differ too.  Purely
// derived: the same handle recomposes it at open and at seal.
static u64 pup_owner(kv64b pups) {
    u64 self = (u64)(uintptr_t)pups;
    return (((u64)getpid() & 0x3FFFFFULL) << 38) |
           ((self >> 4) & 0x3FFFFFFFFFULL);
}

// DOG-046: compose `<dir>/~<10-RON64-owner><ext>` — the memtable is PER
// HANDLE, so two rw writers on one dir never share an inode and never O_TRUNC
// each other's rows away.  It still ENDS in `<ext>` (bee's sweep unlinks what
// does not), yet `~` is outside RON64 and the seqno is 11 wide, so the
// length-strict `pup_parse_seqno` can never read it as a run.
static ok64 pup_mem_path(path8b out, path8s dir, u8cs ext, kv64b pups) {
    sane(u8bOK(out));
    call(PATHu8bDup, out, dir);
    a_pad(u8, name, PUP_SEQNO_W + 32);
    call(u8bFeed1, name, '~');
    call(RONu8sFeedPad, u8bIdle(name), (ok64)pup_owner(pups),
         PUP_SEQNO_W);
    call(u8bFeed, name, ext);
    call(PATHu8bPush, out, u8bDataC(name));
    done;
}

// DOG-027: create + RW-map a fresh memtable file (truncating this handle's own
// crash leftover), dict it under the ALL-ONES key so it sorts LAST.
// DOG-032: its size is `pages` WHOLE OS pages (0 = one, the default) — a
// sub-page memtable would pay a full page per flush anyway.
static ok64 pup_mem_open(u8bp *mem, kv64b pups, path8s dir, u8cs ext,
                             u32 pages) {
    sane(mem != NULL && pups != NULL && $ok(dir) && $ok(ext));
    if (pup_mem(pups) != NULL) return PUPFAIL;
    a_path(mpath);
    call(pup_mem_path, mpath, dir, ext, pups);
    u8bp buf = NULL;
    size_t size = (size_t)(pages ? pages : PUP_MEM_PAGES) * FILESysPage();
    call(FILEMapCreate, &buf, $path(mpath), size);
    int fd = FILEBookedFD(buf);
    if (fd < 0) { FILEUnMap(buf); return PUPFAIL; }
    kv64 kv = {.key = PUP_MEM_KEY, .val = (u64)fd};
    try(kv64bPush, pups, &kv);
    if (__ != OK) { FILEUnMap(buf); fail(__); }
    *mem = buf;
    done;
}

// DOG-027: INTERNAL sync — the typed hook runs only on non-empty DATA;
// collapse on `force` or DATA past PUP_COLLAPSE, never on a plain read.
static ok64 pup_sync(u8bp mem, pupsync sync, b8 force) {
    sane(u8bOK(mem) && sync != NULL);
    if (u8bDataLen(mem) == 0) done;
    b8 collapse = force || u8bDataLen(mem) > PUP_COLLAPSE;
    call(sync, mem, collapse);
    done;
}

// DOG-035: the ToC rides PAST the trimmed mapping, appended through the fd —
// the write path stays one pass and the rename stays atomic.
static ok64 pup_toc_append(int fd, u8csc block) {
    sane(FILEok(fd));
    if (u8csEmpty(block)) done;
    test(lseek(fd, 0, SEEK_END) >= 0, PUPFAIL);
    call(FILEFeedAll, fd, block);
    done;
}

// DOG-027: seal the collapsed memtable as a run — trim, optional fsync,
// atomic rename to `<10-RON64><ext>`, re-key the dict entry in place.
// DOG-035: `row` (the record width, 0 = none) decides whether a ToC block
// follows the data.
static ok64 pup_seal_run(kv64b pups, path8s dir, u8cs ext, u32 row,
                         b8 durable) {
    sane(pups != NULL && $ok(dir) && $ok(ext));
    //  DOG-046: pick the fresh key against the runs OTHER writers sealed
    //  meanwhile, and leave the dict a true oldest-first stack for the ladder.
    call(pup_refresh, pups, dir, ext);
    u8bp mem = pup_mem(pups);
    if (!mem) return PUPFAIL;
    call(FILETrimMap, mem);
    int fd = FILEBookedFD(mem);
    u8cs body = {mem[0], mem[2]};
    size_t cells = pup_toc_cells((size_t)u8csLen(body), row);
    if (cells > 0 && fd >= 0) {
        a_carve(u8, toc, cells * row + PUP_TOC_TAIL_W);
        call(pup_toc_feed, u8bIdle(toc), body, row);
        call(pup_toc_append, fd, u8bDataC(toc));
    }
    if (durable) {
        call(FILESync, &fd);
    }
    u64 key = 0;
    call(pup_free_key, &key, pups, dir, ext);
    a_path(runpath);
    call(pup_path, runpath, dir, key, ext);
    a_path(mpath);
    call(pup_mem_path, mpath, dir, ext, pups);
    call(FILERename, $path(mpath), $path(runpath));
    kv64 *base = (kv64 *)kv64bDataHead(pups);
    base[kv64bDataLen(pups) - 1].key = key;
    pup_tail(pups);  //  a real run now — back under the memtable
    done;
}

//  The seal under the stack's write lock: the key pick and the rename onto it
//  are one critical section.
static ok64 pup_mem_seal(kv64b pups, path8s dir, u8cs ext, u32 row,
                         b8 durable) {
    sane(pups != NULL && $ok(dir) && $ok(ext));
    int lk = -1;
    call(PUPLock, &lk, dir);
    try(pup_seal_run, pups, dir, ext, row, durable);
    PUPUnlock(&lk);
    nedo fail(__);
    done;
}

// DOG-027: merge the youngest `m` runs into one fresh run — the caller's
// `merge` does the typed merge, Pup owns the tmp + rename seal and the
// source unlinks.
static ok64 pup_collapse(kv64b pups, path8s dir, u8cs ext, pupmerge merge,
                         u32 row, u8css stack, size_t m) {
    sane(pups != NULL && $ok(dir) && $ok(ext) && merge != NULL && m > 1);
    size_t n = (size_t)$len(stack), total = 0;
    for (size_t i = n - m; i < n; i++) total += (size_t)u8csLen(stack[0][i]);
    //  Heap, not BASS: a compaction merge is unbounded by the memtable.
    //  DOG-035: room for the merged run's own ToC block + trailer on top —
    //  one cell per page the merge can fill, and dedup only shortens it.
    size_t page = PUPTocRows(row) * row;
    Bu8 scr = {};
    call(u8bAllocate, scr,
         total + (page ? (total / page + 2) * row : 0) +
             PUP_TOC_TAIL_W);
    u8s into = {u8bIdleHead(scr), u8bTerm(scr)};
    u8css tail = {stack[0] + (n - m), stack[0] + n};
    //  PUPS-001: the merge spans the whole stack only when it takes every run
    //  and nothing older is inherited; only then may a tombstone be dropped.
    b8 bottom = m == n && PUPCountAll(pups) == PUPCount(pups);
    try(merge, into, tail, bottom);
    nedo { u8bFree(scr); fail(__); }
    a_dup(u8c, merged, ((u8cs){u8bIdleHead(scr), into[0]}));
    //  PUPS-005: the sources go OLDEST first.  A crash part way leaves the
    //  merged run (if any) plus a YOUNGEST suffix of the sources: every
    //  tombstone outlives the rows it shadows, so the stack reads as before.
    u32 oldest = PUPCount(pups) - (u32)m;
    //  PUPS-001: nothing survived (every row was dead): no empty run, which
    //  could not be mapped anyway — the sources just go.
    if (u8csEmpty(merged)) {
        u8bFree(scr);
        for (size_t k = 0; k < m; k++)
            call(pup_drop, pups, dir, ext, oldest, YES);
        pup_tail(pups);
        done;
    }
    //  DOG-035: a compaction output is ToC'd exactly like a seal's — same
    //  block, appended into the scratch before the run is booked.
    try(pup_toc_feed, into, merged, row);
    nedo { u8bFree(scr); fail(__); }
    a_dup(u8c, body, ((u8cs){u8bIdleHead(scr), into[0]}));
    //  PUPS-005: a run name is never reused — renaming onto the youngest
    //  source (PUPS-004) let a crash keep older sources whose tombstones the
    //  bottom merge had dropped.  The merged run takes the next free key past
    //  the youngest source (sequential; the ladder holds the write lock), is
    //  made durable, booked, and only then are the sources unlinked.
    u64 key = PUPSeqno(pups, PUPCount(pups) - 1) + 1;
    for (b8 taken = YES; __ == OK; key++) {
        __ = pup_key_taken(&taken, pups, dir, ext, key);
        if (__ == OK && !taken) break;
    }
    then try(pup_write, dir, ext, body, key, YES);
    then try(pup_book, pups, dir, ext, key);
    for (size_t k = 0; k < m && __ == OK; k++)
        __ = pup_drop(pups, dir, ext, oldest, YES);
    u8bFree(scr);
    nedo fail(__);
    pup_tail(pups);
    done;
}

static ok64 pup_ladder(kv64b pups, path8s dir, u8cs ext, pupmerge merge,
                       u32 row) {
    sane(pups != NULL && $ok(dir) && $ok(ext));
    //  DOG-046: a ladder over a stale view collapses the wrong tail — map in
    //  what other writers sealed, then weigh.  Only merged runs are unlinked.
    call(pup_refresh, pups, dir, ext);
    for (;;) {
        Bu8cs srcs = {};
        call(u8csbAllocate, srcs, HIT_MAX_RUNS);
        //  DOG-035: clipped lengths — a ToC block is not run bytes, so the
        //  ladder must not weigh it and the merge must never see a cell.
        try(PUPAllDataRow, srcs, pups, row);
        nedo { u8csbFree(srcs); fail(__); }
        u8cs *sl = u8csbDataHead(srcs);
        u8css stack = {sl, sl + u8csbDataLen(srcs)};
        //  The ladder reads run LENGTHS only — byte lengths here, element
        //  counts in a HIT stack; HIT.h stays the one home of the 1/8 policy.
        a_pad(u64, lens, HIT_MAX_RUNS);
        $for(u8cs, r, stack) u64bFeed1(lens, (u64)u8csLen(*r));
        size_t m = HITLadderOverRuns(u64bDataC(lens));
        if (m < 2) { u8csbFree(srcs); done; }
        try(pup_collapse, pups, dir, ext, merge, row, stack, m);
        u8csbFree(srcs);
        nedo fail(__);
    }
}

ok64 PUPLadder(kv64b pups, path8s dir, u8cs ext, pupmerge merge, u32 row) {
    sane(pups != NULL && $ok(dir) && $ok(ext));
    if (merge == NULL) done;
    //  One merger per stack: the lock covers the rescan and every collapse.
    int lk = -1;
    call(PUPLock, &lk, dir);
    try(pup_ladder, pups, dir, ext, merge, row);
    PUPUnlock(&lk);
    nedo fail(__);
    done;
}

ok64 PUPPutMem(kv64b pups, path8s dir, u8cs ext, u8csc rec, pupsync sync,
               pupmerge merge, u32 row, u32 mem_pages) {
    sane(pups != NULL && $ok(dir) && $ok(ext) && u8csOK(rec));
    if (sync == NULL) return PUPFAIL;
    u8bp mem = pup_mem(pups);
    if (!mem) call(pup_mem_open, &mem, pups, dir, ext, mem_pages);
    if (u8bIdleLen(mem) < (size_t)u8csLen(rec)) {
        //  DOG-027: full buffer — the collapse may dedup room free; if not,
        //  flush into a run, run the ladder, start a fresh memtable.
        call(pup_sync, mem, sync, YES);
        if (u8bIdleLen(mem) < (size_t)u8csLen(rec)) {
            call(pup_mem_seal, pups, dir, ext, row, NO);
            call(PUPLadder, pups, dir, ext, merge, row);
            call(pup_mem_open, &mem, pups, dir, ext, mem_pages);
        }
    }
    call(u8bFeed, mem, rec);
    done;
}

ok64 PUPPut(kv64b pups, path8s dir, u8cs ext, u8csc rec, pupsync sync,
            pupmerge merge, u32 row) {
    return PUPPutMem(pups, dir, ext, rec, sync, merge, row, PUP_MEM_PAGES);
}

ok64 PUPCommitAs(kv64b pups, path8s dir, u8cs ext, pupsync sync,
                 pupmerge merge, u32 row, b8 durable) {
    sane(pups != NULL && $ok(dir) && $ok(ext));
    if (sync == NULL) return PUPFAIL;
    u8bp mem = pup_mem(pups);
    if (!mem || u8bBusyLen(mem) == 0) done;
    call(pup_sync, mem, sync, YES);
    call(pup_mem_seal, pups, dir, ext, row, durable);
    call(PUPLadder, pups, dir, ext, merge, row);
    done;
}

ok64 PUPCommit(kv64b pups, path8s dir, u8cs ext, pupsync sync,
               pupmerge merge, u32 row) {
    return PUPCommitAs(pups, dir, ext, sync, merge, row, YES);
}

ok64 PUPAllRunsToc(u8csb out, u8csb tocs, kv64b pups, pupsync sync,
                   u32 row) {
    sane(out && pups != NULL);
    if (sync == NULL) return PUPFAIL;
    u8csbReset(out);
    if (tocs) u8csbReset(tocs);
    u8cs none = {};
    u8bp mem = pup_mem(pups);
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
    if (!mem || u8bBusyLen(mem) == 0) done;
    call(pup_sync, mem, sync, NO);
    if (u8bPastLen(mem) > 0) {
        a_dup(u8c, past, u8bPastC(mem));
        call(u8csbFeed1, out, past);
        if (tocs) call(u8csbFeed1, tocs, none);
    }
    if (u8bDataLen(mem) > 0) {
        a_dup(u8c, data, u8bDataC(mem));
        call(u8csbFeed1, out, data);
        if (tocs) call(u8csbFeed1, tocs, none);
    }
    done;
}

ok64 PUPAllRuns(u8csb out, kv64b pups, pupsync sync, u32 row) {
    return PUPAllRunsToc(out, NULL, pups, sync, row);
}

