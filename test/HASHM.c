//  HASH-26A275ba4V: HASHx Move (rehash into a caller-supplied table) on
//  u64 (CONVERGE, line 16), kv64 (line 64), k96v32 (CONVERGE, line 16)
#include "HASH.h"
#include "INT.h"
#include "KV.h"
#include "PRO.h"
#include "TEST.h"

#define X(M, name) M##u64##name
#define ABC_HASH_CONVERGE 1
#define ABC_HASH_LINE 16
#define ABC_HASH_MISSSTOP 1
#include "HASHx.h"
fun u64 Mu64of(u64 k, b8 val) { return k; }
#define MLINE 16
#define MCONV 1
#include "HASHMx.h"
#undef X

#define X(M, name) M##kv64##name
#define ABC_HASH_LINE 64
#include "HASHx.h"
fun kv64 Mkv64of(u64 k, b8 val) { return (kv64){k, val ? ~k : 0}; }
#define MLINE 64
#define MCONV 0
#include "HASHMx.h"
#undef X

#define X(M, name) M##k96v32##name
#define ABC_HASH_CONVERGE 1
#define ABC_HASH_LINE 16
#define ABC_HASH_MISSSTOP 1
#include "HASHx.h"
fun k96v32 Mk96v32of(u64 k, b8 val) {
    return (k96v32){k, (u32)(k >> 17), val ? (u32)k * 7 : 0};
}
#define MLINE 16
#define MCONV 1
#include "HASHMx.h"
#undef X

ok64 HASHM() {
    sane(1);
    call(Mu64test);
    call(Mkv64test);
    call(Mk96v32test);
    done;
}

TEST(HASHM);
