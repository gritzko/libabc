//  PUPS-26A309qbG9: a caller that publishes runs itself, by a key ceiling.
//  (1) a key floor: every run a write creates (create, seal, ladder output)
//      takes a key at or above `pupopt.floor`, one counter across stacks;
//  (2) a ladder that hands its merged-away runs back (`pupopt.dead`) instead
//      of unlinking them, and PUPUnlink to retire them later;
//  (3) PUPOpenTo: runs keyed above the ceiling are not mapped, and a run that
//      vanishes between the listing and its mapping fails with PUPGONE.
#include "PUPS.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "FILE.h"
#include "PRO.h"
#include "TEST.h"

#include "TESTBE.h"

#define X(M, name) M##kv64##name
#include "HITx.h"
#include "PUPx.h"
#undef X

//  open shim: libabc is linked statically, so FILEOpen's open() binds here.
//  Once armed, the `key_gone_at`-th open of a `key_gone_ext` run unlinks the
//  file first, as another writer's unlink racing the scan would.
static int key_gone_at = 0, key_gone_ops = 0;
static char const *key_gone_ext = "";

int open(char const *path, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    size_t l = strlen(path), e = strlen(key_gone_ext);
    if (key_gone_at != 0 && l >= e && strcmp(path + l - e, key_gone_ext) == 0 &&
        ++key_gone_ops == key_gone_at)
        unlinkat(AT_FDCWD, path, 0);
    return openat(AT_FDCWD, path, flags, mode);
}

//  one kv64 row as a run under `key`
static ok64 key_run(pup256b pups, path8s dir, u8cs ext, u64 key, u64 k) {
    sane(1);
    kv64 row = {k, k * 10};
    a_dup(u8c, rec, ((u8cs){(u8c *)&row, (u8c *)(&row + 1)}));
    call(PUPCreateAt, pups, dir, ext, rec, key);
    done;
}

static b8 key_file(path8s dir, char const *ext, u64 key) {
    a_path(p);
    if (PATHu8bDup(p, dir) != OK) return NO;
    a_pad(u8, name, 64);
    if (RONu8sFeedPad(u8bIdle(name), (ok64)key, PUP_SEQNO_W) != OK) return NO;
    a_cstr(e, ext);
    u8bFeed(name, e);
    if (PATHu8bPush(p, u8bDataC(name)) != OK) return NO;
    return FILEExists($path(p)) == OK;
}

//  (1) the floor: creates on two stacks share one counter, a seal and a
//  ladder merge land at or above it too.
static ok64 PUPKeyFloor(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    a_cstr(ea, ".a.idx");
    a_cstr(eb, ".b.idx");
    Bpup256 pa = {}, pb = {};
    call(pup256bAllocate, pa, 16);
    call(pup256bAllocate, pb, 16);
    u64 f0 = (u64)RONNow() + (1ULL << 30);  //  well past what RONNow picks
    pupopt o = {.floor = f0};
    kv64 row = {1, 10};
    a_dup(u8c, rec, ((u8cs){(u8c *)&row, (u8c *)(&row + 1)}));

    call(PUPCreateOpt, pa, $path(dir), ea, rec, NO, &o);
    u64 k1 = PUPSeqno(pa, 0);
    want(k1 >= f0);
    want(o.floor > k1);
    u64 f1 = o.floor;
    call(PUPCreateOpt, pb, $path(dir), eb, rec, NO, &o);
    u64 k2 = PUPSeqno(pb, 0);
    want(k2 >= f1);
    want(o.floor > k2);

    //  a seal of the memtable: its run lands past the counter too
    kv64cs slots[ABC_HIT_MAX_RUNS];
    kv64cs *runs[4] = {slots, slots, slots, slots + ABC_HIT_MAX_RUNS};
    u64 f2 = o.floor;
    kv64 r2 = {2, 20};
    a_dup(u8c, rec2, ((u8cs){(u8c *)&r2, (u8c *)(&r2 + 1)}));
    call(PUPkv64PutOpt, pb, runs, $path(dir), eb, rec2, 0, &o);
    call(PUPkv64CommitOpt, pb, runs, $path(dir), eb, YES, &o);
    //  the ladder may merge the two runs; either way every key is >= f1
    u32 n = PUPCount(pb);
    want(n >= 1);
    want(PUPSeqno(pb, n - 1) >= f2);
    want(o.floor > PUPSeqno(pb, n - 1));
    call(PUPClose, pa);
    call(PUPClose, pb);

    //  a ladder merge: three equal runs keyed low, the output at the floor
    a_cstr(ec, ".c.idx");
    Bpup256 pc = {};
    call(pup256bAllocate, pc, 16);
    for (u64 r = 0; r < 3; r++)
        call(key_run, pc, $path(dir), ec, 1000 + r, 10 + r);
    u64 f3 = o.floor;
    call(PUPkv64LadderOpt, pc, $path(dir), ec, &o);
    testeqv((long long)PUPCount(pc), 1LL, "%lld");
    want(PUPSeqno(pc, 0) >= f3);  //  not 1003 any more
    want(o.floor > PUPSeqno(pc, 0));
    call(PUPClose, pc);

    //  no opt: the plain entry points pick their keys as before
    Bpup256 pd = {};
    call(pup256bAllocate, pd, 16);
    a_cstr(ed, ".d.idx");
    for (u64 r = 0; r < 3; r++)
        call(key_run, pd, $path(dir), ed, 1000 + r, 10 + r);
    call(PUPkv64LadderOpt, pd, $path(dir), ed, NULL);
    testeqv((long long)PUPSeqno(pd, 0), 1003LL, "%lld");
    call(PUPClose, pd);
    TESTBErmrf(tmp);
    done;
}

//  (2) a ladder with `dead`: the merged-away runs stay on disk, their keys
//  come back, the handle and later scans through the opt skip them, and
//  PUPUnlink retires them.
static ok64 PUPKeyDead(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    a_cstr(ext, ".dead.idx");
    Bpup256 pups = {};
    call(pup256bAllocate, pups, 16);
    for (u64 r = 0; r < 3; r++)
        call(key_run, pups, $path(dir), ext, 1000 + r, 10 + r);
    a_pad(u64, dead, 16);
    pupopt o = {.dead = dead};
    call(PUPkv64LadderOpt, pups, $path(dir), ext, &o);
    testeqv((long long)PUPCount(pups), 1LL, "%lld");
    testeqv((long long)PUPSeqno(pups, 0), 1003LL, "%lld");
    testeqv((long long)u64bDataLen(dead), 3LL, "%lld");
    for (u64 r = 0; r < 3; r++) {
        want(u64bDataHead(dead)[r] == 1000 + r);
        want(key_file($path(dir), ".dead.idx", 1000 + r));
    }
    want(key_file($path(dir), ".dead.idx", 1003));

    //  a fresh plain scan sees the merged run AND its sources, which read the
    //  same: the merged run is younger and wins
    kv64 out[16];
    kv64s o1 = {out, out + 16};
    Bpup256 fresh = {};
    call(pup256bAllocate, fresh, 16);
    call(PUPOpenAll, fresh, $path(dir), ext);
    testeqv((long long)PUPCount(fresh), 4LL, "%lld");
    call(PUPkv64Read, o1, fresh, NULL);
    testeqv((long long)(o1[0] - out), 3LL, "%lld");
    call(PUPClose, fresh);

    //  a seal through the same opt re-scans the dir: the sources stay out
    kv64cs slots[ABC_HIT_MAX_RUNS];
    kv64cs *runs[4] = {slots, slots, slots, slots + ABC_HIT_MAX_RUNS};
    kv64 row = {20, 200};
    a_dup(u8c, rec, ((u8cs){(u8c *)&row, (u8c *)(&row + 1)}));
    call(PUPkv64PutOpt, pups, runs, $path(dir), ext, rec, 0, &o);
    call(PUPkv64CommitOpt, pups, runs, $path(dir), ext, YES, &o);
    for (u32 i = 0; i < PUPCount(pups); i++)
        want(PUPSeqno(pups, i) < 1000 || PUPSeqno(pups, i) > 1002);
    kv64s o2 = {out, out + 16};
    call(PUPkv64Read, o2, pups, runs);
    testeqv((long long)(o2[0] - out), 4LL, "%lld");

    //  retire: every dead file goes (an unsorted list too), a retry is fine
    u64 *dk = u64bDataHead(dead);
    u64 t = dk[0];
    dk[0] = dk[u64bDataLen(dead) - 1];
    dk[u64bDataLen(dead) - 1] = t;
    call(PUPUnlink, $path(dir), ext, u64bData(dead));
    call(PUPUnlink, $path(dir), ext, u64bData(dead));
    $for(u64, k, u64bData(dead)) want(!key_file($path(dir), ".dead.idx", *k));
    Bpup256 after = {};
    call(pup256bAllocate, after, 16);
    call(PUPOpenAll, after, $path(dir), ext);
    testeqv((long long)PUPCount(after), (long long)PUPCount(pups), "%lld");
    kv64s o3 = {out, out + 16};
    call(PUPkv64Read, o3, after, NULL);
    testeqv((long long)(o3[0] - out), 4LL, "%lld");
    call(PUPClose, after);
    call(PUPClose, pups);
    TESTBErmrf(tmp);
    done;
}

//  (3) PUPOpenTo: the ceiling bounds the view; a run unlinked between the
//  listing and its mapping is PUPGONE, and a retry reads the rest.
static ok64 PUPKeyCeil(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    a_cstr(ext, ".ceil.idx");
    Bpup256 pups = {};
    call(pup256bAllocate, pups, 16);
    for (u64 r = 0; r < 3; r++)
        call(key_run, pups, $path(dir), ext, 1000 + r, 10 + r);
    call(PUPClose, pups);

    u64 ceils[4] = {999, 1000, 1001, 1002};
    for (u32 c = 0; c < 4; c++) {
        Bpup256 p = {};
        call(pup256bAllocate, p, 16);
        call(PUPOpenTo, p, $path(dir), ext, ceils[c]);
        testeqv((long long)PUPCount(p), (long long)c, "%lld");
        for (u32 i = 0; i < c; i++) want(PUPSeqno(p, i) == 1000 + i);
        call(PUPClose, p);
    }

    //  the second run vanishes mid-open: PUPGONE, not PUPNOMAP
    key_gone_ext = ".ceil.idx";
    key_gone_ops = 0;
    key_gone_at = 2;
    Bpup256 g = {};
    call(pup256bAllocate, g, 16);
    ok64 o = PUPOpenTo(g, $path(dir), ext, 1002);
    key_gone_at = 0;
    testeqv(o, PUPGONE, "%llx");
    call(PUPClose, g);
    Bpup256 r = {};
    call(pup256bAllocate, r, 16);
    call(PUPOpenAll, r, $path(dir), ext);
    testeqv((long long)PUPCount(r), 2LL, "%lld");
    call(PUPClose, r);
    TESTBErmrf(tmp);
    done;
}

ok64 PUPKEYtest() {
    sane(1);
    call(PUPKeyFloor);
    call(PUPKeyDead);
    call(PUPKeyCeil);
    done;
}

TEST(PUPKEYtest);
