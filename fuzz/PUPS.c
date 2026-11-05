//  abc/fuzz/PUPS.c — PUPS-26B04CThJv: the PUPS house (PUPx.h) vs a plain
//  array model.  The input is a byte program of ops on a u16 key space: put,
//  delete, fill, commit soft or durable, ladder, close+reopen, get, range,
//  create; the kv64 lane (youngest wins, tombstones) and the cnt64 lane
//  (ABC_X_MERGE sums) run it all, then compare once, again after a reopen;
//  per op only codes and Gets.  A 1-page memtable, runs past 2 pages get a
//  ToC, creates reach the 62-run cap.
//  Each input gets $PUPS_FUZZ_DIR/PUPS.<pid>.<n>/{kv,cnt}/, removed at its end.

#include <stdlib.h>
#include <unistd.h>

#include "PUPS.h"
#include "FILE.h"
#include "PRO.h"
#include "TEST.h"

#define PF_TOMB (~0ULL)
#define PUPx_DEAD(r) ((r)->val == PF_TOMB)
#define X(M, name) M##kv64##name
#include "PUPx.h"
#include "PUPSx.h"
#undef X

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
#define PF_SUM
#include "PUPSx.h"
#undef X

#define PF_MAX_INPUT 4096

//  The scratch dir of this input, made fresh, a subdir per lane.
static ok64 pf_dir(path8b dir, path8b kd, path8b cd) {
    sane(1);
    static u64 seq = 0;
    char const *base = getenv("PUPS_FUZZ_DIR");
    if (!base || !*base) base = "/tmp";
    a_cstr(bs, base);
    a_cstr(pre, "PUPS.");
    a_cstr(dot, ".");
    a_pad(u8, seg, 64);
    call(u8bFeed, seg, pre);
    call(utf8sFeed10, u8bIdle(seg), (u64)getpid());
    call(u8bFeed, seg, dot);
    call(utf8sFeed10, u8bIdle(seg), seq++);
    call(PATHu8bFeed, dir, bs);
    a_dup(u8c, name, u8bData(seg));
    call(PATHu8bPush, dir, name);
    a_cstr(kv, "kv");
    a_cstr(cn, "cnt");
    call(PATHu8bFeed, kd, $path(dir));
    call(PATHu8bPush, kd, kv);
    call(PATHu8bFeed, cd, $path(dir));
    call(PATHu8bPush, cd, cn);
    call(FILEMakeDirP, $path(kd));
    call(FILEMakeDirP, $path(cd));
    done;
}

//  The next input byte, 0 past the end.
static u8 pf_byte(u8cs in) {
    if ($empty(in)) return 0;
    return *in[0]++;
}

static u32 pf_key(u8cs in) {
    u32 hi = pf_byte(in);
    return hi << 8 | pf_byte(in);
}

FUZZ(u8, PUPSfuzz) {
    sane(1);
    if ($len(input) > PF_MAX_INPUT) input[1] = input[0] + PF_MAX_INPUT;
    call(FILEInit);
    a_path(dir);
    a_path(kd);
    a_path(cd);
    call(pf_dir, dir, kd, cd);
    a_cstr(kx, ".kv");
    a_cstr(cx, ".cnt");
    size_t page = FILESysPage();
    a_pup(kv64, kh);
    a_pup(cnt64, ch);
    call(kv64bMap, BOXkv64Mem(kh), page / sizeof(kv64));
    call(cnt64bMap, BOXcnt64Mem(ch), page / sizeof(cnt64));
    PFkv64Reset();
    PFcnt64Reset();
#define KV kh, kh_keys, $path(kd), kx
#define CN ch, ch_keys, $path(cd), cx
    a_dup(u8c, in, input);
    while (!$empty(in)) {
        u8 op = pf_byte(in);
        switch (op & 15) {
            case 0 ... 5: {
                u32 k = pf_key(in);
                u64 v = pf_byte(in);
                call(PFkv64Put, KV, k, v);
                call(PFcnt64Put, CN, k, v + 1);
            } break;
            case 6: {
                u32 k = pf_key(in);
                call(PFkv64Put, KV, k, PF_TOMB);
                call(PFcnt64Get, CN, k);
            } break;
            case 7: {  //  fill: up to 255 consecutive keys, a memtable's worth
                u32 k = pf_key(in), n = pf_byte(in);
                u64 v = pf_byte(in);
                for (u32 i = 0; i < n && k + i < (1u << 16); i++) {
                    call(PFkv64Put, KV, k + i, v);
                    call(PFcnt64Put, CN, k + i, v + 1);
                }
            } break;
            case 8:
            case 9:
                call(PFkv64Commit, KV, (op & 15) == 9);
                call(PFcnt64Commit, CN, (op & 15) == 9);
                break;
            case 10:
                call(PFkv64Ladder, KV);
                call(PFcnt64Ladder, CN);
                break;
            case 11:
                call(PFkv64Reopen, KV);
                call(PFcnt64Reopen, CN);
                break;
            case 12: {
                u32 k = pf_key(in);
                call(PFkv64Get, KV, k);
                call(PFcnt64Get, CN, k);
            } break;
            case 13: {
                u32 lo = pf_key(in), hi = pf_key(in);
                if (lo > hi) {
                    u32 t = lo;
                    lo = hi, hi = t;
                }
                call(PFkv64Range, KV, lo, hi, NO);
                call(PFcnt64Range, CN, lo, hi, NO);
            } break;
            case 14: {  //  one run of up to 64 rows
                u32 k = pf_key(in), step = pf_byte(in) % 16 + 1;
                u32 n = pf_byte(in) % 65;
                u64 v = pf_byte(in);
                call(PFkv64Create, KV, k, step, n, v);
                call(PFcnt64Create, CN, k, step, n, v + 1);
            } break;
            case 15: {  //  up to 63 one-row runs: the 62-run cap
                u32 k = pf_key(in), n = pf_byte(in) % 64;
                for (u32 i = 0; i < n; i++) {
                    call(PFkv64Create, KV, (k + i) & 0xffff, 1, 1, i);
                    call(PFcnt64Create, CN, (k + i) & 0xffff, 1, 1, i + 1);
                }
            } break;
        }
    }
    u32 seed = (u32)$len(input);
    call(PFkv64Final, KV, seed);
    call(PFcnt64Final, CN, seed);
    call(PFkv64Reopen, KV);
    call(PFcnt64Reopen, CN);
    call(PFkv64Final, KV, seed);
    call(PFcnt64Final, CN, seed);
#undef KV
#undef CN
    PUPkv64Close(kh, kh_keys);
    PUPcnt64Close(ch, ch_keys);
    kv64bUnMap(BOXkv64Mem(kh));
    cnt64bUnMap(BOXcnt64Mem(ch));
    call(FILErmrf, $path(dir));
    done;
}
