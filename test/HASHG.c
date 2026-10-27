//  HASH-26A275TmIX: HASHx in-place Grow on u64, kv64, k96v32, kv128 at
//  ABC_HASH_LINE 16 and 64, with and without ABC_HASH_CONVERGE
#include "HASH.h"
#include "INT.h"
#include "KV.h"
#include "PRO.h"
#include "TEST.h"

#define X(M, name) M##u64##name
#define ABC_HASH_CONVERGE 1
#include "HASHx.h"
fun u64 Gu64of(u64 k, b8 val) { return k; }
#define GLINE 16
#define GCONV 1
#include "HASHGx.h"
#undef X

#define X(M, name) M##kv64##name
#define ABC_HASH_LINE 64
#include "HASHx.h"
fun kv64 Gkv64of(u64 k, b8 val) { return (kv64){k, val ? ~k : 0}; }
#define GLINE 64
#define GCONV 0
#include "HASHGx.h"
#undef X

#define X(M, name) M##k96v32##name
#include "HASHx.h"
fun k96v32 Gk96v32of(u64 k, b8 val) {
    return (k96v32){k, (u32)(k >> 17), val ? (u32)k * 7 : 0};
}
#define GLINE 16
#define GCONV 0
#include "HASHGx.h"
#undef X

#define X(M, name) M##kv128##name
#define ABC_HASH_LINE 64
#define ABC_HASH_CONVERGE 1
#include "HASHx.h"
fun kv128 Gkv128of(u64 k, b8 val) {
    return (kv128){{{k, ~k}}, {{val ? k * 3 : 0, val ? k ^ 5 : 0}}};
}
#define GLINE 64
#define GCONV 1
#include "HASHGx.h"
#undef X

ok64 HASHG() {
    sane(1);
    call(Gu64test);
    call(Gkv64test);
    call(Gk96v32test);
    call(Gkv128test);
    done;
}

TEST(HASHG);
