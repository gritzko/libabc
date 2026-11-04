// abc/fuzz/HIT.c — FUZZ-26B04IqDqU: the HIT instances HITfuzz drives.
//
// Two lanes over the same {u16 key, u16 val} row: kv (the youngest run's row
// wins) and fold (ABC_X_MERGE): val is an affine map x -> a*x+b over u8, the
// fold composes; associative as LSM needs, not commutative, so order shows.
// Each wrapper takes the runs as a hfrun array, lets HIT compact it in place
// and reports the new count; `len` is in/out.

#include "HITfz.h"
#include "PRO.h"

typedef hk hc;
fun b8 hkZ(hk const *a, hk const *b) { return a->key < b->key; }
fun b8 hkKeyEq(hk const *a, hk const *b) { return a->key == b->key; }
fun b8 hcZ(hc const *a, hc const *b) { return a->key < b->key; }
fun b8 hcKeyEq(hc const *a, hc const *b) { return a->key == b->key; }
fun void hcMerge(hc *acc, hc const *nxt) {
    u8 a = (u8)(acc->val >> 8), b = (u8)acc->val;
    u8 na = (u8)(nxt->val >> 8), nb = (u8)nxt->val;
    acc->val = (u16)((u8)(na * a) << 8 | (u8)(na * b + nb));
}

#define X(M, name) M##hk##name
#include "Bx.h"
#include "QSORTx.h"
#include "HITx.h"
#undef X

#define X(M, name) M##hc##name
#include "Bx.h"
#include "QSORTx.h"
#define ABC_X_MERGE
#include "HITx.h"
#undef X

#define HF_CSS(r, n) hkcs *css[2] = {(hkcs *)(r), (hkcs *)(r) + *(n)}

static ok64 hf_out(ok64 (*f)(hks, hkcs **), hfrun *runs, size_t *n, hk *out,
                   size_t *len) {
    HF_CSS(runs, n);
    hks o = {out, out + *len};
    ok64 r = f(o, css);
    *len = (size_t)(o[0] - out);
    *n = (size_t)(css[1] - css[0]);
    return r;
}

ok64 hfMerge(int lane, hfrun *runs, size_t *n, hk *out, size_t *len) {
    return hf_out(lane ? HIThcMerge : HIThkMerge, runs, n, out, len);
}

ok64 hfMergeBag(int lane, hfrun *runs, size_t *n, hk *out, size_t *len) {
    return hf_out(lane ? HIThcMergeBag : HIThkMergeBag, runs, n, out, len);
}

ok64 hfSeek(int lane, hfrun *runs, size_t *n, u16 key) {
    HF_CSS(runs, n);
    hk k = {key, (u16)~key};
    ok64 o = lane ? HIThcSeek(css, &k) : HIThkSeek(css, &k);
    *n = (size_t)(css[1] - css[0]);
    return o;
}

ok64 hfSeekRange(int lane, hfrun *runs, size_t *n, u16 lo, u16 hi) {
    HF_CSS(runs, n);
    hk l = {lo, 1}, h = {hi, 2};
    ok64 o = lane ? HIThcSeekRange(css, &l, &h) : HIThkSeekRange(css, &l, &h);
    *n = (size_t)(css[1] - css[0]);
    return o;
}

ok64 hfIntersect(hfrun *runs, size_t *n, size_t nruns, hk *out, size_t *len) {
    HF_CSS(runs, n);
    hks o = {out, out + *len};
    ok64 r = HIThkIntersect(o, css, nruns);
    *len = (size_t)(o[0] - out);
    *n = (size_t)(css[1] - css[0]);
    return r;
}

ok64 hfIntersectMerge(hfrun *runs, size_t n, size_t half, hk *out,
                      size_t *len) {
    hkcs *g[2][2] = {{(hkcs *)runs, (hkcs *)runs + half},
                     {(hkcs *)runs + half, (hkcs *)runs + n}};
    hkcss *oh[2] = {g, g + 2};
    hks o = {out, out + *len};
    ok64 r = HIThksIntersectMerge(o, oh);
    *len = (size_t)(o[0] - out);
    return r;
}

ok64 hfCompact(int lane, hfrun *runs, size_t *n, hk *into, size_t *len) {
    HF_CSS(runs, n);
    hks o = {into, into + *len};
    ok64 r = lane ? HIThcCompact(css, o) : HIThkCompact(css, o);
    *len = (size_t)(o[0] - into);
    *n = (size_t)(css[1] - css[0]);
    return r;
}

size_t hfCompactRuns(hfrun *runs, size_t n) {
    HF_CSS(runs, &n);
    return HIThkCompactRuns(css);
}

b8 hfIsCompact(hfrun *runs, size_t n) {
    HF_CSS(runs, &n);
    return HIThkIsCompact(css);
}
