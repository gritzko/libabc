#include "PUPS.h"

#include <dirent.h>
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

// DOG-027: keyed type for the memtable tests — kv64Z compares keys only,
// so keep-last dedup and younger-wins ties are observable in the output.
#define X(M, name) M##kv64##name
#include "HITx.h"
#undef X

// --- MEM-043: PUP error-path leaks --------------------------------
//
//  REPRO-FIRST (CLAUDE.md §17).  Two leaks live in the puppy-stack
//  helpers, neither of which LSan catches (the leaked fd/mmap/DIR
//  handle is reachable from no live pointer but its kernel resource
//  outlives the call).  We make them observable by counting open file
//  descriptors across many iterations: a leak grows the fd table
//  monotonically, a fixed function keeps it flat.
//
//    (1) pups-overflow — PUPOpenAll FILEMapRO+books a puppy file,
//        then pushes (key, fd) into the bounded `pups`.  When that push
//        overflows, the early return drops the just-mapped fd+mmap on
//        the floor (it was never recorded in `pups`, so PUPClose
//        cannot reclaim it).
//    (2) non-END FILENext — when the directory scan aborts with a real
//        error (PATHu8bPush BNOROOM on a path that overflows the 1024-
//        byte path buffer), `seen(END)` short-circuits the return and
//        SKIPS the following FILEIterClose, leaking the DIR handle.

//  Portable live open-fd count: probe with fcntl(F_GETFD).  Works on
//  Linux, FreeBSD (including linuxulator where /proc/self/fd is stubby)
//  and macOS — no /proc dependency.
static int dog_open_fd_count(void) {
    long max_fd = sysconf(_SC_OPEN_MAX);
    if (max_fd <= 0 || max_fd > 4096) max_fd = 4096;
    int n = 0;
    for (int fd = 0; fd < (int)max_fd; fd++) {
        if (fcntl(fd, F_GETFD) != -1) n++;
    }
    return n;
}

//  (1) Force a pups-overflow and assert no fd/mmap leak.
//
//  Create N puppy files in a scratch dir, then repeatedly re-open them
//  into a `pups` buffer too small to hold them all (cap < N): each
//  PUPOpenAll maps every file in key order and pushes until the
//  buffer fills — the (cap+1)-th FILEMapRO is the one whose fd+mmap
//  leaks on the failed push.  Before the fix the fd table grows by
//  (N-cap) per iteration; after the fix it is flat.
static ok64 PUPTestOverflowLeak(void) {
    sane(1);
    call(FILEInit);

    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);

    a_cstr(ext, ".test.idx");
    enum { NPUP = 8, PUPCAP = 3, ITERS = 32 };

    //  Materialize NPUP puppy files on disk via PUPCreate (roomy pups).
    {
        a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
        Bpup256 mk = {};
        call(pup256bAllocate, mk, NPUP + 2);
        for (int i = 0; i < NPUP; i++) {
            a_cstr(body, "puppy-bytes");
            a_dup(u8c, b, body);
            //  distinct explicit keys so all NPUP files coexist
            try(PUPCreateAt, mk, $path(dir), ext, b, (u64)(1000 + i));
            if (__ != OK) { PUPClose(mk); want(0); }
        }
        PUPClose(mk);  //  unmaps all, frees mk — disk files remain
    }

    int fd0 = dog_open_fd_count();
    want(fd0 > 0);

    for (int it = 0; it < ITERS; it++) {
        a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
        Bpup256 pups = {};
        call(pup256bAllocate, pups, PUPCAP);  //  too small for NPUP
        try(PUPOpenAll, pups, $path(dir), ext);
        //  overflow → SNOROOM expected; the leak (if any) already happened
        b8 overflowed = (__ == SNOROOM);
        PUPClose(pups);
        if (!overflowed) {        //  repro precondition not met
            TESTBErmrf(tmp);
            fprintf(stderr, "PupOverflowLeak: no overflow on iter %d\n", it);
            fail(TESTFAIL);
        }
    }

    int fd1 = dog_open_fd_count();
    TESTBErmrf(tmp);
    if (fd1 > fd0) {
        fprintf(stderr,
            "PupOverflowLeak: fd count grew %d -> %d across %d iters "
            "(leaked ~%d fds/iter)\n",
            fd0, fd1, ITERS, (fd1 - fd0) / ITERS);
        fail(TESTFAIL);
    }
    __ = OK;  //  the expected overflow status must not leak out as failure
    done;
}

//  (2) Force a non-END FILENext error and assert no DIR-handle leak.
//
//  PUPOpenAll's FILEIter shares a 1024-byte path buffer seeded with
//  `dir`.  Make `dir` long enough that appending ANY entry name
//  overflows the buffer: PATHu8bPush then returns BNOROOM (a non-END,
//  non-PATHBAD error), the scan aborts, and the pre-fix `seen(END)`
//  returns BEFORE FILEIterClose — leaking the opendir() handle.  One
//  leaked DIR fd per iteration; the loop makes it unmistakable.
static ok64 PUPTestIterErrLeak(void) {
    sane(1);
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__APPLE__)
    //  This repro needs OS PATH_MAX > FILE_PATH_MAX_LEN (1024) so the
    //  on-disk file can be created while the iterator's bounded push
    //  still overflows.  FreeBSD's, OpenBSD's and Darwin's kernel
    //  MAXPATHLEN is 1024, so the precondition cannot be reached —
    //  open(ENAMETOOLONG)s before the scan ever observes an entry.  The
    //  leak fix itself is platform-agnostic and PupOverflowLeak above still exercises the
    //  close path.
    done;
#endif
    call(FILEInit);

    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);

    //  Build a deeply-nested dir whose path is within a few bytes of
    //  FILE_PATH_MAX_LEN (1024), so any entry name push overflows.
    char deep[1100];
    int dlen = snprintf(deep, sizeof deep, "%s", tmp);
    //  255-byte segments (NAME_MAX) until we are ~within an entry-name of cap.
    char seg[256];
    memset(seg, 'a', 200);
    seg[200] = 0;
    while (dlen + 1 + 200 < 1015) {
        char path[1100];
        snprintf(path, sizeof path, "%s/%s", deep, seg);
        if (mkdir(path, 0755) != 0) { TESTBErmrf(tmp); want(0); }
        dlen = snprintf(deep, sizeof deep, "%s", path);
    }
    //  Put a regular file inside so the scan visits an entry and the
    //  push (deep-path + filename) overflows the 1024 buffer.
    {
        char fpath[1200];
        snprintf(fpath, sizeof fpath, "%s/%s", deep, seg);
        int fd = open(fpath, O_CREAT | O_WRONLY, 0644);
        if (fd >= 0) close(fd);
    }

    a_cstr(ext, ".test.idx");
    enum { ITERS = 64 };

    int fd0 = dog_open_fd_count();
    want(fd0 > 0);

    for (int it = 0; it < ITERS; it++) {
        a_path(dir, ((u8cs){(u8 *)deep, (u8 *)deep + strlen(deep)}));
        Bpup256 pups = {};
        call(pup256bAllocate, pups, 16);
        try(PUPOpenAll, pups, $path(dir), ext);
        //  expect a non-END iterator error (PATHNOROOM) to have aborted
        //  the scan; OK/END would mean the precondition wasn't met
        b8 iter_erred = (__ != OK);
        PUPClose(pups);
        if (!iter_erred) {
            TESTBErmrf(tmp);
            fprintf(stderr, "PupIterErrLeak: no iter error on iter %d\n", it);
            fail(TESTFAIL);
        }
    }

    int fd1 = dog_open_fd_count();
    TESTBErmrf(tmp);
    if (fd1 > fd0) {
        fprintf(stderr,
            "PupIterErrLeak: fd count grew %d -> %d across %d iters "
            "(leaked DIR handles)\n",
            fd0, fd1, ITERS);
        fail(TESTFAIL);
    }
    __ = OK;  //  the expected iterator error must not leak out as failure
    done;
}

ok64 PUPTestLeaks(void) {
    sane(1);
    call(PUPTestOverflowLeak);
    call(PUPTestIterErrLeak);
    done;
}

// --- DOG-027: Pup memtable — put / sync / collapse / flush / commit ---

// The typed sort/merge and entry points come from the template.  kv64Z compares
// keys only; on equal keys the youngest row wins (BOX sort, HIT merge).
//  PUPS-001: a row whose val is all ones is a tombstone.
#define PUP_TOMB (~0ULL)
#define PUPx_DEAD(r) ((r)->val == PUP_TOMB)
#define X(M, name) M##kv64##name
#include "PUPx.h"
#undef X

//  PUPS-26A276Xh2e: each handle's memtable runs, held by the test next to its
//  pups (keyed by the handle's address): KR typed, TR as the core's u8csb.
#define T_HANDLES 32
static struct {
    void const *pups;
    kv64cs slots[ABC_HIT_MAX_RUNS];
    kv64cs *hdr[4];
} t_runs[T_HANDLES];
static kv64cs *const *KR(pup256b pups) {
    int free_at = -1;
    for (int i = 0; i < T_HANDLES; i++) {
        if (t_runs[i].pups == (void const *)pups) return t_runs[i].hdr;
        if (free_at < 0 && t_runs[i].pups == NULL) free_at = i;
    }
    if (free_at < 0) return NULL;
    t_runs[free_at].pups = (void const *)pups;
    kv64cs **h = t_runs[free_at].hdr;
    h[0] = h[1] = h[2] = t_runs[free_at].slots;
    h[3] = t_runs[free_at].slots + ABC_HIT_MAX_RUNS;
    return t_runs[free_at].hdr;
}
#define TR(p) ((u8cs **)KR(p))

// The core takes (sync, merge, row).  T_PLAIN: no ToC; T_TOC: row-width ToC.
#define T_PLAIN PUPkv64Sync, PUPkv64Merge, 0
#define T_TOC PUPkv64Sync, PUPkv64Merge, (u32)sizeof(kv64)

// DOG-027: put one (key,val) row through PUPPut.
static ok64 dog_mem_put(pup256b pups, path8s dir, u8cs ext, u64 key, u64 val) {
    sane(1);
    kv64 row = {key, val};
    u8cs rec = {(u8c *)&row, (u8c *)(&row + 1)};
    call(PUPPut, pups, TR(pups), dir, ext, rec, T_PLAIN);
    done;
}

// DOG-027: query the stack — AllRuns sources fed to HITkv64Merge; merged
// rows land in `out` (head advances), source count in *nsrc.
// DOG-035: `row` picks the width, so one helper serves both twins.
static ok64 pup_query(kv64s out, u32 *nsrc, pup256b pups, u32 row) {
    sane(out != NULL && nsrc != NULL);
    Bu8cs srcs = {};
    call(u8csbAllocate, srcs, ABC_HIT_MAX_RUNS);
    try(PUPAllRuns, srcs, pups, TR(pups), PUPkv64Sync, row);
    nedo { u8csbFree(srcs); fail(__); }
    u32 n = (u32)u8csbDataLen(srcs);
    *nsrc = n;
    kv64cs runs[ABC_HIT_MAX_RUNS];
    u8cs *sl = u8csbDataHead(srcs);
    for (u32 i = 0; i < n; i++) {
        runs[i][0] = (kv64 const *)sl[i][0];
        runs[i][1] = (kv64 const *)sl[i][1];
    }
    kv64css hp = {runs, runs + n};
    try(HITkv64Merge, out, hp);
    u8csbFree(srcs);
    done;
}

static ok64 dog_mem_query(kv64s out, u32 *nsrc, pup256b pups) {
    return pup_query(out, nsrc, pups, 0);
}

// DOG-027: round-trip — unsorted arrivals, keep-last dup; a second read
// sees the same rows (sync idempotent), memtable keyed ALL-ONES.
static ok64 PUPTestMemRoundTrip(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_cstr(ext, ".mem.idx");
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    Bpup256 pups = {};
    call(pup256bAllocate, pups, 8);

    // DOG-027: no sync passed down — the memtable API refuses, never crashes
    kv64 pre = {1, 1};
    u8cs prerec = {(u8c *)&pre, (u8c *)(&pre + 1)};
    want(PUPPut(pups, TR(pups), $path(dir), ext, prerec, NULL, NULL, 0) == PUPBADARG);

    call(dog_mem_put, pups, $path(dir), ext, 7, 70);
    call(dog_mem_put, pups, $path(dir), ext, 3, 30);
    call(dog_mem_put, pups, $path(dir), ext, 5, 50);
    call(dog_mem_put, pups, $path(dir), ext, 3, 31);

    kv64 got[16];
    kv64s out = {got, got + 16};
    u32 nsrc = 0;
    call(dog_mem_query, out, &nsrc, pups);
    testeqv((long long)(out[0] - got), 3LL, "%lld");
    testeqv((long long)nsrc, 1LL, "%lld");
    want(got[0].key == 3 && got[0].val == 31);
    want(got[1].key == 5 && got[1].val == 50);
    want(got[2].key == 7 && got[2].val == 70);

    kv64 got2[16];
    kv64s out2 = {got2, got2 + 16};
    call(dog_mem_query, out2, &nsrc, pups);
    testeqv((long long)(out2[0] - got2), 3LL, "%lld");
    want(got2[0].val == 31 && got2[1].val == 50 && got2[2].val == 70);

    want(PUPCount(pups) == 0);  //  reserved entries are not runs
    want(PUPSeqno(pups, (u32)pup256bDataLen(pups) - 1) == PUP_MEM_KEY);
    want(PUPSeqno(pups, 99) == 0);

    call(PUPClose, pups);
    TESTBErmrf(tmp);
    done;
}

// DOG-027: DATA past 512 collapses to ONE slice on read; smaller DATA
// reads as TWO slices (PAST+DATA); a PAST/DATA key tie goes to DATA.
static ok64 PUPTestMemCollapse(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_cstr(ext, ".mem.idx");
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    Bpup256 pups = {};
    call(pup256bAllocate, pups, 8);

    // DOG-027: 40 rows = 640 B > PUP_COLLAPSE — first read collapses
    for (u64 i = 0; i < 40; i++)
        call(dog_mem_put, pups, $path(dir), ext, 1000 - i, i);
    kv64 got[64];
    kv64s out = {got, got + 64};
    u32 nsrc = 0;
    call(dog_mem_query, out, &nsrc, pups);
    testeqv((long long)nsrc, 1LL, "%lld");
    testeqv((long long)(out[0] - got), 40LL, "%lld");
    for (int i = 1; i < 40; i++) want(got[i - 1].key < got[i].key);

    call(dog_mem_put, pups, $path(dir), ext, 5000, 1);
    call(dog_mem_put, pups, $path(dir), ext, 970, 777);
    kv64 got2[64];
    kv64s out2 = {got2, got2 + 64};
    call(dog_mem_query, out2, &nsrc, pups);
    testeqv((long long)nsrc, 2LL, "%lld");
    testeqv((long long)(out2[0] - got2), 41LL, "%lld");
    b8 hit = NO;
    for (kv64 *p = got2; p < out2[0]; p++)
        if (p->key == 970) { hit = YES; want(p->val == 777); }
    want(hit);

    // DOG-027: 32 B DATA stays two-slice — reads alone never re-collapse
    kv64 got3[64];
    kv64s out3 = {got3, got3 + 64};
    call(dog_mem_query, out3, &nsrc, pups);
    testeqv((long long)nsrc, 2LL, "%lld");

    call(PUPClose, pups);
    TESTBErmrf(tmp);
    done;
}

// DOG-027: a full one-page buffer flushes into a run under a fresh monotonic
// pup_key; a new `<ext>` memtable takes over and the run is scan-visible.
// DOG-032:1054 the page is the OS's, so the fill count is measured, not 256.
static ok64 PUPTestMemFlush(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_cstr(ext, ".mem.idx");
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    u64 fill = FILESysPage() / sizeof(kv64);
    Bpup256 pups = {};
    call(pup256bAllocate, pups, 8);

    for (u64 i = 0; i < fill; i++)
        call(dog_mem_put, pups, $path(dir), ext, 10000 - i, i);
    want(PUPCount(pups) == 0);
    call(dog_mem_put, pups, $path(dir), ext, 42, 4242);
    want(PUPCount(pups) == 1);
    u64 runkey = PUPSeqno(pups, 0);
    want(runkey != 0 && runkey != PUP_MEM_KEY);
    want(PUPSeqno(pups, (u32)pup256bDataLen(pups) - 1) == PUP_MEM_KEY);

    // DOG-027: fresh scan sees the run, never the dot-file
    Bpup256 fresh = {};
    call(pup256bAllocate, fresh, 8);
    call(PUPOpenAll, fresh, $path(dir), ext);
    want(PUPCount(fresh) == 1);
    want(PUPSeqno(fresh, 0) == runkey);
    u8cs run = {};
    PUPData(run, fresh, 0);
    testeqv((long long)(run[1] - run[0]), (long long)(fill * sizeof(kv64)),
            "%lld");
    kv64 const *rr = (kv64 const *)run[0];
    for (u64 i = 1; i < fill; i++) want(rr[i - 1].key < rr[i].key);
    call(PUPClose, fresh);

    Bkv64 gotb = {};
    call(kv64bAllocate, gotb, (u32)fill + 64);
    kv64 *got = kv64bIdleHead(gotb);
    kv64s out = {got, kv64bTerm(gotb)};
    u32 nsrc = 0;
    try(dog_mem_query, out, &nsrc, pups);
    nedo { kv64bFree(gotb); fail(__); }
    testeqv((long long)nsrc, 2LL, "%lld");
    testeqv((long long)(out[0] - got), (long long)(fill + 1), "%lld");
    b8 hit = NO;
    for (kv64 *p = got; p < out[0]; p++)
        if (p->key == 42) { hit = YES; want(p->val == 4242); }
    want(hit);
    kv64bFree(gotb);

    // DOG-027: ThinTail drops the RUN, never the reserved tail — and the
    // hook survives the slide, so the memtable still takes puts.
    call(PUPThinTail, pups, $path(dir), ext, 1);
    want(PUPCount(pups) == 0);
    want(PUPSeqno(pups, (u32)pup256bDataLen(pups) - 1) == PUP_MEM_KEY);
    call(dog_mem_put, pups, $path(dir), ext, 9, 99);

    call(PUPClose, pups);
    TESTBErmrf(tmp);
    done;
}

// DOG-027: commit collapses, trims and renames `<ext>` into
// `<10-RON64><ext>`; the dot-file is gone, the run is complete.
static ok64 PUPTestMemCommit(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_cstr(ext, ".mem.idx");
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    Bpup256 pups = {};
    call(pup256bAllocate, pups, 8);

    call(dog_mem_put, pups, $path(dir), ext, 2, 20);
    call(dog_mem_put, pups, $path(dir), ext, 1, 10);
    call(dog_mem_put, pups, $path(dir), ext, 1, 11);
    call(PUPCommit, pups, TR(pups), $path(dir), ext, T_PLAIN);

    want(PUPCount(pups) == 1);
    u64 runkey = PUPSeqno(pups, 0);
    want(runkey != 0 && runkey != PUP_MEM_KEY);

    char mp[300];
    snprintf(mp, sizeof mp, "%s/%s", tmp, ".mem.idx");
    struct stat st = {};
    want(stat(mp, &st) != 0);

    Bpup256 fresh = {};
    call(pup256bAllocate, fresh, 8);
    call(PUPOpenAll, fresh, $path(dir), ext);
    want(PUPCount(fresh) == 1);
    u8cs run = {};
    PUPData(run, fresh, 0);
    testeqv((long long)(run[1] - run[0]), (long long)(2 * sizeof(kv64)),
            "%lld");
    kv64 const *rr = (kv64 const *)run[0];
    want(rr[0].key == 1 && rr[0].val == 11);
    want(rr[1].key == 2 && rr[1].val == 20);
    call(PUPClose, fresh);

    call(PUPClose, pups);
    TESTBErmrf(tmp);
    done;
}

// DOG-027: a crashed `<ext>` memtable (raw dot-file of unsorted bytes) is
// invisible to PUPOpenAll — only real `<10-RON64><ext>` runs load.
static ok64 PUPTestMemCrashInvisible(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_cstr(ext, ".mem.idx");
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));

    {
        Bpup256 mk = {};
        call(pup256bAllocate, mk, 4);
        a_cstr(body, "sixteen-byte-row");
        a_dup(u8c, b, body);
        try(PUPCreateAt, mk, $path(dir), ext, b, (u64)1000);
        nedo { PUPClose(mk); want(0); }
        call(PUPClose, mk);
    }
    {
        char mp[300];
        snprintf(mp, sizeof mp, "%s/%s", tmp, ".mem.idx");
        int fd = open(mp, O_CREAT | O_WRONLY, 0600);
        want(fd >= 0);
        want(write(fd, "garbage unsorted crash bytes", 28) == 28);
        close(fd);
    }
    //  DOG-027: and a run named the way the retired JS writer named them —
    //  8 zero-padded chars.  It is not a run to the 10-char scan, so the open
    //  IGNORES it; an open never deletes what it cannot read.
    char legacy[300];
    snprintf(legacy, sizeof legacy, "%s/%s", tmp, "00000001.mem.idx");
    {
        int fd = open(legacy, O_CREAT | O_WRONLY, 0600);
        want(fd >= 0);
        want(write(fd, "sixteen-byte-row", 16) == 16);
        close(fd);
    }

    Bpup256 pups = {};
    call(pup256bAllocate, pups, 8);
    call(PUPOpenAll, pups, $path(dir), ext);
    want(PUPCount(pups) == 1);
    want(PUPSeqno(pups, 0) == 1000);
    struct stat lst = {};
    want(stat(legacy, &lst) == 0);  //  the legacy run is left on disk
    call(PUPClose, pups);
    TESTBErmrf(tmp);
    done;
}

// DOG-027: several page-full flushes drive the ladder in C — the on-disk
// stack keeps the 1/8 invariant and queries still answer newest-wins.
static ok64 PUPTestLadder(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_cstr(ext, ".mem.idx");
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    Bpup256 pups = {};
    call(pup256bAllocate, pups, 64);

    //  2048 distinct keys = 8 full pages; every seal ends in the ladder
    for (u64 i = 0; i < 2048; i++)
        call(dog_mem_put, pups, $path(dir), ext, 100000 - i, i);
    //  the 2049th put seals page 8, then lands in a fresh memtable and
    //  shadows a key that already sits in the committed run
    call(dog_mem_put, pups, $path(dir), ext, 99999, 7777);
    call(PUPCommit, pups, TR(pups), $path(dir), ext, T_PLAIN);

    //  the ladder collapsed the 8 pages into ONE run; the 16-byte tail run
    //  is under a 1/8 of it, so the stack is compact and stays two deep
    u32 n = PUPCount(pups);
    testeqv((long long)n, 2LL, "%lld");
    for (u32 i = 0; i + 1 < n; i++) {
        u8cs a = {}, b = {};
        PUPData(a, pups, i);
        PUPData(b, pups, i + 1);
        want(u8csLen(b) * ABC_HIT_LADDER_DIV <= u8csLen(a));
    }
    u8cs big = {};
    PUPData(big, pups, 0);
    testeqv((long long)u8csLen(big), (long long)(2048 * sizeof(kv64)), "%lld");

    //  a fresh scan sees exactly those two files — the collapsed sources
    //  were unlinked, and no dot-file survived
    Bpup256 fresh = {};
    call(pup256bAllocate, fresh, 64);
    call(PUPOpenAll, fresh, $path(dir), ext);
    testeqv((long long)PUPCount(fresh), 2LL, "%lld");
    call(PUPClose, fresh);

    Bkv64 got = {};
    call(kv64bAllocate, got, 4096);
    kv64 *gb = kv64bIdleHead(got);
    kv64s out = {gb, kv64bTerm(got)};
    u32 nsrc = 0;
    try(dog_mem_query, out, &nsrc, pups);
    nedo { kv64bFree(got); PUPClose(pups); fail(__); }
    testeqv((long long)(out[0] - gb), 2048LL, "%lld");
    for (kv64 *p = gb + 1; p < out[0]; p++) want(p[-1].key < p->key);
    b8 hit = NO;
    for (kv64 *p = gb; p < out[0]; p++)
        if (p->key == 99999) { hit = YES; want(p->val == 7777); }
    want(hit);
    kv64bFree(got);

    call(PUPClose, pups);
    TESTBErmrf(tmp);
    done;
}

// PUPS-004: a ladder merge's key stays next to its YOUNGEST source's key, so a
// key read as tx time never jumps past writes that landed meanwhile.
// PUPS-005: youngest + 1 — a run name is never reused.
static ok64 PUPTestLadderKey(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_cstr(ext, ".key.idx");
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    Bpup256 pups = {};
    call(pup256bAllocate, pups, 16);
    //  three equal-size runs violate the 1/8 ladder by construction
    for (u64 r = 0; r < 3; r++) {
        kv64 row = {10 + r, 100 + r};
        a_dup(u8c, rec, ((u8cs){(u8c *)&row, (u8c *)(&row + 1)}));
        call(PUPCreateAt, pups, $path(dir), ext, rec, 1000 + r);
    }
    call(PUPLadder, pups, $path(dir), ext, PUPkv64Merge, 0);
    testeqv((long long)PUPCount(pups), 1LL, "%lld");
    testeqv((long long)PUPSeqno(pups, 0), 1003LL, "%lld");
    //  a fresh scan agrees: one file, named youngest + 1
    Bpup256 fresh = {};
    call(pup256bAllocate, fresh, 16);
    call(PUPOpenAll, fresh, $path(dir), ext);
    testeqv((long long)PUPCount(fresh), 1LL, "%lld");
    testeqv((long long)PUPSeqno(fresh, 0), 1003LL, "%lld");
    call(PUPClose, fresh);
    call(PUPClose, pups);
    TESTBErmrf(tmp);
    done;
}


//  PUPS-001: book a sorted run straight, bypassing the ladder.
static ok64 tomb_run(pup256b pups, path8s dir, u8cs ext, u64 key, u64 from,
                     u64 n, u64 val) {
    sane(1);
    kv64 rows[32];
    for (u64 i = 0; i < n; i++) rows[i] = (kv64){from + i, val ? val : (from + i) * 10};
    u8cs rec = {(u8c *)rows, (u8c *)(rows + n)};
    call(PUPCreateAt, pups, dir, ext, rec, key);
    done;
}

//  PUPS-001: a tombstone hides its key from reads at once, a partial ladder
//  merge keeps it (an older run may hold the key), the merge that spans the
//  whole stack drops it and the rows it shadowed.
static ok64 PUPTestTomb(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_cstr(ext, ".tomb.idx");
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    Bpup256 pups = {};
    call(pup256bAllocate, pups, 16);
    kv64 out[64];
    kv64s o;

    call(tomb_run, pups, $path(dir), ext, 1000, 1, 20, 0);  //  keys 1..20
    call(tomb_run, pups, $path(dir), ext, 1001, 2, 1, PUP_TOMB);  //  delete 2
    call(PUPLadder, pups, $path(dir), ext, PUPkv64Merge, 0);
    testeqv((long long)PUPCount(pups), 2LL, "%lld");  //  1/20 < 1/8: no merge
    o[0] = out; o[1] = out + 64;
    call(PUPkv64Read, o, pups, KR(pups));
    testeqv((long long)(o[0] - out), 19LL, "%lld");  //  key 2 is hidden
    for (kv64 *r = out; r < o[0]; r++) want(r->key != 2);

    //  a small put: the youngest two runs merge, the stack is NOT spanned
    call(tomb_run, pups, $path(dir), ext, 1002, 30, 1, 0);
    call(PUPLadder, pups, $path(dir), ext, PUPkv64Merge, 0);
    testeqv((long long)PUPCount(pups), 2LL, "%lld");
    u8cs sl;
    PUPDataRow(sl, pups, 1, 0);
    testeqv((long long)(u8csLen(sl) / sizeof(kv64)), 2LL, "%lld");  //  tomb kept
    o[0] = out; o[1] = out + 64;
    call(PUPkv64Read, o, pups, KR(pups));
    testeqv((long long)(o[0] - out), 20LL, "%lld");

    //  a bigger put reaches the oldest run: tomb + shadowed row go
    call(tomb_run, pups, $path(dir), ext, 1010, 31, 3, 0);
    call(PUPLadder, pups, $path(dir), ext, PUPkv64Merge, 0);
    testeqv((long long)PUPCount(pups), 1LL, "%lld");
    PUPDataRow(sl, pups, 0, 0);
    testeqv((long long)(u8csLen(sl) / sizeof(kv64)), 23LL, "%lld");
    for (kv64 const *r = (kv64 const *)sl[0]; r < (kv64 const *)sl[1]; r++)
        want(r->val != PUP_TOMB && r->key != 2);
    o[0] = out; o[1] = out + 64;
    call(PUPkv64Read, o, pups, KR(pups));
    testeqv((long long)(o[0] - out), 23LL, "%lld");
    call(PUPClose, pups);

    //  everything deleted: the merge leaves nothing, reads see nothing
    Bpup256 p2 = {};
    call(pup256bAllocate, p2, 16);
    a_cstr(ext2, ".gone.idx");
    call(tomb_run, p2, $path(dir), ext2, 2000, 5, 1, 50);
    call(tomb_run, p2, $path(dir), ext2, 2001, 5, 1, PUP_TOMB);
    call(PUPLadder, p2, $path(dir), ext2, PUPkv64Merge, 0);
    o[0] = out; o[1] = out + 64;
    call(PUPkv64Read, o, p2, KR(p2));
    testeqv((long long)(o[0] - out), 0LL, "%lld");
    PUPDataRow(sl, p2, 0, 0);
    testeqv((long long)u8csLen(sl), 0LL, "%lld");
    call(PUPClose, p2);
    TESTBErmrf(tmp);
    done;
}

//  PUPS-005: crash points for a ladder merge.  The test binary interposes
//  `rename` and `unlink`: once armed, the `pup_crash_at`-th successful one on a
//  `pup_crash_ext` run kills the process right after it, as a crash would.
static int pup_crash_at = 0, pup_crash_ops = 0;
static char const *pup_crash_ext = "";
#define PUP_CRASHED 77

static void pup_crash_tick(char const *path) {
    if (pup_crash_at == 0) return;
    size_t l = strlen(path), e = strlen(pup_crash_ext);
    if (l < e || strcmp(path + l - e, pup_crash_ext) != 0) return;
    if (++pup_crash_ops == pup_crash_at) _exit(PUP_CRASHED);
}

int unlink(char const *path) {
    int rc = unlinkat(AT_FDCWD, path, 0);
    if (rc == 0) pup_crash_tick(path);
    return rc;
}

int rename(char const *from, char const *to) {
    int rc = renameat(AT_FDCWD, from, AT_FDCWD, to);
    if (rc == 0) pup_crash_tick(to);
    return rc;
}

//  PUPS-005: three equal runs (the ladder merges them all, a bottom merge that
//  drops tombstones): rows 1..4, then deletes of 2 and 3 plus a new 4 and 5,
//  then 6..9.  `gone` deletes everything instead: the merge output is empty.
static ok64 crash_stack(path8s dir, u8cs ext, b8 gone) {
    sane(1);
    Bpup256 pups = {};
    call(pup256bAllocate, pups, 16);
    kv64 runs[3][4] = {
        {{1, 10}, {2, 20}, {3, 30}, {4, 40}},
        {{2, PUP_TOMB}, {3, PUP_TOMB}, {4, 41}, {5, 50}},
        {{6, 60}, {7, 70}, {8, 80}, {9, 90}},
    };
    kv64 dead[2][4] = {
        {{1, 10}, {2, 20}, {3, 30}, {4, 40}},
        {{1, PUP_TOMB}, {2, PUP_TOMB}, {3, PUP_TOMB}, {4, PUP_TOMB}},
    };
    u32 n = gone ? 2 : 3;
    for (u32 r = 0; r < n; r++) {
        kv64 *rows = gone ? dead[r] : runs[r];
        u8cs rec = {(u8c *)rows, (u8c *)(rows + 4)};
        call(PUPCreateAt, pups, dir, ext, rec, 1000 + r);
    }
    call(PUPClose, pups);
    done;
}

//  PUPS-005: a fresh handle's read of the stack in `dir`.
static ok64 crash_read(kv64s out, u32 *nruns, path8s dir, u8cs ext) {
    sane(1);
    Bpup256 pups = {};
    call(pup256bAllocate, pups, 16);
    call(PUPOpenAll, pups, dir, ext);
    *nruns = PUPCount(pups);
    call(PUPkv64Read, out, pups, KR(pups));
    call(PUPClose, pups);
    done;
}

//  PUPS-005: a ladder merge killed after its rename and after each unlink
//  leaves a directory that reads exactly as before the merge — deleted rows
//  stay deleted.  The pre-PUPS-005 order (rename onto the youngest source's
//  name, unlink youngest-first) brought deleted rows back.
static ok64 PUPTestCrashLadder(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    char extc[16];
    for (int gone = 0; gone < 2; gone++) {
        //  the view before the merge, from an untouched copy of the stack
        snprintf(extc, sizeof extc, ".r%d.idx", gone);
        a_cstr(rext, extc);
        call(crash_stack, $path(dir), rext, gone);
        kv64 want_rows[16], got[16];
        kv64s w = {want_rows, want_rows + 16};
        u32 nr = 0;
        call(crash_read, w, &nr, $path(dir), rext);
        size_t nwant = (size_t)(w[0] - want_rows);
        testeqv((long long)nwant, gone ? 0LL : 7LL, "%lld");
        //  crash after step k: the rename, then each unlink (k = 9: never)
        int steps = gone ? 2 : 4;
        for (int k = 1; k <= steps + 1; k++) {
            int at = k > steps ? 9 : k;
            snprintf(extc, sizeof extc, ".c%d%d.idx", gone, k);
            a_cstr(ext, extc);
            call(crash_stack, $path(dir), ext, gone);
            pid_t pid = fork();
            want(pid >= 0);
            if (pid == 0) {
                Bpup256 pups = {};
                pup_crash_ext = extc;
                pup_crash_at = at;
                if (pup256bAllocate(pups, 16) != OK) _exit(1);
                if (PUPOpenAll(pups, $path(dir), ext) != OK) _exit(2);
                if (PUPLadder(pups, $path(dir), ext, PUPkv64Merge, 0) != OK)
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

//  Write lock: forked writers share one stack.  PUPCreate picks keys from the
//  clock, so writers in one millisecond reach for the same name; commits make
//  the ladder merge all the time.  Without the lock a run is eaten by a rename
//  or a row lost in a double collapse; with it every run and every row stays.
//  A handle keeps the mappings of runs other writers unlinked (DOG-046), and a
//  stack view holds ABC_HIT_MAX_RUNS: the commit loop stays under that, the create
//  loop (no ladder, no view) does not need to.
#define LOCK_KIDS 4
#define LOCK_N 150
#define LOCK_COMMITS 12
static ok64 PUPTestLock(void) {
    sane(1);
    call(FILEInit);
    //  PUPOpenAll below maps all LOCK_KIDS*LOCK_N runs, an fd each: more
    //  than OpenBSD's (512) or macOS's (256) default soft RLIMIT_NOFILE.
    struct rlimit rl = {};
    want(getrlimit(RLIMIT_NOFILE, &rl) == 0);
    if (rl.rlim_cur != RLIM_INFINITY && rl.rlim_cur < 1024) {
        rl.rlim_cur = rl.rlim_max < 1024 ? rl.rlim_max : 1024;
        want(setrlimit(RLIMIT_NOFILE, &rl) == 0);
    }
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_cstr(ext, ".lock.idx");
    a_cstr(ext2, ".lock2.idx");
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    pid_t kid[LOCK_KIDS];
    for (int c = 0; c < LOCK_KIDS; c++) {
        kid[c] = fork();
        want(kid[c] >= 0);
        if (kid[c] > 0) continue;
        int bad = 0;
        Bpup256 mine = {}, cm = {};
        if (pup256bAllocate(mine, 1024) != OK || pup256bAllocate(cm, 1024) != OK) _exit(2);
        for (int i = 0; i < LOCK_N; i++) {
            kv64 row = {(u64)c * 1000 + i, 7};
            u8cs rec = {(u8c *)&row, (u8c *)(&row + 1)};
            if (PUPCreate(mine, $path(dir), ext, rec) != OK) bad = 1;
            //  a second stack in the same directory, committed row by row
            if (i >= LOCK_COMMITS) continue;
            if (PUPPut(cm, TR(cm), $path(dir), ext2, rec, T_PLAIN) != OK) bad = 1;
            if (PUPCommit(cm, TR(cm), $path(dir), ext2, T_PLAIN) != OK) bad = 1;
        }
        _exit(bad);
    }
    for (int c = 0; c < LOCK_KIDS; c++) {
        int st = 0;
        want(waitpid(kid[c], &st, 0) == kid[c]);
        want(WIFEXITED(st) && WEXITSTATUS(st) == 0);
    }
    //  PUPCreate: every run is still there, none eaten by a rename
    Bpup256 a = {};
    call(pup256bAllocate, a, 1024);
    call(PUPOpenAll, a, $path(dir), ext);
    testeqv((long long)PUPCount(a), (long long)LOCK_KIDS * LOCK_N, "%lld");
    call(PUPClose, a);
    //  PUPCommit + ladder: every row of every writer survives the merges
    Bpup256 b = {};
    call(pup256bAllocate, b, 1024);
    call(PUPOpenAll, b, $path(dir), ext2);
    kv64 *out = malloc(LOCK_KIDS * LOCK_COMMITS * sizeof(kv64));
    want(out != NULL);
    kv64s o = {out, out + LOCK_KIDS * LOCK_COMMITS};
    call(PUPkv64Read, o, b, KR(b));
    testeqv((long long)(o[0] - out), (long long)LOCK_KIDS * LOCK_COMMITS, "%lld");
    free(out);
    call(PUPClose, b);
    //  the lock files are not runs, and they stay out of a rescan
    Bpup256 c2 = {};
    call(pup256bAllocate, c2, 1024);
    call(PUPOpenAll, c2, $path(dir), ext2);
    want(PUPCount(c2) < LOCK_KIDS * LOCK_COMMITS);
    call(PUPClose, c2);
    TESTBErmrf(tmp);
    done;
}

// DOG-027: drop the i-th run — the file goes, the dict closes the gap, and
// the reserved tail stays put.
static ok64 PUPTestDropAt(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_cstr(ext, ".mem.idx");
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    Bpup256 pups = {};
    call(pup256bAllocate, pups, 16);

    //  three runs booked straight (a commit each would collapse them: three
    //  equal-size runs violate the 1/8 ladder by construction)
    for (u64 r = 0; r < 3; r++) {
        kv64 row = {10 + r, 100 + r};
        a_dup(u8c, rec, ((u8cs){(u8c *)&row, (u8c *)(&row + 1)}));
        call(PUPCreateAt, pups, $path(dir), ext, rec, 1000 + r);
    }
    testeqv((long long)PUPCount(pups), 3LL, "%lld");
    u64 mid = PUPSeqno(pups, 1), young = PUPSeqno(pups, 2);

    call(PUPDropAt, pups, $path(dir), ext, 1);
    testeqv((long long)PUPCount(pups), 2LL, "%lld");
    want(PUPSeqno(pups, 1) == young);
    want(PUPCount(pups) == (u32)pup256bDataLen(pups));  //  no memtable
    want(PUPDropAt(pups, $path(dir), ext, 9) == PUPBADARG);

    //  gone from disk too
    Bpup256 fresh = {};
    call(pup256bAllocate, fresh, 16);
    call(PUPOpenAll, fresh, $path(dir), ext);
    testeqv((long long)PUPCount(fresh), 2LL, "%lld");
    want(PUPSeqno(fresh, 0) != mid && PUPSeqno(fresh, 1) != mid);
    call(PUPClose, fresh);

    kv64 got[16];
    kv64s out = {got, got + 16};
    u32 nsrc = 0;
    call(dog_mem_query, out, &nsrc, pups);
    testeqv((long long)(out[0] - got), 2LL, "%lld");
    want(got[0].key == 10 && got[1].key == 12);

    call(PUPClose, pups);
    TESTBErmrf(tmp);
    done;
}

// DOG-027: a key whose NAME is taken is stepped over, never written through.
// Case folding can't be staged on ext4 — squatting the exact name can.
static ok64 PUPTestNameTaken(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_cstr(ext, ".take.idx");
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));

    //  A key above RONNow, so the next pick is exactly M+1 (DATA's max wins).
    u64 const M = 0x0FFFFFFFFFFFF00ULL;
    Bpup256 pups = {};
    call(pup256bAllocate, pups, 8);
    a_cstr(first, "sixteen-byte-row");
    a_dup(u8c, fb, first);
    call(PUPCreateAt, pups, $path(dir), ext, fb, M);

    //  Squat M+1's name with bytes of our own — the run dog is about to seal.
    a_pad(u8, name, PUP_SEQNO_W + 32);
    call(RONu8sFeedPad, u8bIdle(name), (ok64)(M + 1), PUP_SEQNO_W);
    call(u8bFeed, name, ext);
    a_path(squat, $path(dir), u8bDataC(name));
    int sfd = open((char *)u8bDataHead(squat), O_CREAT | O_WRONLY, 0600);
    want(sfd >= 0);
    want(write(sfd, "squatter--------", 16) == 16);
    close(sfd);

    //  The create must step to M+2 and leave the squatter's bytes alone.
    a_cstr(second, "another-16-bytes");
    a_dup(u8c, sb, second);
    call(PUPCreate, pups, $path(dir), ext, sb);
    want(PUPCount(pups) == 2);
    want(PUPSeqno(pups, 0) == M);
    want(PUPSeqno(pups, 1) == M + 2);
    {
        char back[17] = "";
        int rfd = open((char *)u8bDataHead(squat), O_RDONLY);
        want(rfd >= 0);
        want(read(rfd, back, 16) == 16);
        close(rfd);
        want(memcmp(back, "squatter--------", 16) == 0);
    }
    //  A caller-pinned key on a taken name is refused, not written through.
    want(PUPCreateAt(pups, $path(dir), ext, sb, M + 1) != OK);
    want(PUPCount(pups) == 2);

    call(PUPClose, pups);
    TESTBErmrf(tmp);
    done;
}

// DOG-046: two rw stacks on ONE dir.  The memtable used to be the fixed file
// `<dir>/<ext>`, opened O_TRUNC, so B's open wiped A's rows and A's seal then
// renamed B's inode away — every row must survive, both commits must succeed.
static ok64 PUPTestTwoWriters(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_cstr(ext, ".two.idx");
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    Bpup256 A = {}, B = {};
    call(pup256bAllocate, A, 16);
    call(pup256bAllocate, B, 16);

    //  DOG-046:two.js — A's rows, B's rows over the same dir, A's mark last.
    for (u64 i = 1; i <= 5; i++) call(dog_mem_put, A, $path(dir), ext, i, i);
    for (u64 i = 100; i <= 105; i++)
        call(dog_mem_put, B, $path(dir), ext, i, i);
    call(dog_mem_put, A, $path(dir), ext, 6, 6);

    //  DOG-046: two live memtables, each `~<10-RON64><ext>` — they still END
    //  in `<ext>`, so bee's sweep spares them, yet no run scan ever sees one.
    //  PUPS-26A276Xh2e: the memtable is no file any more: none on disk.
    {
        u32 mems = 0;
        DIR *dh = opendir(tmp);
        want(dh != NULL);
        for (struct dirent *e; (e = readdir(dh));) {
            size_t n = strlen(e->d_name);
            if (n < 8 || strcmp(e->d_name + n - 8, ".two.idx") != 0) continue;
            want(e->d_name[0] == '~');
            testeqv((long long)n, (long long)(PUP_SEQNO_W + 1 + 8), "%lld");
            mems++;
        }
        closedir(dh);
        testeqv((long long)mems, 0LL, "%lld");
        Bpup256 peek = {};
        call(pup256bAllocate, peek, 16);
        call(PUPOpenAll, peek, $path(dir), ext);
        testeqv((long long)PUPCount(peek), 0LL, "%lld");
        call(PUPClose, peek);
    }

    want(PUPCommit(A, TR(A), $path(dir), ext, T_PLAIN) == OK);
    want(PUPCommit(B, TR(B), $path(dir), ext, T_PLAIN) == OK);
    call(PUPClose, A);
    call(PUPClose, B);

    //  a fresh stack reads all 12 rows back, in key order
    Bpup256 fresh = {};
    call(pup256bAllocate, fresh, 16);
    call(PUPOpenAll, fresh, $path(dir), ext);
    kv64 got[32];
    kv64s out = {got, got + 32};
    u32 nsrc = 0;
    try(dog_mem_query, out, &nsrc, fresh);
    nedo { PUPClose(fresh); fail(__); }
    testeqv((long long)(out[0] - got), 12LL, "%lld");
    for (u64 i = 1; i <= 6; i++)
        want(got[i - 1].key == i && got[i - 1].val == i);
    for (u64 i = 100; i <= 105; i++)
        want(got[i - 94].key == i && got[i - 94].val == i);
    call(PUPClose, fresh);
    TESTBErmrf(tmp);
    done;
}

// DOG-046: a run sealed by ANOTHER handle while this one is open.  The dict is
// re-scanned before every seal and ladder, so A's commit maps B's run in and
// A's read view answers over both.
static ok64 PUPTestForeignRun(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_cstr(ext, ".fgn.idx");
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    Bpup256 A = {}, B = {};
    call(pup256bAllocate, A, 16);
    call(pup256bAllocate, B, 16);

    //  A holds rows in its memtable; B seals a run 8x bigger under it, so the
    //  1/8 ladder leaves BOTH runs standing and the count is the assertion.
    for (u64 i = 1; i <= 3; i++) call(dog_mem_put, A, $path(dir), ext, i, i);
    for (u64 i = 1000; i < 1064; i++)
        call(dog_mem_put, B, $path(dir), ext, i, i);
    want(PUPCommit(B, TR(B), $path(dir), ext, T_PLAIN) == OK);
    testeqv((long long)PUPCount(B), 1LL, "%lld");
    u64 bkey = PUPSeqno(B, 0);

    //  Before its own commit A knows nothing of B's run; after it, B's run
    //  is the OLDER of the two A carries.
    testeqv((long long)PUPCount(A), 0LL, "%lld");
    want(PUPCommit(A, TR(A), $path(dir), ext, T_PLAIN) == OK);
    testeqv((long long)PUPCount(A), 2LL, "%lld");
    want(PUPSeqno(A, 0) == bkey);
    want(PUPSeqno(A, 1) > bkey);

    kv64 got[128];
    kv64s out = {got, got + 128};
    u32 nsrc = 0;
    try(dog_mem_query, out, &nsrc, A);
    nedo { PUPClose(A); PUPClose(B); fail(__); }
    testeqv((long long)(out[0] - got), 67LL, "%lld");
    for (u64 i = 1; i <= 3; i++) want(got[i - 1].key == i);
    for (u64 i = 1000; i < 1064; i++) want(got[i - 997].key == i);

    call(PUPClose, A);
    call(PUPClose, B);
    TESTBErmrf(tmp);
    done;
}

// --- DOG-035: the run ToC — [data][toc][len:5]["TC"] -----------------

// DOG-035: put one row into a `pages`-page memtable, record width `width`.
static ok64 dog_toc_put(pup256b pups, path8s dir, u8cs ext, u32 width,
                        u32 pages, u64 key, u64 val) {
    sane(1);
    kv64 row = {key, val};
    u8cs rec = {(u8c *)&row, (u8c *)(&row + 1)};
    call(PUPPutMem, pups, TR(pups), dir, ext, rec, PUPkv64Sync, PUPkv64Merge, width,
         pages);
    done;
}

// DOG-035: compose `<tmp>/<10-RON64 key><ext>` — the ON-DISK run, read back
// with plain IO, since a seal leaves its own mapping trimmed to the data.
static ok64 dog_toc_path(char *out, size_t cap, char const *tmp, u64 key,
                         char const *ext) {
    sane(out != NULL);
    a_pad(u8, name, PUP_SEQNO_W + 32);
    call(RONu8sFeedPad, u8bIdle(name), (ok64)key, PUP_SEQNO_W);
    snprintf(out, cap, "%s/%.*s%s", tmp, (int)u8bDataLen(name),
             (char const *)u8bDataHead(name), ext);
    done;
}

// DOG-035: seal a `pages`-page run of descending keys, record width `row`; the run's
// pup_key lands in *key, the row count is the return of the caller's `n`.
static ok64 dog_toc_seal(pup256b pups, path8s dir, u8cs ext, u32 row,
                         u32 pages, u64 base, u64 n, u64 *key) {
    sane(key != NULL);
    for (u64 i = 0; i < n; i++)
        call(dog_toc_put, pups, dir, ext, row, pages, base - i, i);
    call(PUPCommitAs, pups, TR(pups), dir, ext, PUPkv64Sync, PUPkv64Merge, row, YES);
    *key = PUPSeqno(pups, 0);
    done;
}

// DOG-035: a sealed run ends in `[len:5]["TC"]`; the clipped data is exactly
// the pre-ToC bytes, and cell `i` is page `i`'s first record verbatim.
static ok64 PUPTestTocSeal(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_cstr(ext, ".toc.idx");
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    size_t per = PUPTocRows((u32)sizeof(kv64));
    want(per > 0);
    u64 n = (u64)per * 4;  //  exactly four pages -> four ToC cells
    Bpup256 pups = {};
    call(pup256bAllocate, pups, 16);
    u64 key = 0;
    call(dog_toc_seal, pups, $path(dir), ext, (u32)sizeof(kv64), 4, 1000000,
         n, &key);
    testeqv((long long)PUPCount(pups), 1LL, "%lld");
    call(PUPClose, pups);

    //  the on-disk trailer: the ToC block's BYTES, little-endian, then "TC"
    char path[512];
    call(dog_toc_path, path, sizeof path, tmp, key, ".toc.idx");
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

    //  a fresh open maps the WHOLE file — the clip is what hides the block
    Bpup256 fresh = {};
    call(pup256bAllocate, fresh, 16);
    call(PUPOpenAll, fresh, $path(dir), ext);
    u8cs raw = {}, data = {}, toc = {};
    PUPData(raw, fresh, 0);  //  row 0 — a caller that never opts in
    PUPDataRow(data, fresh, 0, (u32)sizeof(kv64));
    PUPToc(toc, fresh, 0, (u32)sizeof(kv64));
    testeqv((long long)u8csLen(raw), (long long)st.st_size, "%lld");
    testeqv((long long)u8csLen(data), (long long)(n * sizeof(kv64)), "%lld");
    testeqv((long long)u8csLen(toc), (long long)tb, "%lld");

    kv64 const *rows = (kv64 const *)data[0];
    kv64 const *cells = (kv64 const *)toc[0];
    for (size_t i = 0; i < 4; i++) {
        kv64 const *first = rows + i * per;
        want(cells[i].key == first->key && cells[i].val == first->val);
    }
    for (u64 i = 1; i < n; i++) want(rows[i - 1].key < rows[i].key);
    want(rows[0].key == 1000000 - (n - 1) && rows[n - 1].key == 1000000);

    //  and the merged query answers exactly what a plain run's would
    Bkv64 got = {};
    call(kv64bAllocate, got, n + 16);
    kv64 *gb = kv64bIdleHead(got);
    kv64s out = {gb, kv64bTerm(got)};
    u32 nsrc = 0;
    try(pup_query, out, &nsrc, fresh, (u32)sizeof(kv64));
    nedo { kv64bFree(got); PUPClose(fresh); fail(__); }
    testeqv((long long)(out[0] - gb), (long long)n, "%lld");
    for (u64 i = 0; i < n; i++) want(gb[i].key == 1000000 - (n - 1) + i);
    kv64bFree(got);
    call(PUPClose, fresh);
    TESTBErmrf(tmp);
    done;
}

// DOG-035: `row = 0` seals no ToC at all, and a run whose raw bytes
// merely END in "TC" is refused — presence is checked, never inferred.
static ok64 PUPTestTocOptOut(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_cstr(ext, ".plain.idx");
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    size_t per = PUPTocRows((u32)sizeof(kv64));
    u64 n = (u64)per * 4;
    Bpup256 pups = {};
    call(pup256bAllocate, pups, 16);
    u64 key = 0;
    call(dog_toc_seal, pups, $path(dir), ext, 0, 4, 2000000,
         n, &key);
    call(PUPClose, pups);

    //  four pages of rows and not one byte more
    char path[512];
    call(dog_toc_path, path, sizeof path, tmp, key, ".plain.idx");
    struct stat st = {};
    want(stat(path, &st) == 0);
    testeqv((long long)st.st_size, (long long)(n * sizeof(kv64)), "%lld");

    //  a booked run whose last bytes read `\0\0\0\0\0TC`: the magic matches
    //  and the length fits, but 32 - 7 - 0 is 9 mod 16, so the residue check
    //  refuses it — and a row-0 reader never looks in the first place.
    Bpup256 raw = {};
    call(pup256bAllocate, raw, 8);
    u8 fake[32] = {};
    fake[30] = 'T';
    fake[31] = 'C';
    a_dup(u8c, fb, ((u8cs){fake, fake + sizeof fake}));
    call(PUPCreateAt, raw, $path(dir), ext, fb, (u64)1000);
    u8cs whole = {}, data = {}, toc = {};
    PUPData(whole, raw, 0);
    PUPDataRow(data, raw, 0, (u32)sizeof(kv64));
    PUPToc(toc, raw, 0, (u32)sizeof(kv64));
    testeqv((long long)u8csLen(whole), (long long)sizeof fake, "%lld");
    testeqv((long long)u8csLen(data), (long long)sizeof fake, "%lld");
    want(u8csEmpty(toc));

    //  PUPS-26B034zlKV: two plain rows, the last val's bytes `00 09 0 0 0 0
    //  T C` — magic, length and residue all pass, but 16 data bytes take no
    //  cells, so a 9-byte ToC is not this run's geometry: both rows stay.
    kv64 two[2] = {{1, 111}, {2, 0x4354000000000900ULL}};
    a_dup(u8c, tb, ((u8cs){(u8c *)two, (u8c *)(two + 2)}));
    call(PUPCreateAt, raw, $path(dir), ext, tb, (u64)1001);
    PUPDataRow(data, raw, 1, (u32)sizeof(kv64));
    PUPToc(toc, raw, 1, (u32)sizeof(kv64));
    testeqv((long long)u8csLen(data), (long long)sizeof two, "%lld");
    want(u8csEmpty(toc));
    call(PUPClose, raw);
    TESTBErmrf(tmp);
    done;
}

// DOG-035: one stack, a ToC'd run and a plain one side by side — no migration,
// the merge never learns which is which, and newest still wins.
static ok64 PUPTestTocMixed(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_cstr(ext, ".mix.idx");
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    size_t per = PUPTocRows((u32)sizeof(kv64));
    u64 n = (u64)per * 4;
    Bpup256 pups = {};
    call(pup256bAllocate, pups, 16);
    u64 old = 0;
    //  older: a ToC'd four-page seal over keys 1..n, val = key
    for (u64 i = 0; i < n; i++)
        call(dog_toc_put, pups, $path(dir), ext, (u32)sizeof(kv64), 4, n - i,
             n - i);
    call(PUPCommitAs, pups, TR(pups), $path(dir), ext, T_TOC, YES);
    old = PUPSeqno(pups, 0);

    //  younger: a caller-booked PLAIN run over the even keys, val 999.  A
    //  booked run never runs the ladder, so the two stay side by side.
    Bkv64 flat = {};
    call(kv64bAllocate, flat, (u32)per + 4);
    kv64 *fb = kv64bIdleHead(flat);
    for (size_t i = 0; i < per; i++) fb[i] = (kv64){(u64)(i + 1) * 2, 999};
    a_dup(u8c, body, ((u8cs){(u8c *)fb, (u8c *)(fb + per)}));
    call(PUPCreateAt, pups, $path(dir), ext, body, old + 1);
    kv64bFree(flat);
    call(PUPClose, pups);

    Bpup256 fresh = {};
    call(pup256bAllocate, fresh, 16);
    call(PUPOpenAll, fresh, $path(dir), ext);
    testeqv((long long)PUPCount(fresh), 2LL, "%lld");
    u8cs t0 = {}, t1 = {}, d0 = {}, d1 = {};
    PUPToc(t0, fresh, 0, (u32)sizeof(kv64));
    PUPToc(t1, fresh, 1, (u32)sizeof(kv64));
    PUPDataRow(d0, fresh, 0, (u32)sizeof(kv64));
    PUPDataRow(d1, fresh, 1, (u32)sizeof(kv64));
    want(!u8csEmpty(t0) && u8csEmpty(t1));
    testeqv((long long)u8csLen(d0), (long long)(n * sizeof(kv64)), "%lld");
    testeqv((long long)u8csLen(d1), (long long)(per * sizeof(kv64)), "%lld");

    Bkv64 got = {};
    call(kv64bAllocate, got, n + 16);
    kv64 *gb = kv64bIdleHead(got);
    kv64s out = {gb, kv64bTerm(got)};
    u32 nsrc = 0;
    try(pup_query, out, &nsrc, fresh, (u32)sizeof(kv64));
    nedo { kv64bFree(got); PUPClose(fresh); fail(__); }
    testeqv((long long)(out[0] - gb), (long long)n, "%lld");
    for (u64 i = 0; i < n; i++) {
        u64 k = i + 1;
        want(gb[i].key == k);
        //  the younger plain run shadows the even keys it covers
        want(gb[i].val == ((k % 2 == 0 && k <= per * 2) ? 999 : k));
    }
    kv64bFree(got);
    call(PUPClose, fresh);
    TESTBErmrf(tmp);
    done;
}

// DOG-035: a TOTAL row order — what `wh128Z`/`u64Z` are, and the order that
// makes a `{k, 0}` needle sort BEFORE its own key's row.  `kv64Z` is key-only,
// so it cannot express the boundary this test pins.
static b8 dog_kv64_full(kv64 const *a, kv64 const *b) {
    return a->key != b->key ? a->key < b->key : a->val < b->val;
}

// DOG-035: the ToC's contract — cell `i` covers page `i`, so the searched span
// is EXACTLY one page, and a needle whose row is a page's FIRST record is
// answered from the cell itself (that row sorts AFTER the needle, so the page
// search alone would miss it).  The oracle is a linear scan over the cells,
// not a second copy of the search rule.
static ok64 PUPTestTocNarrow(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_cstr(ext, ".narrow.idx");
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    size_t per = PUPTocRows((u32)sizeof(kv64));
    u64 n = (u64)per * 4;
    Bpup256 pups = {};
    call(pup256bAllocate, pups, 16);
    u64 key = 0;
    //  even keys only (every odd probe is a genuine miss) and every val
    //  NON-ZERO, so a `{k, 0}` needle sorts strictly BEFORE its own row
    for (u64 i = 0; i < n; i++)
        call(dog_toc_put, pups, $path(dir), ext, (u32)sizeof(kv64), 4,
             (n - i) * 2, n - i);
    call(PUPCommitAs, pups, TR(pups), $path(dir), ext, T_TOC, YES);
    key = PUPSeqno(pups, 0);
    want(key != 0);
    call(PUPClose, pups);

    Bpup256 fresh = {};
    call(pup256bAllocate, fresh, 16);
    call(PUPOpenAll, fresh, $path(dir), ext);
    u8cs data = {}, toc = {};
    PUPDataRow(data, fresh, 0, (u32)sizeof(kv64));
    PUPToc(toc, fresh, 0, (u32)sizeof(kv64));
    kv64cs rows = {(kv64 const *)data[0], (kv64 const *)data[1]};
    kv64cs cells = {(kv64 const *)toc[0], (kv64 const *)toc[1]};
    size_t nc = (size_t)$len(cells);
    testeqv((long long)nc, 4LL, "%lld");

    u32 fromcell = 0;
    for (u64 k = 1; k <= n * 2 + 2; k++) {
        kv64 needle = {k, 0};
        kv64 const *whole = kv64sFindGEZ(rows, &needle, dog_kv64_full);
        b8 present = whole < rows[1] && whole->key == k;
        //  oracle: the last cell NOT GREATER than the probe, by linear scan
        size_t idx = 0;
        for (size_t i = 0; i < nc; i++)
            if (!dog_kv64_full(&needle, cells[0] + i)) idx = i;
        if (idx + 1 < nc && cells[0][idx + 1].key == k) {
            //  the boundary: cell idx+1 IS page idx+1's first record and IS
            //  the answer, straight from the ToC — no record search at all
            want(present && whole == rows[0] + (idx + 1) * per);
            want(cells[0][idx + 1].val == whole->val);
            fromcell++;
            continue;
        }
        //  otherwise the searched span is EXACTLY page idx, never wider
        kv64cs page = {rows[0] + idx * per, rows[1]};
        if ((size_t)$len(page) > per) page[1] = page[0] + per;
        kv64 const *narrow = kv64sFindGEZ(page, &needle, dog_kv64_full);
        if (present) {
            want(narrow < page[1] && narrow == whole);
        } else {
            want(narrow == page[1] || narrow->key != k);
        }
    }
    //  the boundary is REAL, not a dead branch: one probe per page but the first
    testeqv((long long)fromcell, (long long)(nc - 1), "%lld");
    call(PUPClose, fresh);
    TESTBErmrf(tmp);
    done;
}

// DOG-035: the ladder's output is ToC'd too, and the collapse weighs CLIPPED
// lengths — an unclipped source would feed dedup real-looking duplicates.
static ok64 PUPTestTocLadder(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_cstr(ext, ".ladder.idx");
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    size_t per = PUPTocRows((u32)sizeof(kv64));
    u64 n = (u64)per * 8;  //  eight one-page seals -> the ladder collapses
    Bpup256 pups = {};
    call(pup256bAllocate, pups, 64);
    for (u64 i = 0; i < n; i++)
        call(dog_toc_put, pups, $path(dir), ext, (u32)sizeof(kv64), 1,
             100000 - i, i);
    call(PUPCommitAs, pups, TR(pups), $path(dir), ext, T_TOC, YES);

    Bpup256 fresh = {};
    call(pup256bAllocate, fresh, 64);
    call(PUPOpenAll, fresh, $path(dir), ext);
    u32 runs = PUPCount(fresh);
    want(runs >= 1);
    u8cs big = {}, tocbig = {};
    PUPDataRow(big, fresh, 0, (u32)sizeof(kv64));
    PUPToc(tocbig, fresh, 0, (u32)sizeof(kv64));
    //  the collapsed run holds whole rows and carries its own block
    want(u8csLen(big) % (long)sizeof(kv64) == 0);
    want(u8csLen(big) >= (long)(2 * per * sizeof(kv64)));
    testeqv((long long)u8csLen(tocbig),
            (long long)(((size_t)u8csLen(big) / (per * sizeof(kv64))) *
                        sizeof(kv64)),
            "%lld");

    Bkv64 got = {};
    call(kv64bAllocate, got, n + 64);
    kv64 *gb = kv64bIdleHead(got);
    kv64s out = {gb, kv64bTerm(got)};
    u32 nsrc = 0;
    try(pup_query, out, &nsrc, fresh, (u32)sizeof(kv64));
    nedo { kv64bFree(got); PUPClose(fresh); fail(__); }
    testeqv((long long)(out[0] - gb), (long long)n, "%lld");
    for (u64 i = 1; i < n; i++) want(gb[i - 1].key < gb[i].key);
    want(gb[0].key == 100000 - (n - 1) && gb[n - 1].key == 100000);
    kv64bFree(got);
    call(PUPClose, fresh);
    call(PUPClose, pups);
    TESTBErmrf(tmp);
    done;
}

ok64 PUPTestToc(void) {
    sane(1);
    call(PUPTestTocSeal);
    call(PUPTestTocOptOut);
    call(PUPTestTocMixed);
    call(PUPTestTocNarrow);
    call(PUPTestTocLadder);
    done;
}

// --- PUPS-26A276Xh2e: runs hold no fd, the memtable is no file ---------

//  Live fds of this process, from /proc/self/fd (the fcntl probe where there
//  is none).
static int pup_proc_fds(void) {
    DIR *d = opendir("/proc/self/fd");
    if (d == NULL) return dog_open_fd_count();
    int n = 0;
    while (readdir(d) != NULL) n++;
    closedir(d);
    return n;
}

//  More runs than FILE_MAX_OPEN are open at once, every one readable, and the
//  open leaves the fd table as it found it.
static ok64 PUPTestManyRuns(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_cstr(ext, ".many.idx");
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    enum { N = FILE_MAX_OPEN + 8 };
    int fd0 = pup_proc_fds();
    want(fd0 > 0);
    Bpup256 pups = {};
    call(pup256bAllocate, pups, N + 4);
    for (u64 i = 0; i < N; i++) {
        kv64 row = {i, i * 10};
        u8cs rec = {(u8c *)&row, (u8c *)(&row + 1)};
        call(PUPCreateAt, pups, $path(dir), ext, rec, 1000 + i);
    }
    testeqv((long long)pup_proc_fds(), (long long)fd0, "%lld");
    call(PUPClose, pups);

    Bpup256 fresh = {};
    call(pup256bAllocate, fresh, N + 4);
    call(PUPOpenAll, fresh, $path(dir), ext);
    testeqv((long long)PUPCount(fresh), (long long)N, "%lld");
    testeqv((long long)pup_proc_fds(), (long long)fd0, "%lld");
    for (u32 i = 0; i < N; i++) {
        u8cs d = {};
        PUPData(d, fresh, i);
        want(u8csLen(d) == sizeof(kv64));
        kv64 const *r = (kv64 const *)d[0];
        want(r->key == i && r->val == (u64)i * 10);
    }
    call(PUPClose, fresh);
    TESTBErmrf(tmp);
    done;
}

//  The memtable lives in anonymous memory: puts, segment seals and a commit
//  hold no fd and leave no file but the run; the caller's runs carry the
//  memtable's sorted runs until the seal empties them.
static ok64 PUPTestMemNoFd(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_cstr(ext, ".nofd.idx");
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    int fd0 = pup_proc_fds();
    want(fd0 > 0);
    Bpup256 pups = {};
    call(pup256bAllocate, pups, 8);
    kv64cs slots[ABC_HIT_MAX_RUNS];
    kv64csb runs = {slots, slots, slots, slots + ABC_HIT_MAX_RUNS};
    for (u64 i = 0; i < 200; i++) {
        kv64 row = {(i * 7919) % 200, i};
        u8cs rec = {(u8c *)&row, (u8c *)(&row + 1)};
        call(PUPkv64Put, pups, runs, $path(dir), ext, rec);
    }
    want(runs[2] > runs[1]);
    testeqv((long long)pup_proc_fds(), (long long)fd0, "%lld");
    {
        u32 files = 0;
        DIR *dh = opendir(tmp);
        want(dh != NULL);
        for (struct dirent *e; (e = readdir(dh));) files += e->d_name[0] != '.';
        closedir(dh);
        testeqv((long long)files, 0LL, "%lld");
    }
    kv64 got[256];
    kv64s out = {got, got + 256};
    call(PUPkv64Read, out, pups, runs);
    testeqv((long long)(out[0] - got), 200LL, "%lld");
    call(PUPkv64Commit, pups, runs, $path(dir), ext);
    want(runs[2] == runs[1]);
    testeqv((long long)PUPCount(pups), 1LL, "%lld");
    testeqv((long long)pup_proc_fds(), (long long)fd0, "%lld");
    kv64 got2[256];
    kv64s out2 = {got2, got2 + 256};
    call(PUPkv64Read, out2, pups, runs);
    testeqv((long long)(out2[0] - got2), 200LL, "%lld");
    want(memcmp(got, got2, sizeof(kv64) * 200) == 0);
    call(PUPClose, pups);
    TESTBErmrf(tmp);
    done;
}

// PUPS-26B034NDyT: a lock is keyed by the directory's identity, not its path
// spelling: an alias (symlink) re-enters the held lock instead of flocking a
// second fd (a self-deadlock), and more than 8 directories may be held.
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

ok64 PUPTestMem(void) {
    sane(1);
    call(PUPTestNameTaken);
    call(PUPTestLadder);
    call(PUPTestDropAt);
    call(PUPTestMemRoundTrip);
    call(PUPTestMemCollapse);
    call(PUPTestMemFlush);
    call(PUPTestMemCommit);
    call(PUPTestMemCrashInvisible);
    call(PUPTestLadderKey);
    call(PUPTestTomb);
    call(PUPTestCrashLadder);
    call(PUPTestLock);
    call(PUPTestLockAlias);
    call(PUPTestTwoWriters);
    call(PUPTestForeignRun);
    call(PUPTestManyRuns);
    call(PUPTestMemNoFd);
    done;
}
ok64 PUPStest() {
    sane(1);
    call(PUPTestLeaks);
    call(PUPTestMem);
    call(PUPTestToc);
    done;
}

TEST(PUPStest);
