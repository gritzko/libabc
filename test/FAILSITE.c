// CODE-26A24BvaL1: a failing TEST binary must name the failing check's
// FILE:LINE, not main:NN.  This binary is EXPECTED to fail; the ctest
// wrappers (test/CMakeLists.txt) run it and grep its stderr.
// FAILSITE_MODE=want fails a want() three calls deep, =test fails a
// test() there (reported via the fail-site record).  Both first
// swallow an unrelated failure, which must not be reported.
#include <stdlib.h>
#include <string.h>

#include "01.h"
#include "OK.h"
#include "PRO.h"
#include "TEST.h"

ok64 FAILSITEdecoy() {
    sane(1);
    test(0, BADARG);  // swallowed by mute() below; not the culprit
    done;
}

ok64 FAILSITEdeep(int x) {
    sane(1);
    char const *mode = getenv("FAILSITE_MODE");
    if (mode && 0 == strcmp(mode, "want")) {
        want(x == 2);
    } else {
        test(x == 2, BADARG);  // same code as the decoy
    }
    done;
}

ok64 FAILSITEmid(int x) {
    sane(1);
    call(FAILSITEdeep, x + 1);
    done;
}

ok64 FAILSITEtest() {
    sane(1);
    mute(FAILSITEdecoy(), BADARG);
    call(FAILSITEmid, 0);
    done;
}

TEST(FAILSITEtest);
