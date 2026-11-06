//  abc/fuzz/BOXfz.h — FUZZ-26B04IqDqU: the C side of BOXfuzz.  BOXx.h is C
//  only (a_wrap, PUP_HOUSE), so BOX.c instantiates the kv and sum lanes and
//  BOX.cpp drives them against std::map.
#ifndef ABC_FUZZ_BOXFZ_H
#define ABC_FUZZ_BOXFZ_H
#include "BOX.h"
#include "INT.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bk {
    u16 key;
    u16 val;
} bk;

#define BF_KV 0
#define BF_SUM 1
#define BF_ROWS 1024

//  `past` cells go to PAST (fewer DATA cells: BOXFULL), `cap` rows of memtable.
void bfInit(size_t past, size_t cap);
b8 bfNoRoom(int lane);
b8 bfNoCell(int lane);
b8 bfSeg(int lane);
size_t bfRuns(int lane);
//  `same`: the house and memtable bytes are as before the call.
ok64 bfPut(int lane, bk r, b8 *same);
ok64 bfBatch(int lane, bk const *rows, size_t n, b8 *same);
ok64 bfSort(int lane, b8 all, b8 *same);
ok64 bfGet(int lane, u16 key, bk *out);
//  Range [lo,hi) into `slots` cells, drained by the lane's HIT Merge.
ok64 bfRange(int lane, u16 lo, u16 hi, size_t slots, bk *out, size_t *n);

#ifdef __cplusplus
}
#endif
#endif
