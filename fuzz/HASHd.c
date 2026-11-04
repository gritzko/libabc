//
// (c) Victor Grishchenko, 2020-2023
//
#include "INT.h"
#include "KV.h"
#include "PRO.h"
#include "TEST.h"

#define X(M, name) M##u32##name
#define ABC_HASH_CONVERGE 1
#define ABC_HASH_LINE 16  // 16-slot tables below
#define ABC_HASH_MISSSTOP 1
#include "HASHx.h"
#undef X

// HASH-26B034Benh: a presence model predicts every code (FULL: 16 live, one
// line); a FULL Put makes the layout order-dependent, so *full skips the diff
fun ok64 Play($u32 dict, u8 const *ins, size_t Size, b8 *full) {
    sane(1);
    b8 has[128] = {};
    u32 live = 0;
    *full = NO;
    for (size_t i = 0; i < Size; ++i) {
        if (ins[i] == 0 || ins[i] == 128) break;
        u32 rec = ins[i] & 127;
        if (ins[i] < 128) {
            ok64 w = has[rec] || live < 16 ? OK : FULL;
            test(w == HASHu32Put(dict, &rec), TESTFAIL);
            if (w == FULL) *full = YES;
            else if (!has[rec]) has[rec] = YES, ++live;
        } else {
            test((has[rec] ? OK : NONE) == HASHu32Del(dict, &rec), TESTFAIL);
            if (has[rec]) has[rec] = NO, --live;
        }
    }
    for (u32 k = 1; k < 128; ++k) {
        u32 rec = k;
        test((OK == HASHu32Get(&rec, dict)) == has[k], TESTFAIL);
    }
    done;
}

FUZZ(u8, HASHdfuzz) {
    sane(1);
    int Size = $size(input);
    if (Size < 2) return 0;
    srandom(**input);
    Bu32 dictbuf = {};
    u32bAlloc(dictbuf, 16);
    call(u32bFed, dictbuf, 16);
    u32$ dict = u32bData(dictbuf);
    Bu32 copybuf = {};
    u32bAlloc(copybuf, 16);

    u8 *ins = (u8 *)malloc(Size);
    u8 *orig = (u8 *)malloc(Size);
    memcpy(ins, *input, Size);
    memcpy(orig, *input, Size);

    b8 full = NO, full2 = NO;
    u32bZero(dictbuf);
    call(Play, dict, ins, Size, &full);

    $u32feedall(u32bIdle(copybuf), u32bDataC(dictbuf));

    for (int j = 0; j < 1000; ++j) {
        u32bZero(dictbuf);
        // Reset ins from original each iteration to avoid cumulative swaps
        memcpy(ins, orig, Size);
        int p = random() % (Size - 1);
        // Skip swaps involving break markers (0 or 128)
        if (ins[p] == 0 || ins[p] == 128 || ins[p + 1] == 0 || ins[p + 1] == 128)
            continue;
        // Skip if swapping insert-X with delete-X (would change semantics)
        if (ins[p] == (128 ^ ins[p + 1])) continue;
        u8Swap(ins + p, ins + p + 1);

        call(Play, dict, ins, Size, &full2);
        if (full || full2) continue;
        must($eq(u32bData(dictbuf), u32bData(copybuf)), "dict mismatch");
    }
    free(orig);

    u32bFree(dictbuf);
    u32bFree(copybuf);
    free(ins);

    return 0;
}
