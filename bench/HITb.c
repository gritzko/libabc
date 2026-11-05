//  abc/bench/HITb.c — BENCH-26B05APPSh: HITx.h is C only (a_wrap), so the
//  u64 Merge HIT.cpp benches is instantiated here; `runs` is k u64cs.
#include "INT.h"

#define X(M, name) M##u64##name
#include "HITx.h"
#undef X

ok64 hbMerge(u64 *out, size_t *len, u64cs *runs, size_t k) {
    u64s o = {out, out + *len};
    u64cs *css[2] = {runs, runs + k};
    ok64 r = HITu64Merge(o, css);
    *len = (size_t)(o[0] - out);
    return r;
}
