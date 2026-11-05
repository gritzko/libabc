#include "PUPS.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "B.h"
#include "FILE.h"
#include "PRO.h"

// --- Puppies: stack of <seqno>.<ext> files ---

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

//  Write lock: ONE flock per DIRECTORY, `<dir>/.lock` (PUPS.md "Write lock").
//  PUPS-26B034NDyT: reentrant per process; a slot is one {dev, ino, fd, depth}.
#define PUP_LQ 4
static Bu64 pup_held = {};

ok64 PUPLock(int *slot, path8s dir) {
    sane(slot != NULL && $ok(dir));
    *slot = -1;
    if (pup_held[0] == NULL) call(u64bMap, pup_held, FILE_MAX_OPEN * PUP_LQ);
    a_path(lp);
    call(PATHu8bDup, lp, dir);
    struct stat st;
    if (stat((char const *)u8bDataHead(lp), &st) != 0) return PUPSTAT;
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
    if (free_slot < 0 && u64bIdleLen(pup_held) < PUP_LQ) return PUPLCKFULL;
    a_cstr(name, ".lock");
    call(PATHu8bPush, lp, name);
    int f = open((char const *)u8bDataHead(lp), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (f < 0) return PUPNOOPEN;
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

// PUPS-26A309qbG9: is `key` on the caller's list of merged-away runs?
static b8 pup_has(u64csc keys, u64 key) {
    if (keys == NULL) return NO;
    $for(u64c, k, keys) if (*k == key) return YES;
    return NO;
}

ok64 PUPScan(u64b keys, path8sc dir, u8csc ext, u64 ceil, u64csc dead) {
    sane(u64bOK(keys) && u8csOK(dir) && u8csOK(ext));
    u64 *from = u64bIdleHead(keys);
    a_path(dpat, dir);
    fileit it = {};
    //  CODE-26A23CSStk: only an ABSENT dir is an empty stack; one that exists
    //  but cannot be listed (EMFILE, EACCES) is an unreadable one.
    ok64 o = FILEIterOpen(&it, dpat);
    if (o == FILENONE) done;
    if (o != OK) fail(o);
    scan(FILENext, &it) {
        if (it.type != DT_REG) continue;
        u8cs base = {};
        PATHu8sBase(base, u8bDataC(it.path));
        u64 sq = pup_parse_seqno(base, ext);
        //  DOG-027: a tmp, a pre-ron64 name, a stray file: skipped, kept
        if (sq == 0 || sq > ceil || pup_has(dead, sq)) continue;
        if (u64bFeed1(keys, sq) != OK) {
            __ = PUPNOROOM;
            break;
        }
    }
    //  Close UNCONDITIONALLY: a non-END FILENext error (e.g. PATHNOROOM) would
    //  make `seen(END)` fail() past the close and leak the DIR handle.
    FILEIterClose(&it);
    if (__ != END) fail(__);
    __ = OK;
    u64s fresh = {from, u64bIdleHead(keys)};
    u64sSort(fresh);
    done;
}

ok64 PUPMap(u8csp run, path8s dir, u8cs ext, u64 key) {
    sane(run != NULL && $ok(dir) && $ok(ext));
    a_path(path);
    call(pup_path, path, dir, key, ext);
    int fd = FILE_CLOSED;
    ok64 op = FILEOpen(&fd, $path(path), O_RDONLY);
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
    run[0] = (u8c *)map;
    run[1] = (u8c *)map + st.st_size;
    done;
}

void PUPUnMap(u8csc run) {
    if (run[0] != NULL) munmap((void *)run[0], (size_t)u8csLen(run));
}

ok64 PUPKeyTaken(b8 *out, u64csc keys, path8s dir, u8cs ext, u64 key) {
    sane(out != NULL && $ok(dir) && $ok(ext));
    *out = YES;
    if (pup_has(keys, key)) done;
    a_path(probe);
    call(pup_path, probe, dir, key, ext);
    ok64 e = FILEExists($path(probe));
    if (e == OK) done;            //  a file is already on the name
    if (e != FILENONE) return e;  //  perms/IO — not ours to paper over
    *out = NO;
    done;
}

ok64 PUPNextKey(u64 *out, u64csc keys, path8s dir, u8cs ext, u64 floor) {
    sane(out != NULL && $ok(dir) && $ok(ext));
    u64 key = (u64)RONNow();
    if (key < floor) key = floor;
    if (!$empty(keys) && *$last(keys) >= key) key = *$last(keys) + 1;
    if (key == 0) key = 1;
    for (b8 taken = YES;; key++) {
        call(PUPKeyTaken, &taken, keys, dir, ext, key);
        if (!taken) break;
    }
    *out = key;
    done;
}

//  `<dir>/<key><ext>.tmp`, the name a run is built under before its link.
static ok64 pup_tmp_path(path8b out, path8s dir, u64 key, u8cs ext) {
    sane(u8bOK(out));
    call(pup_path, out, dir, key, ext);
    a_cstr(tmpsuf, ".tmp");
    call(u8bFeed, out, tmpsuf);
    call(PATHu8bTerm, out);
    done;
}

ok64 PUPTmpMap(u8bp *tmp, path8s dir, u8cs ext, u64 key, size_t bound) {
    sane(tmp != NULL && $ok(dir) && $ok(ext));
    a_path(path);
    call(pup_tmp_path, path, dir, key, ext);
    //  PUPS-26B04KwMzK: O_EXCL, so two writers never share a tmp
    int fd = open((char const *)u8bDataHead(path),
                  O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0644);
    if (fd < 0) return errno == EEXIST ? PUPTAKEN : FILEErr(FILENOOPEN);
    try(FILEResize, &fd, bound);
    nedo close(fd);
    then try(FILEMapFD, tmp, &fd, PROT_READ | PROT_WRITE);
    nedo {
        FILEUnLink($path(path));
        fail(__);
    }
    done;
}

ok64 PUPTmpDrop(u8bp tmp, path8s dir, u8cs ext, u64 key) {
    sane(tmp != NULL && $ok(dir) && $ok(ext));
    a_path(path);
    call(pup_tmp_path, path, dir, key, ext);
    FILEUnMap(tmp);
    call(FILEUnLink, $path(path));
    done;
}

// PUPS-26A24FhuO9: an empty run on disk, an empty ToC block's trailer.
static u8c const PUP_NIL_RUN[PUP_TOC_TAIL_W] = {0, 0, 0, 0, 0, 'T', 'C'};

//  PUPS-004: the dir fsync that makes a link durable.
static ok64 pup_dir_sync(path8s dir) {
    sane($ok(dir));
    int dfd = -1;
    call(FILEOpenDir, &dfd, dir);
    callsafe(FILESync(&dfd), close(dfd));
    close(dfd);
    done;
}

ok64 PUPTmpSeal(u8csp run, u8bp tmp, path8s dir, u8cs ext, u64 key,
                b8 durable) {
    sane(run != NULL && u8bOK(tmp) && $ok(dir) && $ok(ext));
    a_path(tpath);
    call(pup_tmp_path, tpath, dir, key, ext);
    a_path(rpath);
    call(pup_path, rpath, dir, key, ext);
    //  PUPS-26A24FhuO9: a 0-byte file stays what a crashed create leaves
    u8cs nil = {PUP_NIL_RUN, PUP_NIL_RUN + PUP_TOC_TAIL_W};
    if (u8bDataLen(tmp) == 0) try(u8bFeed, tmp, nil);
    u8 *base = tmp[0];
    size_t full = (size_t)(tmp[3] - tmp[0]);
    int fd = FILEBookedFD(tmp);
    then try(FILETrimMap, tmp);
    then if (durable) try(FILESync, &fd);
    //  the trim shortened the record, not the mapping: release its tail
    size_t pg = FILESysPage();
    size_t kept = ((size_t)(tmp[3] - tmp[0]) + pg - 1) / pg * pg;
    if (__ == OK && kept < full) munmap((void *)(base + kept), full - kept);
    FILEUnMap(tmp);
    if (__ != OK) FILEUnLink($path(tpath));
    nedo fail(__);
    //  PUPS-26B04KwMzK: link, never rename: a taken name is not clobbered
    try(FILEHardLink, $path(rpath), $path(tpath));
    FILEUnLink($path(tpath));
    if (__ == FILEEXIST) __ = PUPTAKEN;
    nedo fail(__);
    if (durable) call(pup_dir_sync, dir);
    call(PUPMap, run, dir, ext, key);
    done;
}

// --- DOG-035: the run ToC — [data][toc][len:5]["TC"] ---

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

ok64 PUPTocFeed(u8bp into, u8csc data, u32 row) {
    sane(u8bOK(into));
    size_t len = (size_t)u8csLen(data);
    size_t cells = pup_toc_cells(len, row);
    if (cells == 0) done;
    size_t page = PUPTocRows(row) * row;
    for (size_t i = 0; i < cells; i++) {
        a_part(u8c, cell, data, i * page, row);
        call(u8bFeed, into, cell);
    }
    size_t bytes = cells * row;
    for (u32 k = 0; k < PUP_TOC_LEN_W; k++)
        call(u8bFeed1, into, (u8)(bytes >> (8 * k)));
    a_cstr(magic, "TC");
    call(u8bFeed, into, magic);
    done;
}

void PUPTocSplit(u8csp data, u8csp toc, u8csc raw, u32 row) {
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
    //  PUPS-26B034zlKV: exactly the geometry PUPTocFeed writes, else plain
    size_t cells = pup_toc_cells(size - PUP_TOC_TAIL_W - bytes, row);
    if (cells == 0 || bytes != cells * row) return;
    data[1] = tail[0] - bytes;
    toc[0] = data[1];
    toc[1] = tail[0];
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

