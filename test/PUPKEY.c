//  PUPS-26A309qbG9: a caller that publishes runs itself, by a key ceiling.
//  (1) a key floor: every run a write creates (create, seal, ladder output)
//      takes a key at or above the caller's `floor`, one counter across stacks;
//  (2) a ladder that hands its merged-away runs back (`dead`) instead
//      of unlinking them, and PUPUnlink to retire them later;
//  (3) Open's ceiling: runs keyed above it are not mapped, and a run that
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

#ifdef __GLIBC__
//  gcc + _FORTIFY_SOURCE rewrites open(path, flags) to __open_2
int __open_2(char const *path, int flags) { return open(path, flags); }
#endif

//  one kv64 row as a run under `key`
static ok64 key_run(kv64csb h, u64b keys, path8s dir, u8cs ext, u64 key,
                    u64 k) {
    sane(1);
    kv64 row = {k, k * 10};
    kv64cs rs = {&row, &row + 1};
    call(PUPkv64Create, h, keys, dir, ext, rs, key, NO, NULL);
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
    a_pup(kv64, pa);
    a_pup(kv64, pb);
    u64 f0 = (u64)RONNow() + (1ULL << 30);  //  well past what RONNow picks
    u64 fl = f0;
    kv64 row = {1, 10};
    kv64cs rec = {&row, &row + 1};

    call(PUPkv64Create, pa, pa_keys, $path(dir), ea, rec, 0, NO, &fl);
    u64 k1 = u64bDataHead(pa_keys)[0];
    want(k1 >= f0 && fl > k1);
    u64 f1 = fl;
    call(PUPkv64Create, pb, pb_keys, $path(dir), eb, rec, 0, NO, &fl);
    u64 k2 = u64bDataHead(pb_keys)[0];
    want(k2 >= f1 && fl > k2);

    //  a seal of the memtable: its run lands past the counter too
    call(kv64bMap, BOXkv64Mem(pb), 64);
    u64 f2 = fl;
    kv64 r2 = {2, 20};
    call(PUPkv64Put, pb, &r2);
    call(PUPkv64Commit, pb, pb_keys, $path(dir), eb, YES, &fl);
    size_t n = u64bDataLen(pb_keys);
    want(n >= 1);
    want(u64bDataHead(pb_keys)[n - 1] >= f2);
    want(fl > u64bDataHead(pb_keys)[n - 1]);
    PUPkv64Close(pa, pa_keys);
    PUPkv64Close(pb, pb_keys);
    kv64bUnMap(BOXkv64Mem(pb));

    //  a ladder merge: three equal runs keyed low, the output at the floor
    a_cstr(ec, ".c.idx");
    a_pup(kv64, pc);
    for (u64 r = 0; r < 3; r++)
        call(key_run, pc, pc_keys, $path(dir), ec, 1000 + r, 10 + r);
    u64 f3 = fl;
    call(PUPkv64Ladder, pc, pc_keys, $path(dir), ec, NULL, &fl);
    testeqv((long long)u64bDataLen(pc_keys), 1LL, "%lld");
    want(u64bDataHead(pc_keys)[0] >= f3);  //  not 1003 any more
    want(fl > u64bDataHead(pc_keys)[0]);
    PUPkv64Close(pc, pc_keys);

    //  no floor: the youngest key + 1 (PUPS-005)
    a_pup(kv64, pd);
    a_cstr(ed, ".d.idx");
    for (u64 r = 0; r < 3; r++)
        call(key_run, pd, pd_keys, $path(dir), ed, 1000 + r, 10 + r);
    call(PUPkv64Ladder, pd, pd_keys, $path(dir), ed, NULL, NULL);
    testeqv((long long)u64bDataHead(pd_keys)[0], 1003LL, "%lld");
    PUPkv64Close(pd, pd_keys);
    TESTBErmrf(tmp);
    done;
}

//  (2) a ladder with `dead`: the merged-away runs stay on disk, their keys
//  come back, the handle drops them, a scan skipping them leaves them out,
//  PUPUnlink retires them.
static ok64 PUPKeyDead(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    a_cstr(ext, ".dead.idx");
    a_pup(kv64, h);
    for (u64 r = 0; r < 3; r++)
        call(key_run, h, h_keys, $path(dir), ext, 1000 + r, 10 + r);
    a_pad(u64, dead, 16);
    call(PUPkv64Ladder, h, h_keys, $path(dir), ext, dead, NULL);
    testeqv((long long)u64bDataLen(h_keys), 1LL, "%lld");
    testeqv((long long)u64bDataHead(h_keys)[0], 1003LL, "%lld");
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
    a_pup(kv64, fresh);
    call(PUPkv64Open, fresh, fresh_keys, $path(dir), ext, u64max);
    testeqv((long long)u64bDataLen(fresh_keys), 4LL, "%lld");
    call(PUPkv64Read, o1, fresh);
    testeqv((long long)(o1[0] - out), 3LL, "%lld");
    PUPkv64Close(fresh, fresh_keys);

    //  a scan that skips `dead` lists the merged run only; a seal adds one
    a_pad(u64, seen, 16);
    call(PUPScan, seen, $path(dir), ext, u64max, u64bDataC(dead));
    testeqv((long long)u64bDataLen(seen), 1LL, "%lld");
    call(kv64bMap, BOXkv64Mem(h), 64);
    kv64 row = {20, 200};
    call(PUPkv64Put, h, &row);
    call(PUPkv64Commit, h, h_keys, $path(dir), ext, YES, NULL);
    $for(u64c, k, u64bDataC(h_keys)) want(*k < 1000 || *k > 1002);
    kv64s o2 = {out, out + 16};
    call(PUPkv64Read, o2, h);
    testeqv((long long)(o2[0] - out), 4LL, "%lld");

    //  retire: every dead file goes (an unsorted list too), a retry is fine
    u64 *dk = u64bDataHead(dead);
    u64 t = dk[0];
    dk[0] = dk[u64bDataLen(dead) - 1];
    dk[u64bDataLen(dead) - 1] = t;
    call(PUPUnlink, $path(dir), ext, u64bData(dead));
    call(PUPUnlink, $path(dir), ext, u64bData(dead));
    $for(u64, k, u64bData(dead)) want(!key_file($path(dir), ".dead.idx", *k));
    a_pup(kv64, after);
    call(PUPkv64Open, after, after_keys, $path(dir), ext, u64max);
    testeqv((long long)u64bDataLen(after_keys), (long long)u64bDataLen(h_keys),
            "%lld");
    kv64s o3 = {out, out + 16};
    call(PUPkv64Read, o3, after);
    testeqv((long long)(o3[0] - out), 4LL, "%lld");
    PUPkv64Close(after, after_keys);
    PUPkv64Close(h, h_keys);
    kv64bUnMap(BOXkv64Mem(h));
    TESTBErmrf(tmp);
    done;
}

//  (3) Open with a ceiling bounds the view; a run unlinked between the
//  listing and its mapping is PUPGONE, and a retry reads the rest.
static ok64 PUPKeyCeil(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    a_cstr(ext, ".ceil.idx");
    {
        a_pup(kv64, mk);
        for (u64 r = 0; r < 3; r++)
            call(key_run, mk, mk_keys, $path(dir), ext, 1000 + r, 10 + r);
        PUPkv64Close(mk, mk_keys);
    }
    u64 ceils[4] = {999, 1000, 1001, 1002};
    for (u32 c = 0; c < 4; c++) {
        a_pup(kv64, p);
        call(PUPkv64Open, p, p_keys, $path(dir), ext, ceils[c]);
        testeqv((long long)u64bDataLen(p_keys), (long long)c, "%lld");
        for (u32 i = 0; i < c; i++) want(u64bDataHead(p_keys)[i] == 1000 + i);
        PUPkv64Close(p, p_keys);
    }
    //  the second run vanishes mid-open: PUPGONE, not PUPNOMAP
    key_gone_ext = ".ceil.idx";
    key_gone_ops = 0;
    key_gone_at = 2;
    a_pup(kv64, g);
    ok64 o = PUPkv64Open(g, g_keys, $path(dir), ext, 1002);
    key_gone_at = 0;
    testeqv((long long)o, (long long)PUPGONE, "%llx");
    PUPkv64Close(g, g_keys);
    a_pup(kv64, r);
    call(PUPkv64Open, r, r_keys, $path(dir), ext, u64max);
    testeqv((long long)u64bDataLen(r_keys), 2LL, "%lld");
    PUPkv64Close(r, r_keys);
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
