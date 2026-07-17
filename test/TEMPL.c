// ABC-015: template hygiene pin — every container template must build at
// a non-u8 type and leave no macros (T, knobs, X) behind in the TU.
#include "DIFF.h"
#include "INT.h"
#include "LIST.h"
#include "PRO.h"
#include "TEST.h"

// NFAx at u32 with NO prior NFA.h include: NFAx.h pulls NFA.h itself,
// which used to clobber the includer's X while instantiating its u8 copy.
#define X(M, name) M##u32##name
#include "NFAx.h"
#undef X

#define X(M, name) M##u32##name
#define ABC_HASH_CONVERGE 1
#include "HASHx.h"
#undef X
#ifdef ABC_HASH_CONVERGE
#error "HASHx.h leaked ABC_HASH_CONVERGE"
#endif

#define X(M, name) M##u32##name
#include "HEAPx.h"
#undef X

#define X(M, name) M##u32##name
#include "MSETx.h"
#undef X

#define X(M, name) M##u32##name
#include "SKIPx.h"
#undef X
#ifdef SKIP_BLK_HI
#error "SKIPx.h leaked SKIP_BLK_HI"
#endif
#ifdef SKIP_NONE
#error "SKIPx.h leaked SKIP_NONE"
#endif

// HITx needs a csSwap for the entry type (array type, not in Sx.h)
fun void u32csSwap(u32cs *a, u32cs *b) {
    u32c *t0 = (*a)[0], *t1 = (*a)[1];
    (*a)[0] = (*b)[0];
    (*a)[1] = (*b)[1];
    (*b)[0] = t0;
    (*b)[1] = t1;
}

#define X(M, name) M##u32##name
#include "HITx.h"
#undef X

#define X(M, name) M##u32##name
#include "DIFFx.h"
#undef X

// LISTx wants a record type with an embedded `_list` link
typedef struct {
    u64 value;
    list64 _list;
} tent128;
fun b8 tent128Z(tent128 const *a, tent128 const *b) {
    return a->value < b->value;
}
#define X(M, name) M##tent128##name
#include "LISTx.h"
#undef X

#ifdef T
#error "a template leaked the T macro"
#endif
// names the templates used to leak must be free for TU-local use
typedef int T;
typedef int Key;

// smoke-test a few u32 instantiations actually work
ok64 TEMPL0() {
    sane(1);
    aBpad(u32, pad, 32);
    call(HEAPu32Push1, pad, 3);
    call(HEAPu32Push1, pad, 1);
    call(HEAPu32Push1, pad, 2);
    u32 v = 0;
    call(HEAPu32Pop, &v, pad);
    testeqv((long long)(v), (long long)(1), "%lld");
    done;
}

ok64 TEMPL1() {
    sane(1);
    u32 tab[16] = {};
    $u32 data = {tab, tab + 16};
    u32 k = 42;
    call(HASHu32Put, data, &k);
    u32 g = 42;
    call(HASHu32Get, &g, data);
    want(HASHNONE == HASHu32Del(data, &g) || OK == HASHu32Del(data, &g));
    done;
}

ok64 TEMPL2() {
    sane(1);
    u32 a[] = {1, 3, 5};
    u32 b[] = {2, 4, 6};
    u32cs runs[2] = {{a, a + 3}, {b, b + 3}};
    u32css iter = {runs, runs + 2};
    u32 out[6];
    $u32 into = {out, out + 6};
    call(MSETu32Merge, into, iter);
    for (int i = 0; i < 6; i++) testeqv((long long)(out[i]), (long long)(i + 1), "%lld");
    done;
}

ok64 TEMPLtest() {
    sane(1);
    call(TEMPL0);
    call(TEMPL1);
    call(TEMPL2);
    done;
}

TEST(TEMPLtest);
