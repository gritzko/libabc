#include "SORT.h"

#include <unistd.h>

#include "PRO.h"
#include "B.h"
#include "TEST.h"

//  MEM-003: trigger lengths force a partial trailing chunk at clen>=64;
//  the property is "no OOB read" under ASan (chunks clamp at creation).
//  ABC-021: the merge result used to be garbage (Yx tie-loop guard had
//  swapped args, SORTu64y ate duplicates, ping-pong passes used whole
//  buffer extents); every case now also checks the RESULT against
//  u64sSort, with `into` deliberately larger than the element count.

//  fill patterns: 0 distinct, 1 all-equal, 2 a >64-long equal run
//  interleaved with distinct values (tie-pad flush, ABC-021)
static u64 SORTfill(u64 i, int pat) {
    if (pat == 1) return 0x2222222222222222ULL;
    if (pat == 2) return (i % 3) ? 0xffffffffffffffffULL : i * 57;
    return i ^ 57;
}

#define SORTCASE(N, PAT)                                               \
    ok64 SORT_##N##_##PAT() {                                          \
        sane(1);                                                       \
        aBpad2(u64, ins, N);                                           \
        aBpad2(u64, ref, N);                                           \
        aBpad2(u64, out, (N) + 37);                                    \
        for (u64 i = 0; i < (N); ++i) {                                \
            u64sFeed1(insidle, SORTfill(i, PAT));                      \
            u64sFeed1(refidle, SORTfill(i, PAT));                      \
        }                                                              \
        u64sSort(refdata);                                             \
        call(SORTu64, outidle, insdata);                               \
        testeqv((long long)(N), (long long)($len(outdata)), "%lld");   \
        for (u64 i = 0; i < (N); ++i) {                                \
            want(u64sAt(outdata, i) == u64sAt(refdata, i));            \
        }                                                              \
        done;                                                          \
    }

//  1: degenerate. 64, 4096: clean multiples (no partial chunk).
//  65, 129, 917, 4097: MEM-003 partial-chunk triggers; 65 also hits
//  the ABC-021 ping-pong extent bug. 2/pat1: minimal duplicate pair
//  (the CI SORTfuzz crash input). 110/pat2, 4097/pat2: >64 equal
//  runs overflow the Y_MAX_INPUTS tie pad.
SORTCASE(1, 0)
SORTCASE(2, 1)
SORTCASE(64, 0)
SORTCASE(65, 0)
SORTCASE(110, 2)
SORTCASE(129, 0)
SORTCASE(917, 0)
SORTCASE(4096, 0)
SORTCASE(4097, 0)
SORTCASE(4097, 2)

ok64 SORTtest() {
    sane(1);
    call(SORT_1_0);
    call(SORT_2_1);
    call(SORT_64_0);
    call(SORT_65_0);
    call(SORT_110_2);
    call(SORT_129_0);
    call(SORT_917_0);
    call(SORT_4096_0);
    call(SORT_4097_0);
    call(SORT_4097_2);
    done;
}

TEST(SORTtest);
