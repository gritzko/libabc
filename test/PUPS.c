//  PUPS-26B045Ffpb: the pup house over kv64 — fd leaks, the memtable, commit,
//  the ladder and its crash points, the lock, many writers, the ToC,
//  Get and Range against the merged Read view.
#include "PUPS.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "FILE.h"
#include "PRO.h"
#include "TEST.h"

#include "TESTBE.h"

// DOG-027: kv64Z compares keys only, so keep-last dedup and younger-wins ties
// are observable.  PUPS-001: a row whose val is all ones is a tombstone.
#define PUP_TOMB (~0ULL)
#define PUPx_DEAD(r) ((r)->val == PUP_TOMB)
#define X(M, name) M##kv64##name
#include "PUPx.h"
#undef X

//  PUPS-26B045Ffpb: a merge-hook lane, vals add up (ABC_X_MERGE).
typedef struct cnt64 {
    u64 key;
    u64 val;
} cnt64;
fun b8 cnt64Z(cnt64 const *a, cnt64 const *b) { return a->key < b->key; }
fun b8 cnt64KeyEq(cnt64 const *a, cnt64 const *b) { return a->key == b->key; }
fun void cnt64Merge(cnt64 *acc, cnt64 const *nxt) { acc->val += nxt->val; }
con cnt64 cnt64Max = {u64max, u64max};
#define X(M, name) M##cnt64##name
#include "Bx.h"
#include "QSORTx.h"
#define ABC_X_MERGE
#include "PUPx.h"
#undef X

#define T_DIR(ext_lit)                                      \
    char tmp[256];                                          \
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);             \
    a_cstr(ext, ext_lit);                                   \
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}))
#define T_OPEN(h) PUPkv64Open(h, h##_keys, $path(dir), ext, u64max)
#define T_CLOSE(h) PUPkv64Close(h, h##_keys)
#define T_PAGE (FILESysPage() / sizeof(kv64))

//  PUPS-26B04KwMzK: Commit no longer ladders; the tests want both.
static ok64 t_commit(kv64csb h, u64b keys, path8s dir, u8cs ext, b8 durable,
                     u64 *floor) {
    sane(1);
    call(PUPkv64Commit, h, keys, dir, ext, durable, floor);
    call(PUPkv64Ladder, h, keys, dir, ext, NULL, floor);
    done;
}

//  One (key, val) row; a full memtable is committed, then the put retried
//  (D-1: the owner seals).
static ok64 t_put(kv64csb h, u64b keys, path8s dir, u8cs ext, u64 key,
                  u64 val) {
    sane(1);
    kv64 row = {key, val};
    ok64 o = PUPkv64Put(h, &row);
    if (o == NOROOM) {
        call(t_commit, h, keys, dir, ext, NO, NULL);
        o = PUPkv64Put(h, &row);
    }
    if (o != OK) fail(o);
    done;
}

//  Rows [from, from+n) as a run under `key`, val `val` (0: key*10).
static ok64 t_run(kv64csb h, u64b keys, path8s dir, u8cs ext, u64 key,
                  u64 from, u64 n, u64 val) {
    sane(n <= 64);
    kv64 rows[64];
    for (u64 i = 0; i < n; i++)
        rows[i] = (kv64){from + i, val ? val : (from + i) * 10};
    kv64cs rs = {rows, rows + n};
    call(PUPkv64Create, h, keys, dir, ext, rs, key, NO, NULL);
    done;
}

//  The read view (PUP Runs) merged into `out`, its run count in *nsrc.
static ok64 t_query(kv64s out, u32 *nsrc, kv64csb h) {
    sane(out != NULL && nsrc != NULL);
    a_pad(kv64cs, srcs, ABC_HIT_MAX_RUNS);
    call(PUPkv64Runs, srcs, h);
    *nsrc = (u32)$len(srcs_data);
    call(HITkv64Merge, out, srcs_data);
    done;
}

//  The rows of PAST run `i`, ToC clipped.
static void t_rows(kv64cs rows, kv64csb h, size_t i) {
    kv64cs toc = {};
    PUPkv64Split(rows, toc, h[0][i]);
}

static int dog_open_fd_count(void) {
    long max_fd = sysconf(_SC_OPEN_MAX);
    if (max_fd <= 0 || max_fd > 4096) max_fd = 4096;
    int n = 0;
    for (int fd = 0; fd < (int)max_fd; fd++)
        if (fcntl(fd, F_GETFD) != -1) n++;
    return n;
}

// --- MEM-043: error-path leaks, counted as fds -------------------------

//  A second open of 40 runs overflows the 62-cell house: PUPNOROOM, and the
//  run it could not house was never mapped (no fd, no mmap kept).
static ok64 PUPTestOverflowLeak(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".test.idx");
    enum { NPUP = 40, ITERS = 16 };
    {
        a_pup(kv64, mk);
        for (u64 i = 0; i < NPUP; i++)
            call(t_run, mk, mk_keys, $path(dir), ext, 1000 + i, i, 1, 0);
        T_CLOSE(mk);
    }
    int fd0 = dog_open_fd_count();
    want(fd0 > 0);
    for (int it = 0; it < ITERS; it++) {
        a_pup(kv64, h);
        call(T_OPEN, h);
        ok64 o = T_OPEN(h);
        T_CLOSE(h);
        want(o == PUPNOROOM);
    }
    testeqv(dog_open_fd_count(), fd0, "%d");
    TESTBErmrf(tmp);
    done;
}

//  A non-END FILENext error (a path past the 1024-byte buffer) still closes
//  the DIR handle.
static ok64 PUPTestIterErrLeak(void) {
    sane(1);
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__APPLE__)
    done;  //  MAXPATHLEN is 1024 there: the precondition cannot be staged
#endif
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    char deep[1100];
    int dlen = snprintf(deep, sizeof deep, "%s", tmp);
    char seg[256];
    memset(seg, 'a', 200);
    seg[200] = 0;
    while (dlen + 1 + 200 < 1015) {
        char path[1100];
        snprintf(path, sizeof path, "%s/%s", deep, seg);
        if (mkdir(path, 0755) != 0) { TESTBErmrf(tmp); want(0); }
        dlen = snprintf(deep, sizeof deep, "%s", path);
    }
    {
        char fpath[1200];
        snprintf(fpath, sizeof fpath, "%s/%s", deep, seg);
        int fd = open(fpath, O_CREAT | O_WRONLY, 0644);
        if (fd >= 0) close(fd);
    }
    a_cstr(ext, ".test.idx");
    int fd0 = dog_open_fd_count();
    want(fd0 > 0);
    for (int it = 0; it < 64; it++) {
        a_path(dir, ((u8cs){(u8 *)deep, (u8 *)deep + strlen(deep)}));
        a_pup(kv64, h);
        ok64 o = T_OPEN(h);
        T_CLOSE(h);
        want(o != OK);
    }
    testeqv(dog_open_fd_count(), fd0, "%d");
    TESTBErmrf(tmp);
    done;
}

// --- DOG-027: the memtable — put, seal, commit --------------------------

//  Unsorted arrivals, keep-last dup; a second read sees the same rows.
static ok64 PUPTestMemRoundTrip(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".mem.idx");
    a_pup(kv64, h);
    kv64 pre = {1, 1};
    want(PUPkv64Put(h, &pre) == NOROOM);  //  D-2: no memtable mapped yet
    call(kv64bMap, BOXkv64Mem(h), T_PAGE);
    u64 const kv[4][2] = {{7, 70}, {3, 30}, {5, 50}, {3, 31}};
    for (int i = 0; i < 4; i++)
        call(t_put, h, h_keys, $path(dir), ext, kv[i][0], kv[i][1]);
    for (int pass = 0; pass < 2; pass++) {
        kv64 got[16];
        kv64s out = {got, got + 16};
        u32 nsrc = 0;
        call(t_query, out, &nsrc, h);
        testeqv((long long)(out[0] - got), 3LL, "%lld");
        testeqv((long long)nsrc, 1LL, "%lld");
        want(got[0].key == 3 && got[0].val == 31);
        want(got[1].key == 5 && got[1].val == 50);
        want(got[2].key == 7 && got[2].val == 70);
    }
    want(Bpastlen(h) == 0 && u64bDataLen(h_keys) == 0);
    T_CLOSE(h);
    kv64bUnMap(BOXkv64Mem(h));
    TESTBErmrf(tmp);
    done;
}

//  Past 512 bytes the segment is a run and the 1/8 ladder joins the runs; a
//  small tail stays a second run; a tie goes to the younger row.
static ok64 PUPTestMemCollapse(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".mem.idx");
    a_pup(kv64, h);
    call(kv64bMap, BOXkv64Mem(h), T_PAGE);
    for (u64 i = 0; i < 40; i++)
        call(t_put, h, h_keys, $path(dir), ext, 1000 - i, i);
    kv64 got[64];
    kv64s out = {got, got + 64};
    u32 nsrc = 0;
    call(t_query, out, &nsrc, h);
    testeqv((long long)nsrc, 1LL, "%lld");
    testeqv((long long)(out[0] - got), 40LL, "%lld");
    for (int i = 1; i < 40; i++) want(got[i - 1].key < got[i].key);
    call(t_put, h, h_keys, $path(dir), ext, 5000, 1);
    call(t_put, h, h_keys, $path(dir), ext, 970, 777);
    for (int pass = 0; pass < 2; pass++) {
        kv64s o2 = {got, got + 64};
        call(t_query, o2, &nsrc, h);
        testeqv((long long)nsrc, 2LL, "%lld");
        testeqv((long long)(o2[0] - got), 41LL, "%lld");
        b8 hit = NO;
        for (kv64 *p = got; p < o2[0]; p++)
            if (p->key == 970) hit = p->val == 777;
        want(hit);
    }
    T_CLOSE(h);
    kv64bUnMap(BOXkv64Mem(h));
    TESTBErmrf(tmp);
    done;
}

//  A full one-page memtable says NOROOM; the owner commits it into a run a
//  fresh scan sees; Drop takes the run and the memtable still takes puts.
static ok64 PUPTestMemFlush(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".mem.idx");
    u64 fill = T_PAGE;
    a_pup(kv64, h);
    call(kv64bMap, BOXkv64Mem(h), fill);
    for (u64 i = 0; i < fill; i++)
        call(t_put, h, h_keys, $path(dir), ext, 10000 - i, i);
    want(Bpastlen(h) == 0);
    call(t_put, h, h_keys, $path(dir), ext, 42, 4242);
    want(Bpastlen(h) == 1);
    u64 runkey = u64bDataHead(h_keys)[0];
    want(runkey != 0);

    a_pup(kv64, fresh);
    call(T_OPEN, fresh);
    want(Bpastlen(fresh) == 1 && u64bDataHead(fresh_keys)[0] == runkey);
    kv64cs rr = {};
    t_rows(rr, fresh, 0);
    testeqv((long long)$len(rr), (long long)fill, "%lld");
    for (u64 i = 1; i < fill; i++) want(rr[0][i - 1].key < rr[0][i].key);
    T_CLOSE(fresh);

    Bkv64 gotb = {};
    call(kv64bAllocate, gotb, (u32)fill + 64);
    kv64 *got = kv64bIdleHead(gotb);
    kv64s out = {got, kv64bTerm(gotb)};
    u32 nsrc = 0;
    try(t_query, out, &nsrc, h);
    nedo { kv64bFree(gotb); fail(__); }
    testeqv((long long)nsrc, 2LL, "%lld");
    testeqv((long long)(out[0] - got), (long long)(fill + 1), "%lld");
    b8 hit = NO;
    for (kv64 *p = got; p < out[0]; p++)
        if (p->key == 42) hit = p->val == 4242;
    want(hit);
    kv64bFree(gotb);

    call(PUPkv64Drop, h, h_keys, $path(dir), ext, 0);
    want(Bpastlen(h) == 0 && u64bDataLen(h_keys) == 0);
    call(t_put, h, h_keys, $path(dir), ext, 9, 99);
    T_CLOSE(h);
    kv64bUnMap(BOXkv64Mem(h));
    TESTBErmrf(tmp);
    done;
}

//  Commit folds the memtable into one complete `<10-RON64><ext>` run.
static ok64 PUPTestMemCommit(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".mem.idx");
    a_pup(kv64, h);
    call(kv64bMap, BOXkv64Mem(h), T_PAGE);
    call(t_put, h, h_keys, $path(dir), ext, 2, 20);
    call(t_put, h, h_keys, $path(dir), ext, 1, 10);
    call(t_put, h, h_keys, $path(dir), ext, 1, 11);
    call(t_commit, h, h_keys, $path(dir), ext, YES, NULL);
    want(Bpastlen(h) == 1 && Bdatalen(h) == 0);
    want(u64bDataHead(h_keys)[0] != 0);
    char mp[300];
    snprintf(mp, sizeof mp, "%s/%s", tmp, ".mem.idx");
    struct stat st = {};
    want(stat(mp, &st) != 0);

    a_pup(kv64, fresh);
    call(T_OPEN, fresh);
    want(Bpastlen(fresh) == 1);
    kv64cs rr = {};
    t_rows(rr, fresh, 0);
    testeqv((long long)$len(rr), 2LL, "%lld");
    want(rr[0][0].key == 1 && rr[0][0].val == 11);
    want(rr[0][1].key == 2 && rr[0][1].val == 20);
    T_CLOSE(fresh);
    T_CLOSE(h);
    kv64bUnMap(BOXkv64Mem(h));
    TESTBErmrf(tmp);
    done;
}

//  A stray `<ext>` file and an 8-char legacy name are not runs: the open
//  skips them and leaves them on disk.
static ok64 PUPTestMemCrashInvisible(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".mem.idx");
    {
        a_pup(kv64, mk);
        call(t_run, mk, mk_keys, $path(dir), ext, 1000, 1, 1, 0);
        T_CLOSE(mk);
    }
    char mp[300];
    snprintf(mp, sizeof mp, "%s/%s", tmp, ".mem.idx");
    int fd = open(mp, O_CREAT | O_WRONLY, 0600);
    want(fd >= 0);
    want(write(fd, "garbage unsorted crash bytes", 28) == 28);
    close(fd);
    char legacy[300];
    snprintf(legacy, sizeof legacy, "%s/%s", tmp, "00000001.mem.idx");
    fd = open(legacy, O_CREAT | O_WRONLY, 0600);
    want(fd >= 0);
    want(write(fd, "sixteen-byte-row", 16) == 16);
    close(fd);

    a_pup(kv64, h);
    call(T_OPEN, h);
    want(Bpastlen(h) == 1 && u64bDataHead(h_keys)[0] == 1000);
    struct stat lst = {};
    want(stat(legacy, &lst) == 0);
    T_CLOSE(h);
    TESTBErmrf(tmp);
    done;
}

// --- DOG-027: the ladder ------------------------------------------------

//  Eight page-full commits collapse into one run; the 1-row tail stays.
static ok64 PUPTestLadder(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".mem.idx");
    a_pup(kv64, h);
    call(kv64bMap, BOXkv64Mem(h), T_PAGE);
    u64 n = T_PAGE * 8;
    for (u64 i = 0; i < n; i++)
        call(t_put, h, h_keys, $path(dir), ext, 100000 - i, i);
    call(t_put, h, h_keys, $path(dir), ext, 99999, 7777);
    call(t_commit, h, h_keys, $path(dir), ext, YES, NULL);
    testeqv((long long)Bpastlen(h), 2LL, "%lld");
    kv64cs a = {}, b = {};
    t_rows(a, h, 0);
    t_rows(b, h, 1);
    want($len(b) * ABC_HIT_LADDER_DIV <= $len(a));
    testeqv((long long)$len(a), (long long)n, "%lld");

    a_pup(kv64, fresh);
    call(T_OPEN, fresh);
    testeqv((long long)Bpastlen(fresh), 2LL, "%lld");
    T_CLOSE(fresh);

    Bkv64 got = {};
    call(kv64bAllocate, got, n + 64);
    kv64 *gb = kv64bIdleHead(got);
    kv64s out = {gb, kv64bTerm(got)};
    u32 nsrc = 0;
    try(t_query, out, &nsrc, h);
    nedo { kv64bFree(got); fail(__); }
    testeqv((long long)(out[0] - gb), (long long)n, "%lld");
    b8 hit = NO;
    for (kv64 *p = gb; p < out[0]; p++) {
        if (p > gb) want(p[-1].key < p->key);
        if (p->key == 99999) hit = p->val == 7777;
    }
    want(hit);
    kv64bFree(got);
    T_CLOSE(h);
    kv64bUnMap(BOXkv64Mem(h));
    TESTBErmrf(tmp);
    done;
}

// PUPS-004, PUPS-005: a ladder merge takes its youngest source's key + 1.
static ok64 PUPTestLadderKey(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".key.idx");
    a_pup(kv64, h);
    for (u64 r = 0; r < 3; r++)
        call(t_run, h, h_keys, $path(dir), ext, 1000 + r, 10 + r, 1, 0);
    call(PUPkv64Ladder, h, h_keys, $path(dir), ext, NULL, NULL);
    want(Bpastlen(h) == 1 && u64bDataHead(h_keys)[0] == 1003);
    a_pup(kv64, fresh);
    call(T_OPEN, fresh);
    want(Bpastlen(fresh) == 1 && u64bDataHead(fresh_keys)[0] == 1003);
    T_CLOSE(fresh);
    T_CLOSE(h);
    TESTBErmrf(tmp);
    done;
}

//  PUPS-001: a tombstone hides its key at once, a partial ladder merge keeps
//  it, the merge that spans the whole stack drops it and what it shadowed.
static ok64 PUPTestTomb(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".tomb.idx");
    a_pup(kv64, h);
    kv64 out[64];
    kv64s o;
    kv64cs sl;
    call(t_run, h, h_keys, $path(dir), ext, 1000, 1, 20, 0);
    call(t_run, h, h_keys, $path(dir), ext, 1001, 2, 1, PUP_TOMB);
    call(PUPkv64Ladder, h, h_keys, $path(dir), ext, NULL, NULL);
    testeqv((long long)Bpastlen(h), 2LL, "%lld");
    o[0] = out, o[1] = out + 64;
    call(PUPkv64Read, o, h);
    testeqv((long long)(o[0] - out), 19LL, "%lld");
    for (kv64 *r = out; r < o[0]; r++) want(r->key != 2);

    call(t_run, h, h_keys, $path(dir), ext, 1002, 30, 1, 0);
    call(PUPkv64Ladder, h, h_keys, $path(dir), ext, NULL, NULL);
    testeqv((long long)Bpastlen(h), 2LL, "%lld");
    t_rows(sl, h, 1);
    testeqv((long long)$len(sl), 2LL, "%lld");  //  the tomb is kept
    o[0] = out, o[1] = out + 64;
    call(PUPkv64Read, o, h);
    testeqv((long long)(o[0] - out), 20LL, "%lld");

    call(t_run, h, h_keys, $path(dir), ext, 1010, 31, 3, 0);
    call(PUPkv64Ladder, h, h_keys, $path(dir), ext, NULL, NULL);
    testeqv((long long)Bpastlen(h), 1LL, "%lld");
    t_rows(sl, h, 0);
    testeqv((long long)$len(sl), 23LL, "%lld");
    $for(kv64c, r, sl) want(r->val != PUP_TOMB && r->key != 2);
    T_CLOSE(h);

    a_cstr(ext2, ".gone.idx");
    a_pup(kv64, p2);
    call(t_run, p2, p2_keys, $path(dir), ext2, 2000, 5, 1, 50);
    call(t_run, p2, p2_keys, $path(dir), ext2, 2001, 5, 1, PUP_TOMB);
    call(PUPkv64Ladder, p2, p2_keys, $path(dir), ext2, NULL, NULL);
    o[0] = out, o[1] = out + 64;
    call(PUPkv64Read, o, p2);
    testeqv((long long)(o[0] - out), 0LL, "%lld");
    testeqv((long long)Bpastlen(p2), 0LL, "%lld");
    T_CLOSE(p2);
    TESTBErmrf(tmp);
    done;
}

//  PUPS-005: crash points for a ladder merge.  The binary interposes `link`
//  and `unlink`: once armed, the `pup_crash_at`-th one on a run kills it.
static int pup_crash_at = 0, pup_crash_ops = 0;
static char const *pup_crash_ext = "";
#define PUP_CRASHED 77

static void pup_crash_tick(char const *path) {
    if (pup_crash_at == 0) return;
    size_t l = strlen(path), e = strlen(pup_crash_ext);
    if (l < e || strcmp(path + l - e, pup_crash_ext) != 0) return;
    if (++pup_crash_ops == pup_crash_at) _exit(PUP_CRASHED);
}

static char const *pup_unlink_deny = NULL;  //  this ext's unlinks fail

int unlink(char const *path) {
    size_t l = strlen(path), e = pup_unlink_deny ? strlen(pup_unlink_deny) : 0;
    if (e && l >= e && strcmp(path + l - e, pup_unlink_deny) == 0) {
        errno = EACCES;
        return -1;
    }
    int rc = unlinkat(AT_FDCWD, path, 0);
    if (rc == 0) pup_crash_tick(path);
    return rc;
}

int link(char const *from, char const *to) {
    int rc = linkat(AT_FDCWD, from, AT_FDCWD, to, 0);
    if (rc == 0) pup_crash_tick(to);
    return rc;
}

//  Three equal runs (a bottom merge): 1..4, deletes of 2, 3 plus 4', 5, then
//  6..9; `gone` deletes everything, so the merge output is empty.
static ok64 crash_stack(path8s dir, u8cs ext, b8 gone) {
    sane(1);
    a_pup(kv64, h);
    kv64 runs[3][4] = {
        {{1, 10}, {2, 20}, {3, 30}, {4, 40}},
        {{2, PUP_TOMB}, {3, PUP_TOMB}, {4, 41}, {5, 50}},
        {{6, 60}, {7, 70}, {8, 80}, {9, 90}},
    };
    kv64 dead[2][4] = {
        {{1, 10}, {2, 20}, {3, 30}, {4, 40}},
        {{1, PUP_TOMB}, {2, PUP_TOMB}, {3, PUP_TOMB}, {4, PUP_TOMB}},
    };
    for (u32 r = 0; r < (gone ? 2u : 3u); r++) {
        kv64 *rows = gone ? dead[r] : runs[r];
        kv64cs rs = {rows, rows + 4};
        call(PUPkv64Create, h, h_keys, dir, ext, rs, 1000 + r, NO, NULL);
    }
    T_CLOSE(h);
    done;
}

static ok64 crash_read(kv64s out, u32 *nruns, path8s dir, u8cs ext) {
    sane(1);
    a_pup(kv64, h);
    call(PUPkv64Open, h, h_keys, dir, ext, u64max);
    *nruns = (u32)Bpastlen(h);
    call(PUPkv64Read, out, h);
    T_CLOSE(h);
    done;
}

//  PUPS-005: a merge killed after its rename or any unlink reads as before.
static ok64 PUPTestCrashLadder(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    char extc[16];
    for (int gone = 0; gone < 2; gone++) {
        snprintf(extc, sizeof extc, ".r%d.idx", gone);
        a_cstr(rext, extc);
        call(crash_stack, $path(dir), rext, gone);
        kv64 want_rows[16], got[16];
        kv64s w = {want_rows, want_rows + 16};
        u32 nr = 0;
        call(crash_read, w, &nr, $path(dir), rext);
        size_t nwant = (size_t)(w[0] - want_rows);
        testeqv((long long)nwant, gone ? 0LL : 7LL, "%lld");
        int steps = gone ? 2 : 4;
        for (int k = 1; k <= steps + 1; k++) {
            int at = k > steps ? 9 : k;
            snprintf(extc, sizeof extc, ".c%d%d.idx", gone, k);
            a_cstr(ext, extc);
            call(crash_stack, $path(dir), ext, gone);
            pid_t pid = fork();
            want(pid >= 0);
            if (pid == 0) {
                a_pup(kv64, h);
                pup_crash_ext = extc;
                pup_crash_at = at;
                if (T_OPEN(h) != OK) _exit(2);
                if (PUPkv64Ladder(h, h_keys, $path(dir), ext, NULL, NULL) != OK)
                    _exit(3);
                _exit(0);
            }
            int st = 0;
            want(waitpid(pid, &st, 0) == pid && WIFEXITED(st));
            testeqv(WEXITSTATUS(st), k > steps ? 0 : PUP_CRASHED, "%d");
            kv64s g = {got, got + 16};
            u32 runs = 0;
            call(crash_read, g, &runs, $path(dir), ext);
            testeqv((long long)(g[0] - got), (long long)nwant, "%lld");
            want(memcmp(got, want_rows, nwant * sizeof(kv64)) == 0);
            if (k > steps) testeqv(runs, gone ? 0u : 1u, "%u");
        }
    }
    TESTBErmrf(tmp);
    done;
}

//  A ladder whose source unlinks fail still houses the merged run and drops
//  the sources' cells: the state of a crash past the rename (PUPS-005).
static ok64 PUPTestLadderUnlinkFail(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".deny.idx");
    a_pup(kv64, h);
    for (u64 r = 0; r < 3; r++)
        call(t_run, h, h_keys, $path(dir), ext, 1000 + r, 10 + r, 1, 0);
    pup_unlink_deny = ".deny.idx";
    ok64 o = PUPkv64Ladder(h, h_keys, $path(dir), ext, NULL, NULL);
    pup_unlink_deny = NULL;
    want(o != OK);
    want(Bpastlen(h) == 1 && u64bDataHead(h_keys)[0] == 1003);
    kv64 got[8];
    kv64s out = {got, got + 8};
    call(PUPkv64Read, out, h);
    testeqv((long long)(out[0] - got), 3LL, "%lld");
    T_CLOSE(h);
    TESTBErmrf(tmp);
    done;
}

//  PUPS-26B04KwMzK: forked writers, no lock, clashing clock keys, constant
//  ladders, lose no run and no row.  60 runs: the house holds 62.
#define LOCK_KIDS 4
#define LOCK_N 15
#define LOCK_COMMITS 12
static ok64 PUPTestLock(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".lock.idx");
    a_cstr(ext2, ".lock2.idx");
    pid_t kid[LOCK_KIDS];
    for (int c = 0; c < LOCK_KIDS; c++) {
        kid[c] = fork();
        want(kid[c] >= 0);
        if (kid[c] > 0) continue;
        int bad = 0;
        a_pup(kv64, mine);
        a_pup(kv64, cm);
        if (kv64bMap(BOXkv64Mem(cm), T_PAGE) != OK) _exit(2);
        for (int i = 0; i < LOCK_N; i++) {
            kv64 row = {(u64)c * 1000 + i, 7};
            kv64cs rs = {&row, &row + 1};
            if (PUPkv64Create(mine, mine_keys, $path(dir), ext, rs, 0, NO,
                              NULL) != OK)
                bad = 1;
            if (i >= LOCK_COMMITS) continue;
            if (PUPkv64Put(cm, &row) != OK) bad = 1;
            if (t_commit(cm, cm_keys, $path(dir), ext2, YES, NULL) != OK)
                bad = 1;
        }
        _exit(bad);
    }
    for (int c = 0; c < LOCK_KIDS; c++) {
        int st = 0;
        want(waitpid(kid[c], &st, 0) == kid[c]);
        want(WIFEXITED(st) && WEXITSTATUS(st) == 0);
    }
    a_pup(kv64, a);
    call(T_OPEN, a);
    testeqv((long long)Bpastlen(a), (long long)LOCK_KIDS * LOCK_N, "%lld");
    T_CLOSE(a);
    a_pup(kv64, b);
    call(PUPkv64Open, b, b_keys, $path(dir), ext2, u64max);
    kv64 out[LOCK_KIDS * LOCK_COMMITS + 1];
    kv64s o = {out, out + LOCK_KIDS * LOCK_COMMITS + 1};
    call(PUPkv64Read, o, b);
    testeqv((long long)(o[0] - out), (long long)LOCK_KIDS * LOCK_COMMITS,
            "%lld");
    want(Bpastlen(b) < LOCK_KIDS * LOCK_COMMITS);  //  the ladder ran
    T_CLOSE(b);
    TESTBErmrf(tmp);
    done;
}

// DOG-027: Drop(i) unlinks the run and closes the gap in cells and keys.
static ok64 PUPTestDrop(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".mem.idx");
    a_pup(kv64, h);
    for (u64 r = 0; r < 3; r++)
        call(t_run, h, h_keys, $path(dir), ext, 1000 + r, 10 + r, 1, 0);
    testeqv((long long)Bpastlen(h), 3LL, "%lld");
    call(PUPkv64Drop, h, h_keys, $path(dir), ext, 1);
    testeqv((long long)Bpastlen(h), 2LL, "%lld");
    want(u64bDataLen(h_keys) == 2 && u64bDataHead(h_keys)[1] == 1002);
    want(h[0][1][0][0].key == 12);
    want(PUPkv64Drop(h, h_keys, $path(dir), ext, 9) == PUPBADARG);
    a_pup(kv64, fresh);
    call(T_OPEN, fresh);
    testeqv((long long)Bpastlen(fresh), 2LL, "%lld");
    want(u64bDataHead(fresh_keys)[0] == 1000);
    want(u64bDataHead(fresh_keys)[1] == 1002);
    T_CLOSE(fresh);
    kv64 got[16];
    kv64s out = {got, got + 16};
    u32 nsrc = 0;
    call(t_query, out, &nsrc, h);
    testeqv((long long)(out[0] - got), 2LL, "%lld");
    want(got[0].key == 10 && got[1].key == 12);
    T_CLOSE(h);
    TESTBErmrf(tmp);
    done;
}

// DOG-027: a key whose NAME is taken is stepped over, never written through.
static ok64 PUPTestNameTaken(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".take.idx");
    u64 const M = 0x0FFFFFFFFFFFF00ULL;  //  above RONNow: the pick is M+1
    a_pup(kv64, h);
    call(t_run, h, h_keys, $path(dir), ext, M, 1, 1, 0);
    a_pad(u8, name, PUP_SEQNO_W + 32);
    call(RONu8sFeedPad, u8bIdle(name), (ok64)(M + 1), PUP_SEQNO_W);
    call(u8bFeed, name, ext);
    a_path(squat, $path(dir), u8bDataC(name));
    int sfd = open((char *)u8bDataHead(squat), O_CREAT | O_WRONLY, 0600);
    want(sfd >= 0);
    want(write(sfd, "squatter--------", 16) == 16);
    close(sfd);
    call(t_run, h, h_keys, $path(dir), ext, 0, 2, 1, 0);
    want(Bpastlen(h) == 2);
    want(u64bDataHead(h_keys)[0] == M && u64bDataHead(h_keys)[1] == M + 2);
    char back[17] = "";
    int rfd = open((char *)u8bDataHead(squat), O_RDONLY);
    want(rfd >= 0);
    want(read(rfd, back, 16) == 16);
    close(rfd);
    want(memcmp(back, "squatter--------", 16) == 0);
    want(t_run(h, h_keys, $path(dir), ext, M + 1, 3, 1, 0) == PUPTAKEN);
    want(Bpastlen(h) == 2);
    T_CLOSE(h);
    TESTBErmrf(tmp);
    done;
}

// DOG-046: two writers on ONE dir, memtables in memory: every row survives.
static ok64 PUPTestTwoWriters(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".two.idx");
    a_pup(kv64, A);
    a_pup(kv64, B);
    call(kv64bMap, BOXkv64Mem(A), T_PAGE);
    call(kv64bMap, BOXkv64Mem(B), T_PAGE);
    for (u64 i = 1; i <= 5; i++) call(t_put, A, A_keys, $path(dir), ext, i, i);
    for (u64 i = 100; i <= 105; i++)
        call(t_put, B, B_keys, $path(dir), ext, i, i);
    call(t_put, A, A_keys, $path(dir), ext, 6, 6);
    {
        u32 files = 0;
        DIR *dh = opendir(tmp);
        want(dh != NULL);
        for (struct dirent *e; (e = readdir(dh));) files += e->d_name[0] != '.';
        closedir(dh);
        testeqv((long long)files, 0LL, "%lld");
    }
    want(t_commit(A, A_keys, $path(dir), ext, YES, NULL) == OK);
    want(t_commit(B, B_keys, $path(dir), ext, YES, NULL) == OK);
    T_CLOSE(A);
    T_CLOSE(B);
    kv64bUnMap(BOXkv64Mem(A));
    kv64bUnMap(BOXkv64Mem(B));
    a_pup(kv64, fresh);
    call(T_OPEN, fresh);
    kv64 got[32];
    kv64s out = {got, got + 32};
    u32 nsrc = 0;
    call(t_query, out, &nsrc, fresh);
    testeqv((long long)(out[0] - got), 12LL, "%lld");
    for (u64 i = 1; i <= 6; i++)
        want(got[i - 1].key == i && got[i - 1].val == i);
    for (u64 i = 100; i <= 105; i++)
        want(got[i - 94].key == i && got[i - 94].val == i);
    T_CLOSE(fresh);
    TESTBErmrf(tmp);
    done;
}

//  PUPS-26B04KwMzK: the caller's sync, what Commit once did itself: house
//  the dir's runs `h` lacks, forget the housed ones gone from the dir.
static ok64 t_adopt(kv64csb h, u64b keys, path8s dir, u8cs ext) {
    sane(1);
    a_pad(u64, found, 64);
    u64 const *none[2] = {};
    call(PUPScan, found, dir, ext, u64max, none);
    for (size_t i = u64bDataLen(keys); i > 0; i--) {
        b8 on = NO;
        $for(u64c, f, found_datac) on |= *f == u64bDataHead(keys)[i - 1];
        if (!on) call(PUPkv64Forget, h, keys, i - 1);
    }
    $for(u64c, f, found_datac) {
        ok64 o = PUPkv64Map(h, keys, dir, ext, *f);
        if (o != OK && o != PUPTAKEN) fail(o);
    }
    done;
}

//  YES when `dir` holds run `key`.
static b8 t_on(path8s dir, u8cs ext, u64 key) {
    a_pad(u64, found, 64);
    u64 const *none[2] = {};
    if (PUPScan(found, dir, ext, u64max, none) != OK) return NO;
    $for(u64c, f, found_datac) if (*f == key) return YES;
    return NO;
}

// DOG-046: a run another handle seals is mapped in by this one's adoption.
static ok64 PUPTestForeignRun(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".fgn.idx");
    a_pup(kv64, A);
    a_pup(kv64, B);
    call(kv64bMap, BOXkv64Mem(A), T_PAGE);
    call(kv64bMap, BOXkv64Mem(B), T_PAGE);
    for (u64 i = 1; i <= 3; i++) call(t_put, A, A_keys, $path(dir), ext, i, i);
    for (u64 i = 1000; i < 1064; i++)
        call(t_put, B, B_keys, $path(dir), ext, i, i);
    want(t_commit(B, B_keys, $path(dir), ext, YES, NULL) == OK);
    testeqv((long long)Bpastlen(B), 1LL, "%lld");
    u64 bkey = u64bDataHead(B_keys)[0];
    testeqv((long long)Bpastlen(A), 0LL, "%lld");
    call(t_adopt, A, A_keys, $path(dir), ext);
    want(t_commit(A, A_keys, $path(dir), ext, YES, NULL) == OK);
    testeqv((long long)Bpastlen(A), 2LL, "%lld");
    want(u64bDataHead(A_keys)[0] == bkey && u64bDataHead(A_keys)[1] > bkey);
    kv64 got[128];
    kv64s out = {got, got + 128};
    u32 nsrc = 0;
    call(t_query, out, &nsrc, A);
    testeqv((long long)(out[0] - got), 67LL, "%lld");
    for (u64 i = 1; i <= 3; i++) want(got[i - 1].key == i);
    for (u64 i = 1000; i < 1064; i++) want(got[i - 997].key == i);
    T_CLOSE(A);
    T_CLOSE(B);
    kv64bUnMap(BOXkv64Mem(A));
    kv64bUnMap(BOXkv64Mem(B));
    TESTBErmrf(tmp);
    done;
}

//  PUPS-26B04KwMzK: two houses on one dir, the caller doing the sync: no
//  clobbered or lost run, PUPTAKEN on a taken key, Forget keeps the file,
//  Drop removes it, each house reads the union after adopting.
static ok64 PUPTestMultiWriter(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".mw.idx");
    a_pup(kv64, A);
    a_pup(kv64, B);
    call(kv64bMap, BOXkv64Mem(A), T_PAGE);
    call(kv64bMap, BOXkv64Mem(B), T_PAGE);
    for (u64 i = 1; i <= 3; i++) call(t_put, A, A_keys, $path(dir), ext, i, i);
    for (u64 i = 100; i <= 102; i++)
        call(t_put, B, B_keys, $path(dir), ext, i, i);
    call(PUPkv64Commit, A, A_keys, $path(dir), ext, YES, NULL);
    call(PUPkv64Commit, B, B_keys, $path(dir), ext, YES, NULL);
    u64 ak = u64bDataHead(A_keys)[0], bk = u64bDataHead(B_keys)[0];
    want(ak != bk && t_on($path(dir), ext, ak) && t_on($path(dir), ext, bk));
    //  a taken key: B's explicit one, a seal onto it, a second tmp of it
    want(t_run(A, A_keys, $path(dir), ext, bk, 7, 1, 0) == PUPTAKEN);
    u8bp t1 = NULL, t2 = NULL;
    call(PUPTmpMap, &t1, $path(dir), ext, bk, 64);
    want(PUPTmpMap(&t2, $path(dir), ext, bk, 64) == PUPTAKEN);
    u8cs junk = {(u8c *)"junkjunkjunkjunk", (u8c *)"junkjunkjunkjunk" + 16};
    call(u8bFeed, t1, junk);
    u8cs r1 = {};
    want(PUPTmpSeal(r1, t1, $path(dir), ext, bk, NO) == PUPTAKEN);
    want(PUPTmpMap(&t2, $path(dir), ext, bk, 64) == OK);  //  tmp gone
    call(PUPTmpDrop, t2, $path(dir), ext, bk);
    kv64 got[16];
    kv64s o = {got, got + 16};
    call(PUPkv64Read, o, B);
    want(o[0] - got == 3 && got[0].key == 100);  //  B's run intact
    //  adopt under the caller's lock: both read the union
    int lk = -1;
    call(PUPLock, &lk, $path(dir));
    call(t_adopt, A, A_keys, $path(dir), ext);
    call(t_adopt, B, B_keys, $path(dir), ext);
    PUPUnlock(&lk);
    o[0] = got;
    call(PUPkv64Read, o, A);
    testeqv((long long)(o[0] - got), 6LL, "%lld");
    o[0] = got;
    call(PUPkv64Read, o, B);
    testeqv((long long)(o[0] - got), 6LL, "%lld");
    //  Forget unhouses, the file stays; Map houses it back, once
    call(PUPkv64Forget, B, B_keys, 0);
    want(Bpastlen(B) == 1 && t_on($path(dir), ext, ak));
    call(PUPkv64Map, B, B_keys, $path(dir), ext, ak);
    want(PUPkv64Map(B, B_keys, $path(dir), ext, ak) == PUPTAKEN);
    //  A merges both runs, hands the sources back, unlinks them itself
    call(PUPLock, &lk, $path(dir));
    a_pad(u64, dead, 8);
    call(PUPkv64Ladder, A, A_keys, $path(dir), ext, dead, NULL);
    want(Bpastlen(A) == 1 && u64bDataLen(dead) == 2);
    want(t_on($path(dir), ext, ak) && t_on($path(dir), ext, bk));
    call(PUPUnlink, $path(dir), ext, u64bData(dead));
    call(t_adopt, B, B_keys, $path(dir), ext);
    PUPUnlock(&lk);
    want(Bpastlen(B) == 1 && u64bDataHead(B_keys)[0] == u64bDataHead(A_keys)[0]);
    o[0] = got;
    call(PUPkv64Read, o, B);
    testeqv((long long)(o[0] - got), 6LL, "%lld");
    //  Drop unhouses and unlinks; A's mapping still reads it
    call(PUPkv64Drop, B, B_keys, $path(dir), ext, 0);
    want(Bpastlen(B) == 0 && !t_on($path(dir), ext, u64bDataHead(A_keys)[0]));
    o[0] = got;
    call(PUPkv64Read, o, A);
    testeqv((long long)(o[0] - got), 6LL, "%lld");
    T_CLOSE(A);
    T_CLOSE(B);
    kv64bUnMap(BOXkv64Mem(A));
    kv64bUnMap(BOXkv64Mem(B));
    TESTBErmrf(tmp);
    done;
}

// --- DOG-035: the run ToC — [data][toc][len:5]["TC"] ------------------

//  `n` puts of descending keys from `base` into a fresh `pages`-page
//  memtable, committed; the run's key lands in *key.
static ok64 t_toc_seal(path8s dir, u8cs ext, u64 pages, u64 base, u64 n,
                       u64 val0, u64 *key) {
    sane(key != NULL);
    a_pup(kv64, h);
    call(kv64bMap, BOXkv64Mem(h), pages * T_PAGE);
    for (u64 i = 0; i < n; i++)
        call(t_put, h, h_keys, dir, ext, base - i * (val0 ? 2 : 1),
             val0 ? (n - i) : i);
    call(t_commit, h, h_keys, dir, ext, YES, NULL);
    *key = u64bDataHead(h_keys)[u64bDataLen(h_keys) - 1];
    T_CLOSE(h);
    kv64bUnMap(BOXkv64Mem(h));
    done;
}

static ok64 t_toc_path(char *out, size_t cap, char const *tmp, u64 key,
                       char const *ext) {
    sane(out != NULL);
    a_pad(u8, name, PUP_SEQNO_W + 32);
    call(RONu8sFeedPad, u8bIdle(name), (ok64)key, PUP_SEQNO_W);
    snprintf(out, cap, "%s/%.*s%s", tmp, (int)u8bDataLen(name),
             (char const *)u8bDataHead(name), ext);
    done;
}

//  A four-page seal ends in `[len:5]["TC"]`; Split clips the block off, and
//  cell i is page i's first record.
static ok64 PUPTestTocSeal(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".toc.idx");
    size_t per = PUPTocRows((u32)sizeof(kv64));
    want(per > 0);
    u64 n = (u64)per * 4, key = 0;
    call(t_toc_seal, $path(dir), ext, 4, 1000000, n, 0, &key);
    char path[512];
    call(t_toc_path, path, sizeof path, tmp, key, ".toc.idx");
    struct stat st = {};
    want(stat(path, &st) == 0);
    u8 tail[PUP_TOC_TAIL_W] = {};
    int fd = open(path, O_RDONLY);
    want(fd >= 0);
    want(pread(fd, tail, sizeof tail,
               (off_t)st.st_size - (off_t)sizeof tail) == (ssize_t)sizeof tail);
    close(fd);
    want(tail[PUP_TOC_LEN_W] == 'T' && tail[PUP_TOC_LEN_W + 1] == 'C');
    u64 tb = 0;
    for (u32 k = 0; k < PUP_TOC_LEN_W; k++) tb |= (u64)tail[k] << (8 * k);
    testeqv((long long)tb, (long long)(4 * sizeof(kv64)), "%lld");
    testeqv((long long)st.st_size,
            (long long)(n * sizeof(kv64) + tb + PUP_TOC_TAIL_W), "%lld");

    a_pup(kv64, fresh);
    call(T_OPEN, fresh);
    kv64cs rows = {}, cells = {};
    PUPkv64Split(rows, cells, fresh[0][0]);
    testeqv((long long)((u8c *)fresh[0][0][1] - (u8c *)fresh[0][0][0]),
            (long long)st.st_size, "%lld");
    testeqv((long long)$len(rows), (long long)n, "%lld");
    testeqv((long long)$len(cells), 4LL, "%lld");
    for (size_t i = 0; i < 4; i++) {
        kv64 const *first = rows[0] + i * per;
        want(cells[0][i].key == first->key && cells[0][i].val == first->val);
    }
    for (u64 i = 1; i < n; i++) want(rows[0][i - 1].key < rows[0][i].key);
    want(rows[0][0].key == 1000000 - (n - 1) && rows[0][n - 1].key == 1000000);
    Bkv64 got = {};
    call(kv64bAllocate, got, n + 16);
    kv64 *gb = kv64bIdleHead(got);
    kv64s out = {gb, kv64bTerm(got)};
    u32 nsrc = 0;
    try(t_query, out, &nsrc, fresh);
    nedo { kv64bFree(got); fail(__); }
    testeqv((long long)(out[0] - gb), (long long)n, "%lld");
    for (u64 i = 0; i < n; i++) want(gb[i].key == 1000000 - (n - 1) + i);
    kv64bFree(got);
    T_CLOSE(fresh);
    TESTBErmrf(tmp);
    done;
}

//  A run whose bytes merely END in "TC" is plain: the residue check refuses
//  it, and PUPS-26B034zlKV, so does the geometry check.
static ok64 PUPTestTocFake(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".plain.idx");
    a_pup(kv64, h);
    kv64 fake[2] = {{1, 0}, {2, 0x4354000000000000ULL}};
    kv64cs fs = {fake, fake + 2};
    call(PUPkv64Create, h, h_keys, $path(dir), ext, fs, 1000, NO, NULL);
    kv64 two[2] = {{1, 111}, {2, 0x4354000000000900ULL}};
    kv64cs ts = {two, two + 2};
    call(PUPkv64Create, h, h_keys, $path(dir), ext, ts, 1001, NO, NULL);
    for (size_t i = 0; i < 2; i++) {
        kv64cs rows = {}, toc = {};
        PUPkv64Split(rows, toc, h[0][i]);
        testeqv((long long)$len(rows), 2LL, "%lld");
        want($empty(toc));
        testeqv((long long)((u8c *)h[0][i][1] - (u8c *)h[0][i][0]), 32LL,
                "%lld");
    }
    T_CLOSE(h);
    TESTBErmrf(tmp);
    done;
}

//  A ToC'd run and a younger plain one side by side; newest still wins.
static ok64 PUPTestTocMixed(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".mix.idx");
    size_t per = PUPTocRows((u32)sizeof(kv64));
    u64 n = (u64)per * 4, old = 0;
    a_pup(kv64, h);
    call(kv64bMap, BOXkv64Mem(h), n);
    for (u64 i = 0; i < n; i++)
        call(t_put, h, h_keys, $path(dir), ext, n - i, n - i);
    call(t_commit, h, h_keys, $path(dir), ext, YES, NULL);
    old = u64bDataHead(h_keys)[0];
    Bkv64 flat = {};
    call(kv64bAllocate, flat, (u32)per + 4);
    kv64 *fb = kv64bIdleHead(flat);
    for (size_t i = 0; i < per; i++) fb[i] = (kv64){(u64)(i + 1) * 2, 999};
    kv64cs body = {fb, fb + per};
    call(PUPkv64Create, h, h_keys, $path(dir), ext, body, old + 1, NO, NULL);
    kv64bFree(flat);
    T_CLOSE(h);
    kv64bUnMap(BOXkv64Mem(h));

    a_pup(kv64, fresh);
    call(T_OPEN, fresh);
    testeqv((long long)Bpastlen(fresh), 2LL, "%lld");
    kv64cs d0 = {}, d1 = {}, t0 = {}, t1 = {};
    PUPkv64Split(d0, t0, fresh[0][0]);
    PUPkv64Split(d1, t1, fresh[0][1]);
    want(!$empty(t0) && $empty(t1));
    testeqv((long long)$len(d0), (long long)n, "%lld");
    testeqv((long long)$len(d1), (long long)per, "%lld");
    Bkv64 got = {};
    call(kv64bAllocate, got, n + 16);
    kv64 *gb = kv64bIdleHead(got);
    kv64s out = {gb, kv64bTerm(got)};
    u32 nsrc = 0;
    try(t_query, out, &nsrc, fresh);
    nedo { kv64bFree(got); fail(__); }
    testeqv((long long)(out[0] - gb), (long long)n, "%lld");
    for (u64 i = 0; i < n; i++) {
        u64 k = i + 1;
        want(gb[i].key == k);
        want(gb[i].val == ((k % 2 == 0 && k <= per * 2) ? 999 : k));
    }
    kv64bFree(got);
    T_CLOSE(fresh);
    TESTBErmrf(tmp);
    done;
}

// DOG-035: a TOTAL row order, so a `{k, 0}` needle sorts before its own row.
static b8 dog_kv64_full(kv64 const *a, kv64 const *b) {
    return a->key != b->key ? a->key < b->key : a->val < b->val;
}

//  DOG-035: cell i covers exactly page i, and a needle whose row is a page's
//  FIRST record is answered from the cell; the oracle is a linear scan.
static ok64 PUPTestTocNarrow(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".narrow.idx");
    size_t per = PUPTocRows((u32)sizeof(kv64));
    u64 n = (u64)per * 4, key = 0;
    call(t_toc_seal, $path(dir), ext, 4, n * 2, n, 1, &key);
    a_pup(kv64, fresh);
    call(T_OPEN, fresh);
    kv64cs rows = {}, cells = {};
    PUPkv64Split(rows, cells, fresh[0][0]);
    size_t nc = (size_t)$len(cells);
    testeqv((long long)nc, 4LL, "%lld");
    u32 fromcell = 0;
    for (u64 k = 1; k <= n * 2 + 2; k++) {
        kv64 needle = {k, 0};
        kv64 const *whole = kv64sFindGEZ(rows, &needle, dog_kv64_full);
        b8 present = whole < rows[1] && whole->key == k;
        size_t idx = 0;
        for (size_t i = 0; i < nc; i++)
            if (!dog_kv64_full(&needle, cells[0] + i)) idx = i;
        if (idx + 1 < nc && cells[0][idx + 1].key == k) {
            want(present && whole == rows[0] + (idx + 1) * per);
            want(cells[0][idx + 1].val == whole->val);
            fromcell++;
            continue;
        }
        kv64cs page = {rows[0] + idx * per, rows[1]};
        if ((size_t)$len(page) > per) page[1] = page[0] + per;
        kv64 const *narrow = kv64sFindGEZ(page, &needle, dog_kv64_full);
        if (present) {
            want(narrow < page[1] && narrow == whole);
        } else {
            want(narrow == page[1] || narrow->key != k);
        }
    }
    testeqv((long long)fromcell, (long long)(nc - 1), "%lld");
    T_CLOSE(fresh);
    TESTBErmrf(tmp);
    done;
}

//  DOG-035: the ladder's output is ToC'd too and weighs clipped lengths.
static ok64 PUPTestTocLadder(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".ladder.idx");
    size_t per = PUPTocRows((u32)sizeof(kv64));
    u64 n = (u64)per * 8;
    a_pup(kv64, h);
    call(kv64bMap, BOXkv64Mem(h), T_PAGE);
    for (u64 i = 0; i < n; i++)
        call(t_put, h, h_keys, $path(dir), ext, 100000 - i, i);
    call(t_commit, h, h_keys, $path(dir), ext, YES, NULL);
    T_CLOSE(h);
    kv64bUnMap(BOXkv64Mem(h));
    a_pup(kv64, fresh);
    call(T_OPEN, fresh);
    want(Bpastlen(fresh) >= 1);
    kv64cs big = {}, tocbig = {};
    PUPkv64Split(big, tocbig, fresh[0][0]);
    want((size_t)$len(big) >= 2 * per);
    testeqv((long long)$len(tocbig), (long long)((size_t)$len(big) / per),
            "%lld");
    Bkv64 got = {};
    call(kv64bAllocate, got, n + 64);
    kv64 *gb = kv64bIdleHead(got);
    kv64s out = {gb, kv64bTerm(got)};
    u32 nsrc = 0;
    try(t_query, out, &nsrc, fresh);
    nedo { kv64bFree(got); fail(__); }
    testeqv((long long)(out[0] - gb), (long long)n, "%lld");
    for (u64 i = 1; i < n; i++) want(gb[i - 1].key < gb[i].key);
    want(gb[0].key == 100000 - (n - 1) && gb[n - 1].key == 100000);
    kv64bFree(got);
    T_CLOSE(fresh);
    TESTBErmrf(tmp);
    done;
}

ok64 PUPTestToc(void) {
    sane(1);
    call(PUPTestTocSeal);
    call(PUPTestTocFake);
    call(PUPTestTocMixed);
    call(PUPTestTocNarrow);
    call(PUPTestTocLadder);
    done;
}

// --- PUPS-26B045Ffpb: Get and Range against the merged Read view -----

//  A house of every layer: an old ToC'd run (even keys), a younger plain run
//  (some keys 999), a run of tombstones, DATA runs and the segment.
static ok64 t_layers(kv64csb h, u64b keys, path8s dir, u8cs ext, u64 n) {
    sane(1);
    for (u64 i = 0; i < n; i++)
        call(t_put, h, keys, dir, ext, (n - i) * 2, n - i);
    call(t_commit, h, keys, dir, ext, NO, NULL);
    kv64 rows[64];
    for (u64 i = 0; i < 64; i++) rows[i] = (kv64){i * 6 + 3, 999};
    kv64cs plain = {rows, rows + 64};
    call(PUPkv64Create, h, keys, dir, ext, plain, 0, NO, NULL);
    for (u64 i = 0; i < 64; i++) rows[i] = (kv64){i * 10 + 4, PUP_TOMB};
    kv64cs tomb = {rows, rows + 64};
    call(PUPkv64Create, h, keys, dir, ext, tomb, 0, NO, NULL);
    for (u64 i = 0; i < 100; i++)
        call(t_put, h, keys, dir, ext, (i * 7919) % (n * 2), 5000 + i);
    done;
}

static ok64 PUPTestGet(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".get.idx");
    u64 n = PUPTocRows((u32)sizeof(kv64)) * 6;
    a_pup(kv64, h);
    call(kv64bMap, BOXkv64Mem(h), n);
    call(t_layers, h, h_keys, $path(dir), ext, n);
    want(Bpastlen(h) == 3 && Bdatalen(h) > 0);
    want(BOXkv64Mem(h)[2] > BOXkv64Mem(h)[1]);  //  a live segment
    Bkv64 all = {};
    call(kv64bAllocate, all, n * 2 + 256);
    kv64 *ab = kv64bIdleHead(all);
    kv64s view = {ab, kv64bTerm(all)};
    call(PUPkv64Read, view, h);
    kv64cs oracle = {ab, view[0]};
    for (u64 k = 0; k < n * 2 + 8; k++) {
        kv64 key = {k, 0}, got = {};
        kv64 const *p = kv64sFindGE(oracle, &key);
        b8 in = p < oracle[1] && p->key == k;
        ok64 o = PUPkv64Get(&got, h, &key);
        want(in ? (o == OK && got.val == p->val) : o == NONE);
    }
    kv64bFree(all);
    T_CLOSE(h);
    kv64bUnMap(BOXkv64Mem(h));
    TESTBErmrf(tmp);
    done;
}

static ok64 PUPTestRange(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".range.idx");
    u64 n = PUPTocRows((u32)sizeof(kv64)) * 6;
    a_pup(kv64, h);
    call(kv64bMap, BOXkv64Mem(h), n);
    call(t_layers, h, h_keys, $path(dir), ext, n);
    Bkv64 all = {}, part = {};
    call(kv64bAllocate, all, n * 2 + 256);
    call(kv64bAllocate, part, n * 2 + 256);
    kv64 *ab = kv64bIdleHead(all);
    kv64s view = {ab, kv64bTerm(all)};
    call(PUPkv64Read, view, h);
    kv64cs oracle = {ab, view[0]};
    u64 const span[][2] = {{0, 1}, {3, 4}, {0, 900}, {250, 1300},
                           {n, n * 2 + 1}, {7, 7}, {0, ~0ULL - 1}};
    for (size_t s = 0; s < sizeof span / sizeof *span; s++) {
        kv64 lo = {span[s][0], 0}, hi = {span[s][1], 0};
        kv64cs runs[ABC_HIT_MAX_RUNS];
        kv64css into = {runs, runs + ABC_HIT_MAX_RUNS};
        call(PUPkv64Range, into, h, &lo, &hi);
        kv64 *pb = kv64bIdleHead(part);
        kv64s out = {pb, kv64bTerm(part)};
        call(HITkv64Merge, out, into);
        kv64 *w = pb;
        for (kv64 *r = pb; r < out[0]; r++)
            if (r->val != PUP_TOMB) *w++ = *r;
        kv64cs want_ = {kv64sFindGE(oracle, &lo), kv64sFindGE(oracle, &hi)};
        testeqv((long long)(w - pb), (long long)$len(want_), "%lld");
        want(memcmp(pb, want_[0], (size_t)(w - pb) * sizeof(kv64)) == 0);
    }
    kv64bFree(all);
    kv64bFree(part);
    T_CLOSE(h);
    kv64bUnMap(BOXkv64Mem(h));
    TESTBErmrf(tmp);
    done;
}

//  With a merge hook, Get folds every version, oldest first: two PAST runs,
//  the DATA runs and the segment, the same sum Read gives.
static ok64 PUPTestGetMerge(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".sum.idx");
    a_pup(cnt64, h);
    call(cnt64bMap, BOXcnt64Mem(h), 256);
    u64 sum[50] = {};
    cnt64 rows[50];
    for (u64 i = 0; i < 50; i++) rows[i] = (cnt64){i, 1}, sum[i] += 1;
    cnt64cs r1 = {rows, rows + 50};
    call(PUPcnt64Create, h, h_keys, $path(dir), ext, r1, 1000, NO, NULL);
    for (u64 i = 0; i < 25; i++) rows[i] = (cnt64){i * 2, 10}, sum[i * 2] += 10;
    cnt64cs r2 = {rows, rows + 25};
    call(PUPcnt64Create, h, h_keys, $path(dir), ext, r2, 1001, NO, NULL);
    for (u64 i = 0; i < 70; i++) {
        cnt64 r = {(i * 7) % 50, 100 + i};
        sum[r.key] += r.val;
        call(PUPcnt64Put, h, &r);
    }
    cnt64 **mem = BOXcnt64Mem(h);
    want(Bpastlen(h) == 2 && Bdatalen(h) > 0 && mem[2] > mem[1]);
    for (u64 k = 0; k < 52; k++) {
        cnt64 key = {k, 0}, got = {};
        ok64 o = PUPcnt64Get(&got, h, &key);
        want(k < 50 ? (o == OK && got.val == sum[k]) : o == NONE);
    }
    cnt64 all[64];
    cnt64s view = {all, all + 64};
    call(PUPcnt64Read, view, h);
    testeqv((long long)(view[0] - all), 50LL, "%lld");
    for (u64 k = 0; k < 50; k++) want(all[k].val == sum[k]);
    PUPcnt64Close(h, h_keys);
    cnt64bUnMap(BOXcnt64Mem(h));
    TESTBErmrf(tmp);
    done;
}

// --- PUPS-26A276Xh2e: runs hold no fd, the memtable is no file ---------

static int pup_proc_fds(void) {
    DIR *d = opendir("/proc/self/fd");
    if (d == NULL) return dog_open_fd_count();
    int n = 0;
    while (readdir(d) != NULL) n++;
    closedir(d);
    return n;
}

//  A full house of 62 runs is open at once, every one readable, no fd held;
//  a 63rd create is PUPNOROOM.
static ok64 PUPTestManyRuns(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".many.idx");
    enum { N = ABC_HIT_MAX_RUNS - 2 };
    int fd0 = pup_proc_fds();
    want(fd0 > 0);
    a_pup(kv64, h);
    for (u64 i = 0; i < N; i++)
        call(t_run, h, h_keys, $path(dir), ext, 1000 + i, i, 1, 0);
    want(t_run(h, h_keys, $path(dir), ext, 2000, 0, 1, 0) == PUPNOROOM);
    testeqv(pup_proc_fds(), fd0, "%d");
    T_CLOSE(h);
    a_pup(kv64, fresh);
    call(T_OPEN, fresh);
    testeqv((long long)Bpastlen(fresh), (long long)N, "%lld");
    testeqv(pup_proc_fds(), fd0, "%d");
    for (u32 i = 0; i < N; i++) {
        kv64cs d = {};
        t_rows(d, fresh, i);
        want($len(d) == 1 && d[0]->key == i && d[0]->val == (u64)i * 10);
    }
    T_CLOSE(fresh);
    TESTBErmrf(tmp);
    done;
}

//  The memtable is anonymous memory: puts and a commit hold no fd and leave
//  no file but the run.
static ok64 PUPTestMemNoFd(void) {
    sane(1);
    call(FILEInit);
    T_DIR(".nofd.idx");
    int fd0 = pup_proc_fds();
    want(fd0 > 0);
    a_pup(kv64, h);
    call(kv64bMap, BOXkv64Mem(h), T_PAGE);
    for (u64 i = 0; i < 200; i++)
        call(t_put, h, h_keys, $path(dir), ext, (i * 7919) % 200, i);
    want(Bdatalen(h) > 0);
    testeqv(pup_proc_fds(), fd0, "%d");
    u32 files = 0;
    DIR *dh = opendir(tmp);
    want(dh != NULL);
    for (struct dirent *e; (e = readdir(dh));) files += e->d_name[0] != '.';
    closedir(dh);
    testeqv((long long)files, 0LL, "%lld");
    kv64 got[256], got2[256];
    kv64s out = {got, got + 256};
    call(PUPkv64Read, out, h);
    testeqv((long long)(out[0] - got), 200LL, "%lld");
    call(t_commit, h, h_keys, $path(dir), ext, YES, NULL);
    want(Bdatalen(h) == 0 && Bpastlen(h) == 1);
    testeqv(pup_proc_fds(), fd0, "%d");
    kv64s out2 = {got2, got2 + 256};
    call(PUPkv64Read, out2, h);
    testeqv((long long)(out2[0] - got2), 200LL, "%lld");
    want(memcmp(got, got2, sizeof(kv64) * 200) == 0);
    T_CLOSE(h);
    kv64bUnMap(BOXkv64Mem(h));
    TESTBErmrf(tmp);
    done;
}

// PUPS-26B034NDyT: a lock is keyed by the directory's identity: an alias
// re-enters the held lock, and more than 8 directories may be held.
static ok64 PUPTestLockAlias(void) {
    sane(1);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    char al[300];
    snprintf(al, sizeof al, "%s.alias", tmp);
    want(symlink(tmp, al) == 0);
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    a_path(ali, ((u8cs){(u8 *)al, (u8 *)al + strlen(al)}));
    alarm(10);  //  the bug blocks forever in flock
    int a = -1, b = -1;
    call(PUPLock, &a, $path(dir));
    call(PUPLock, &b, $path(ali));
    want(a == b);
    PUPUnlock(&b);
    PUPUnlock(&a);
    alarm(0);
    enum { N = 12 };
    int lk[N];
    char sub[N][300];
    for (int i = 0; i < N; i++) {
        snprintf(sub[i], sizeof sub[i], "%s/d%d", tmp, i);
        want(mkdir(sub[i], 0755) == 0);
        a_path(sd, ((u8cs){(u8 *)sub[i], (u8 *)sub[i] + strlen(sub[i])}));
        call(PUPLock, &lk[i], $path(sd));
    }
    for (int i = 0; i < N; i++) PUPUnlock(&lk[i]);
    unlink(al);
    TESTBErmrf(tmp);
    done;
}

ok64 PUPStest() {
    sane(1);
    call(PUPTestOverflowLeak);
    call(PUPTestIterErrLeak);
    call(PUPTestNameTaken);
    call(PUPTestLadder);
    call(PUPTestDrop);
    call(PUPTestMemRoundTrip);
    call(PUPTestMemCollapse);
    call(PUPTestMemFlush);
    call(PUPTestMemCommit);
    call(PUPTestMemCrashInvisible);
    call(PUPTestLadderKey);
    call(PUPTestTomb);
    call(PUPTestCrashLadder);
    call(PUPTestLadderUnlinkFail);
    call(PUPTestLock);
    call(PUPTestLockAlias);
    call(PUPTestTwoWriters);
    call(PUPTestForeignRun);
    call(PUPTestMultiWriter);
    call(PUPTestManyRuns);
    call(PUPTestMemNoFd);
    call(PUPTestToc);
    call(PUPTestGet);
    call(PUPTestRange);
    call(PUPTestGetMerge);
    done;
}

TEST(PUPStest);
