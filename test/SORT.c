#include "SORT.h"

#include <unistd.h>

#include "PRO.h"
#include "B.h"
#include "TEST.h"

//  MEM-003 regression: SORTu64's merge carves chunks of `clen` u64s; for a
//  source whose element count is not a multiple of Y_MAX_INPUTS (==64) the
//  trailing chunk of a clen>=64 level used to run past the source buffer
//  end. The clamp was applied post-hoc to the heap's physical-last slot
//  (a value copy, and not even the overrunning chunk after the heap
//  reorders by value), so the real overrunning chunk survived unclamped
//  and SORTu64next -> u8cssDownZ -> SORTu64z read up to (clen-1)*8 bytes
//  past `from`/`into`. The fix clamps each chunk at construction.
//
//  This test drives SORTu64 over trigger lengths under ASan; the property
//  asserted is "no out-of-bounds read" -- i.e. the call returns and the run
//  completes without an AddressSanitizer stack-buffer-overflow. Each `into`
//  buffer is sized to exactly the element count because SORTu64 ping-pongs
//  the merge between `into` and `from` and uses each buffer's full extent
//  as the working length.
//
//  NOTE: SORTu64's merge RESULT is independently broken (it emits only the
//  first merged element and zeroes the rest, for sorted/reverse/equal input
//  alike, even at multiple-of-64 lengths where no clamp runs) -- tracked as
//  a separate ticket. So these cases do NOT assert sort-order correctness,
//  only the memory-safety property MEM-003 is about.

#define SORTCASE(N)                                                    \
    ok64 SORT_##N() {                                                  \
        sane(1);                                                       \
        aBpad2(u64, ins, N);                                          \
        aBpad2(u64, out, N);                                          \
        u64 r = 57;                                                    \
        for (u64 i = 0; i < (N); ++i) {                                \
            u64sFeed1(insidle, i ^ r);                                 \
        }                                                             \
        call(SORTu64, outidle, insdata);                             \
        testeqv((long long)(N), (long long)($len(outdata)), "%lld");  \
        done;                                                         \
    }

//  64, 4096: clean multiples (no partial chunk). 65, 129, 917, 4097:
//  non-multiples that force a partial trailing chunk at clen>=64 -- the
//  MEM-003 trigger. 1: degenerate (no merge level runs).
SORTCASE(1)
SORTCASE(64)
SORTCASE(65)
SORTCASE(129)
SORTCASE(917)
SORTCASE(4096)
SORTCASE(4097)

ok64 SORTtest() {
    sane(1);
    call(SORT_1);
    call(SORT_64);
    call(SORT_65);
    call(SORT_129);
    call(SORT_917);
    call(SORT_4096);
    call(SORT_4097);
    done;
}

TEST(SORTtest);
