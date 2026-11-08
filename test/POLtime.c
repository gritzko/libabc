#include "POL.h"
#include "PRO.h"
#include "TEST.h"

// STD-26B068i24G: re-tracking a timer must keep its deadline, and
// POLSetTime replaces it (CURLMOPT_TIMERFUNCTION contract, CURL.c).

static u64 t0, fired_at;
static int fires;

static u32 once_cb(u64 ns) {
    fires++;
    fired_at = POLNow();
    return 3600000;  // self-remove
}

static u32 first500_cb(u64 ns) {
    fires++;
    fired_at = POLNow();
    return fires == 1 ? 500 : 3600000;
}

static u64 ms_since(u64 t) { return (fired_at - t) / POLNanosPerMSec; }

// Timer ticked once (period 500 ms); a repeat TrackTime must not re-arm it.
ok64 POLtime_retrack() {
    sane(1);
    call(POLInit, 16);
    fires = 0;
    call(POLTrackTime, first500_cb);
    call(POLLoop, 20 * POLNanosPerMSec);
    want(fires == 1);
    t0 = fired_at;
    call(POLTrackTime, first500_cb);
    call(POLLoop, 1500 * POLNanosPerMSec);
    want(fires == 2);
    want(ms_since(t0) >= 400);
    call(POLFree);
    done;
}

// TrackTime then SetTime(500) fires once, not at ~1 ms.
ok64 POLtime_set500() {
    sane(1);
    call(POLInit, 16);
    fires = 0;
    t0 = POLNow();
    call(POLTrackTime, once_cb);
    call(POLSetTime, once_cb, 500);
    call(POLLoop, 1500 * POLNanosPerMSec);
    want(fires == 1);
    want(ms_since(t0) >= 400);
    call(POLFree);
    done;
}

// SetTime replaces in both directions: 500 then 100 fires at ~100.
ok64 POLtime_reset100() {
    sane(1);
    call(POLInit, 16);
    fires = 0;
    t0 = POLNow();
    call(POLSetTime, once_cb, 500);
    call(POLSetTime, once_cb, 100);
    call(POLLoop, 1500 * POLNanosPerMSec);
    want(fires == 1);
    want(ms_since(t0) >= 80 && ms_since(t0) < 400);
    call(POLFree);
    done;
}

// AddTime keeps its "only sooner" contract (quickjab pol.sooner).
ok64 POLtime_add_sooner() {
    sane(1);
    call(POLInit, 16);
    fires = 0;
    t0 = POLNow();
    call(POLSetTime, once_cb, 100);
    call(POLAddTime, 500);
    call(POLLoop, 1500 * POLNanosPerMSec);
    want(fires == 1);
    want(ms_since(t0) < 400);
    call(POLFree);
    done;
}

ok64 POLtimetest() {
    sane(1);
    call(POLtime_retrack);
    call(POLtime_set500);
    call(POLtime_reset100);
    call(POLtime_add_sooner);
    done;
}

TEST(POLtimetest);
