#include "INT.h"
#include "PRO.h"
#include "TEST.h"

// HITx requires u64csSwap (array type, can't use Sx.h)
fun void u64csSwap(u64cs *a, u64cs *b) {
    u64c *t0 = (*a)[0], *t1 = (*a)[1];
    (*a)[0] = (*b)[0];
    (*a)[1] = (*b)[1];
    (*b)[0] = t0;
    (*b)[1] = t1;
}

#define X(M, name) M##u64##name
#include "HITx.h"
#include "BOXx.h"
#undef X

// Helper: build a u64s buffer of N entries on the stack.
//
// 8 KB range with default 1 KB dirty + 8x ratio gives:
//   dirty = 1 KB (128 entries)
//   L0    = 1 KB (128)
//   L1    = 8 KB ... wait 1+1+8 = 10 KB > 8 KB.  So 8 KB is too small.
// Actual capacities for 8 KB range:
//   dirty 1 KB, L0 1 KB → fits.  L1 8 KB → doesn't fit alone (only 6 KB
//   left).  Top eats the 6 KB → 2 levels: dirty + L0(1KB) + L_top(7KB).
//   But Levels() only opens a level once `step <= budget`, so step=1 KB
//   passes (budget 7 KB), step=8 KB fails (budget 6 KB).  Result:
//   1 sorted level (L0 = 1 KB), then top eats the 7 KB → L0 cap=8 KB.
//
// Use a 16 KB range so we get at least 3 sorted levels for cascade
// testing: dirty 1 + L0 1 + L1 8 = 10, top eats 6 → L1 cap = 14 KB.

#define BOX_RANGE_BYTES (16 * 1024)
#define BOX_RANGE_N     (BOX_RANGE_BYTES / sizeof(u64))

// BOX0: open on a freshly-zeroed range — every Ts is empty.
ok64 BOX0() {
    sane(1);
    u64 mem[BOX_RANGE_N] = {0};
    u8s range = {(u8 *)mem, (u8 *)(mem + BOX_RANGE_N)};

    u64s slots[16];        // descriptor array (1 dirty + N sorted + 1 fence)
    u64sb box = {slots, slots, slots, slots + 16};

    call(BOXu64Open, box, range);

    // PAST has 1 (dirty), DATA has at least 1 sorted level.
    testeqv((long long)((size_t)(box[1] - box[0])), (long long)((size_t)1), "%lld");
    test((size_t)(box[2] - box[1]) >= 1, FAILSANITY);

    // Dirty empty: start == end at range start.
    testeqv((long long)((u64 *)box[0][0][0]), (long long)(mem), "%lld");
    testeqv((long long)((u64 *)box[0][0][1]), (long long)(mem), "%lld");

    // IDLE head is the fence: start == end, both at range_end.
    u64 *fence_pos = (u64 *)mem + BOX_RANGE_N;
    testeqv((long long)((u64 *)box[2][0][0]), (long long)(fence_pos), "%lld");
    testeqv((long long)((u64 *)box[2][0][1]), (long long)(fence_pos), "%lld");
    done;
}

// BOX1: feed a few entries below the dirty limit, no cascade.
ok64 BOX1() {
    sane(1);
    u64 mem[BOX_RANGE_N] = {0};
    u8s range = {(u8 *)mem, (u8 *)(mem + BOX_RANGE_N)};

    u64s slots[16];
    u64sb box = {slots, slots, slots, slots + 16};
    call(BOXu64Open, box, range);

    u64 v;
    v = 100; call(BOXu64Feed1, box, &v);
    v = 7;   call(BOXu64Feed1, box, &v);
    v = 42;  call(BOXu64Feed1, box, &v);

    // Dirty should hold three entries in arrival order.
    testeqv((long long)((size_t)(box[0][0][1] - box[0][0][0])), (long long)((size_t)3), "%lld");
    testeqv((long long)(box[0][0][0][0]), (long long)((u64)100), "%lld");
    testeqv((long long)(box[0][0][0][1]), (long long)((u64)7), "%lld");
    testeqv((long long)(box[0][0][0][2]), (long long)((u64)42), "%lld");

    // Flush and verify sorted-deduped output: 7, 42, 100.
    u64 outbuf[16] = {0};
    u64s save = {outbuf, outbuf + 16};
    u64 *save_start = save[0];
    call(BOXu64Flush, box, save);
    size_t produced = (size_t)(save[0] - save_start);
    testeqv((long long)(produced), (long long)((size_t)3), "%lld");
    testeqv((long long)(outbuf[0]), (long long)((u64)7), "%lld");
    testeqv((long long)(outbuf[1]), (long long)((u64)42), "%lld");
    testeqv((long long)(outbuf[2]), (long long)((u64)100), "%lld");

    // Dirty should be empty after flush.
    testeqv((long long)((size_t)(box[0][0][1] - box[0][0][0])), (long long)((size_t)0), "%lld");
    done;
}

// BOX2: empty Flush is a no-op (no output).
ok64 BOX2() {
    sane(1);
    u64 mem[BOX_RANGE_N] = {0};
    u8s range = {(u8 *)mem, (u8 *)(mem + BOX_RANGE_N)};

    u64s slots[16];
    u64sb box = {slots, slots, slots, slots + 16};
    call(BOXu64Open, box, range);

    u64 outbuf[4] = {0};
    u64s save = {outbuf, outbuf + 4};
    u64 *save_start = save[0];
    call(BOXu64Flush, box, save);
    testeqv((long long)((size_t)(save[0] - save_start)), (long long)((size_t)0), "%lld");
    done;
}

// BOX3: feed enough to trigger one cascade (dirty → L0).
ok64 BOX3() {
    sane(1);
    u64 mem[BOX_RANGE_N] = {0};
    u8s range = {(u8 *)mem, (u8 *)(mem + BOX_RANGE_N)};

    u64s slots[16];
    u64sb box = {slots, slots, slots, slots + 16};
    call(BOXu64Open, box, range);

    // 128 dirty entries (1 KB / 8 B) fills dirty exactly; the 129th
    // entry triggers cascade.
    for (u64 i = 1; i <= 200; i++)
        call(BOXu64Feed1, box, &i);

    u64 outbuf[256] = {0};
    u64s save = {outbuf, outbuf + 256};
    u64 *save_start = save[0];
    call(BOXu64Flush, box, save);
    size_t produced = (size_t)(save[0] - save_start);
    testeqv((long long)(produced), (long long)((size_t)200), "%lld");
    for (size_t i = 0; i < produced; i++)
        testeqv((long long)(outbuf[i]), (long long)((u64)(i + 1)), "%lld");
    done;
}

// BOX4: dedup across cascade — feed 1..50 three times in shuffled
//       order, output should be 1..50 (50 entries).
ok64 BOX4() {
    sane(1);
    u64 mem[BOX_RANGE_N] = {0};
    u8s range = {(u8 *)mem, (u8 *)(mem + BOX_RANGE_N)};

    u64s slots[16];
    u64sb box = {slots, slots, slots, slots + 16};
    call(BOXu64Open, box, range);

    for (int round = 0; round < 3; round++) {
        for (u64 i = 1; i <= 50; i++) {
            u64 v = (i * 7 + (u64)round * 11) % 50 + 1;
            call(BOXu64Feed1, box, &v);
        }
    }

    u64 outbuf[256] = {0};
    u64s save = {outbuf, outbuf + 256};
    u64 *save_start = save[0];
    call(BOXu64Flush, box, save);
    size_t produced = (size_t)(save[0] - save_start);
    testeqv((long long)(produced), (long long)((size_t)50), "%lld");
    for (size_t i = 0; i < produced; i++)
        testeqv((long long)(outbuf[i]), (long long)((u64)(i + 1)), "%lld");
    done;
}

// BOX5: re-Open after Close restores empty state.
ok64 BOX5() {
    sane(1);
    u64 mem[BOX_RANGE_N] = {0};
    u8s range = {(u8 *)mem, (u8 *)(mem + BOX_RANGE_N)};

    u64s slots[16];
    u64sb box = {slots, slots, slots, slots + 16};
    call(BOXu64Open, box, range);

    for (u64 i = 1; i <= 10; i++) call(BOXu64Feed1, box, &i);

    u64 outbuf[16] = {0};
    u64s save = {outbuf, outbuf + 16};
    call(BOXu64Close, box, save);

    // After Close: DATA empty, IDLE absorbed everything.
    testeqv((long long)((size_t)(box[2] - box[1])), (long long)((size_t)0), "%lld");

    // Re-Open and verify we can feed again.
    call(BOXu64Open, box, range);
    test((size_t)(box[2] - box[1]) >= 1, FAILSANITY);
    testeqv((long long)((size_t)(box[0][0][1] - box[0][0][0])), (long long)((size_t)0), "%lld");

    u64 v = 99;
    call(BOXu64Feed1, box, &v);
    testeqv((long long)((size_t)(box[0][0][1] - box[0][0][0])), (long long)((size_t)1), "%lld");
    testeqv((long long)(box[0][0][0][0]), (long long)((u64)99), "%lld");
    done;
}

ok64 BOXtest() {
    sane(1);
    call(BOX0);
    call(BOX1);
    call(BOX2);
    call(BOX3);
    call(BOX4);
    call(BOX5);
    done;
}

TEST(BOXtest);
