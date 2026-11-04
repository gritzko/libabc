//  abc/fuzz/HITfz.h — FUZZ-26B04IqDqU: the C side of HITfuzz.  HITx.h is C
//  only (a_wrap), so HIT.c instantiates the kv and fold lanes and HIT.cpp
//  drives them against std::map/std::vector.
#ifndef ABC_FUZZ_HITFZ_H
#define ABC_FUZZ_HITFZ_H
#include "INT.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hk {
    u16 key;
    u16 val;
} hk;

//  One run, laid out as a X(,cs); `n` is the run count, HIT may compact it.
typedef struct hfrun {
    hk const *lo;
    hk const *hi;
} hfrun;

#define HF_KV 0
#define HF_FOLD 1

ok64 hfMerge(int lane, hfrun *runs, size_t *n, hk *out, size_t *len);
ok64 hfMergeBag(int lane, hfrun *runs, size_t *n, hk *out, size_t *len);
ok64 hfSeek(int lane, hfrun *runs, size_t *n, u16 key);
ok64 hfSeekRange(int lane, hfrun *runs, size_t *n, u16 lo, u16 hi);
ok64 hfIntersect(hfrun *runs, size_t *n, size_t nruns, hk *out, size_t *len);
ok64 hfIntersectMerge(hfrun *runs, size_t n, size_t half, hk *out,
                      size_t *len);
ok64 hfCompact(int lane, hfrun *runs, size_t *n, hk *into, size_t *len);
size_t hfCompactRuns(hfrun *runs, size_t n);
b8 hfIsCompact(hfrun *runs, size_t n);

#ifdef __cplusplus
}
#endif
#endif
