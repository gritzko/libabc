// abc/fuzz/BOX.c — BOX-26A26EbjLj: the run-buffer BOXu16 vs a naive oracle.
//
// u16 keeps the key space small, so the folds, the parallel pass and the
// ladder all fire.  c = 4 rows; the record buffer holds every input row.
// Checked after every put: runs ascending, back to back, the 1/8 ladder holds;
// at the end Get finds exactly the input set and Sort(all) gives one run equal
// to the sorted, deduplicated input.

#include "INT.h"
#include "PRO.h"
#include "TEST.h"

#define BOXx_C 8  //  4 rows
#define X(M, name) M##u16##name
#include "HITx.h"
#include "BOXx.h"
#undef X

#define MAX_INPUT_N (4096 / sizeof(u16))

FUZZ(u16, BOXfuzz) {
    sane(1);
    if ($len(input) > (i64)MAX_INPUT_N) input[1] = input[0] + MAX_INPUT_N;
    if ($empty(input)) done;

    u16 mem[MAX_INPUT_N + 1];
    u16b recs = {mem, mem, mem, mem + MAX_INPUT_N + 1};
    u16cs slots[HIT_MAX_RUNS];
    u16csb runs = {slots, slots, slots, slots + HIT_MAX_RUNS};
    u8 seen[1 << 16] = {};

    $for(u16c, p, input) {
        u16 v = *p;
        if (v == u16max) {
            must(BOXu16Put(recs, runs, &v) == BOXFAIL, "tombstone taken");
            continue;
        }
        must(BOXu16Put(recs, runs, &v) == OK, "Put");
        seen[v] = 1;
        u16 const *at = recs[0];
        a_pad(u64, lens, HIT_MAX_RUNS);
        for (u16cs *r = runs[1]; r < runs[2]; r++) {
            must((*r)[0] == at, "runs not back to back");
            for (u16 const *q = (*r)[0] + 1; q < (*r)[1]; q++)
                must(q[-1] < *q, "run not strictly ascending");
            at = (*r)[1];
            u64bFeed1(lens, (u64)$len(*r));
        }
        must(at == recs[1], "PAST != runs");
        must(HITLadderOK(u64bDataC(lens)), "ladder");
    }
    for (u32 k = 0; k < u16max; k++) {
        u16 key = (u16)k, got = 0;
        ok64 o = BOXu16Get(recs, runs, &key, &got);
        must(seen[k] ? (o == OK && got == key) : o == NONE, "Get");
    }
    must(BOXu16Sort(recs, runs, YES) == OK, "Sort all");
    must(runs[2] - runs[1] <= 1, "one run");
    u16 const *q = recs[0];
    for (u32 k = 0; k < u16max; k++)
        if (seen[k]) must(q < recs[1] && *q++ == k, "oracle");
    must(q == recs[1], "extra rows");
    done;
}
