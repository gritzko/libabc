// ABC-016: a failing TEST binary must exit nonzero even when the ok64
// code's low byte is 0 (raw return truncated 0x100 to shell "success").
#include "01.h"

#include "OK.h"
#include "PRO.h"
#include "TEST.h"

con ok64 LOWBYTEZERO = 0x100;

ok64 EXITtest() {
    sane(1);
    fail(LOWBYTEZERO);
    done;
}

TEST(EXITtest);
