// abc/fuzz/BOX.c — FUZZ-26B04IqDqU: the BOX instances BOXfuzz drives.
//
// Two lanes over the same {u16 key, u16 val} row: kv (the youngest wins) and
// sum (ABC_X_MERGE adds the vals), each with its own house and memtable.
// The ops snapshot the house and the memtable bytes so the driver can see
// that a failed write changes nothing (wiki/Containers.mkd "Errors").

#include "BOXfz.h"
#include "PRO.h"

typedef bk bc;
fun b8 bkZ(bk const *a, bk const *b) { return a->key < b->key; }
fun b8 bkKeyEq(bk const *a, bk const *b) { return a->key == b->key; }
con bk bkMax = {u16max, u16max};
fun b8 bcZ(bc const *a, bc const *b) { return a->key < b->key; }
fun b8 bcKeyEq(bc const *a, bc const *b) { return a->key == b->key; }
fun void bcMerge(bc *acc, bc const *nxt) { acc->val += nxt->val; }
con bc bcMax = {u16max, u16max};

#define ABC_BOX_SEG (4 * sizeof(bk))
#define X(M, name) M##bk##name
#include "Bx.h"
#include "QSORTx.h"
#include "BOXx.h"
#undef X

#define ABC_BOX_SEG (4 * sizeof(bc))
#define X(M, name) M##bc##name
#include "Bx.h"
#include "QSORTx.h"
#define ABC_X_MERGE
#include "BOXx.h"
#undef X

#define BF_CELLS ABC_HIT_MAX_RUNS

//  One lane: the house, its memtable rows, the instance's ops.
typedef struct {
    bkcs cells[BF_CELLS];
    bkcs *h[4];
    bk rows[BF_ROWS];
    typeof(BOXbkPut) *put;
    typeof(BOXbkPutBatch) *batch;
    typeof(BOXbkSort) *sort;
    typeof(BOXbkGet) *get;
    typeof(BOXbkRange) *range;
    typeof(HITbkMerge) *merge;
} bflane;

//  The rows compared are PAST and DATA: a refused Put may scribble on IDLE.
typedef struct {
    bkcs cells[BF_CELLS];
    bkcs *h[4];
    bk rows[BF_ROWS];
    size_t len;
} bfsnap;

static bflane LANES[2] = {
    {.put = BOXbkPut, .batch = BOXbkPutBatch, .sort = BOXbkSort,
     .get = BOXbkGet, .range = BOXbkRange, .merge = HITbkMerge},
    {.put = BOXbcPut, .batch = BOXbcPutBatch, .sort = BOXbcSort,
     .get = BOXbcGet, .range = BOXbcRange, .merge = HITbcMerge},
};
static bfsnap SNAP;

void bfInit(size_t past, size_t cap) {
    for (int i = 0; i < 2; i++) {
        bflane *l = LANES + i;
        memset(l->cells, 0, sizeof(l->cells));
        memset(l->rows, 0, sizeof(l->rows));
        l->h[0] = l->cells;
        l->h[1] = l->h[2] = l->cells + past;
        l->h[3] = l->cells + BF_CELLS - 2;
        bk **m = BOXbkMem(l->h);
        m[0] = m[1] = m[2] = l->rows;
        m[3] = l->rows + cap;
    }
}

b8 bfNoRoom(int lane) {
    bk **m = BOXbkMem(LANES[lane].h);
    return m[2] == m[3];
}

b8 bfNoCell(int lane) { return LANES[lane].h[2] == LANES[lane].h[3]; }

b8 bfSeg(int lane) {
    bk **m = BOXbkMem(LANES[lane].h);
    return m[2] > m[1];
}

size_t bfRuns(int lane) {
    return (size_t)(LANES[lane].h[2] - LANES[lane].h[1]);
}

static void bf_snap(bflane const *l) {
    memcpy(SNAP.cells, l->cells, sizeof(l->cells));
    memcpy(SNAP.h, l->h, sizeof(l->h));
    SNAP.len = (size_t)(BOXbkMem((bkcs **)l->h)[2] - l->rows);
    memcpy(SNAP.rows, l->rows, SNAP.len * sizeof(bk));
}

static b8 bf_same(bflane const *l) {
    return !memcmp(SNAP.cells, l->cells, sizeof(l->cells)) &&
           !memcmp(SNAP.h, l->h, sizeof(l->h)) &&
           !memcmp(SNAP.rows, l->rows, SNAP.len * sizeof(bk));
}

ok64 bfPut(int lane, bk r, b8 *same) {
    bflane *l = LANES + lane;
    bf_snap(l);
    ok64 o = l->put(l->h, &r);
    *same = bf_same(l);
    return o;
}

ok64 bfBatch(int lane, bk const *rows, size_t n, b8 *same) {
    bflane *l = LANES + lane;
    bf_snap(l);
    bkcs s = {rows, rows + n};
    ok64 o = l->batch(l->h, s);
    *same = bf_same(l);
    return o;
}

ok64 bfSort(int lane, b8 all, b8 *same) {
    bflane *l = LANES + lane;
    bf_snap(l);
    ok64 o = l->sort(l->h, all);
    *same = bf_same(l);
    return o;
}

ok64 bfGet(int lane, u16 key, bk *out) {
    bk k = {key, (u16)~key};
    return LANES[lane].get(out, LANES[lane].h, &k);
}

ok64 bfRange(int lane, u16 lo, u16 hi, size_t slots, bk *out, size_t *n) {
    bflane *l = LANES + lane;
    a_pad(bkcs, into, BF_CELLS);
    bkcs **ii = into_idle;
    if (slots < BF_CELLS) ii[1] = ii[0] + slots;
    bk klo = {lo, 1}, khi = {hi, 2};
    ok64 o = l->range(ii, l->h, &klo, &khi);
    *n = 0;
    if (o != OK) return o;
    bks o2 = {out, out + BF_ROWS};
    o = l->merge(o2, ii);
    *n = (size_t)(o2[0] - out);
    return o;
}
