#include "CURL.h"
#include "POL.h"
#include "PRO.h"
#include "TEST.h"

// ABC-001 deterministic regression: a firewalled / unreachable host must
// time out CLEANLY (non-zero/error completion, ZERO ASan reports) and never
// SEGV inside libcurl. Unlike CURLtest/POLtest this case does NOT depend on
// live DNS or a reachable network: it points at RFC-5737 TEST-NET addresses
// (192.0.2.0/24, guaranteed unroutable) with a forced short connect timeout,
// so the connect always fails fast and the curl/poll teardown path is
// exercised hermetically.
//
// Pre-fix, this crashed with: "AddressSanitizer: SEGV ... libcurl ...
// curl_pol_cb (CURL.c) <- POLLoop (POL.c)" at a varying wild address,
// because POLTrackTime keyed timers by the mutating `tofd` and libcurl's
// per-tick re-arm pushed DUPLICATE timer pollers, which scrambled the POL
// min-heap and eventually stamped a negative tofd onto a *file* poller,
// driving a bogus fd into curl_multi_socket_action.

static int fw_completed = 0;
static long fw_status = 0;

static void fw_on_response(CURLreq *req, long status, u8cs body) {
    (void)req;
    (void)body;
    fw_status = status;
    fw_completed++;
    trace("CURLfw completion status=%ld", status);
}

// One table row = one unreachable target. The point is that EVERY request
// completes (reaches CURLTick DONE) within the deadline with no crash; the
// HTTP status is irrelevant (a failed connect yields status 0).
typedef struct {
    const char *url;
    u32 connect_ms;
} fwcase;

static const fwcase FW_CASES[] = {
    {"http://192.0.2.1/", 300},    // TEST-NET-1, blackhole, port 80
    {"http://192.0.2.2:81/", 300}, // odd port, still unroutable
    {"http://198.51.100.7/", 300}, // TEST-NET-2
    {"http://203.0.113.9/", 300},  // TEST-NET-3
};
#define FW_N ((int)(sizeof(FW_CASES) / sizeof(FW_CASES[0])))

ok64 CURLfw_unreachable() {
    sane(1);

    call(POLInit, 64);
    call(CURLInit);

    fw_completed = 0;
    fw_status = 0;

    for (int i = 0; i < FW_N; i++)
        call(CURLGetTimed, FW_CASES[i].url, FW_CASES[i].connect_ms,
             fw_on_response, (void *)(intptr_t)i);

    // Generous backstop deadline (5s) - the per-request 300ms connect
    // timeout means real completion happens far sooner. The loop must exit
    // because all requests completed, not because the deadline elapsed.
    u64 deadline = POLNow() + 5 * POLNanosPerSec;
    while (fw_completed < FW_N && POLNow() < deadline) {
        POLLoop(50 * POLNanosPerMSec);
    }

    // Every unreachable request must have completed cleanly (no SEGV, no
    // hang). A firewalled host surfaces as a connect failure -> status 0,
    // which is fine; the load-bearing assertion is "completed, didn't crash".
    want(fw_completed == FW_N);

    CURLFree();
    POLFree();

    done;
}

// Pure-POL regression for the underlying defect: re-arming the SAME timer
// (same payload) repeatedly - exactly what libcurl's multi-timer callback
// does every tick - must keep EXACTLY ONE timer poller, never accumulate
// duplicates. No network. This is the deterministic fail-first repro for
// the heap-corruption root cause.
static u32 fw_timer_a(u64 ns) {
    (void)ns;
    return 50;
}

ok64 CURLfw_timer_no_dup() {
    sane(1);
    call(POLInit, 16);

    // Arm the same timer payload many times, with a fire in between so the
    // timer's tofd drifts off its -1 initial value (the condition that broke
    // the old tofd-based dedup).
    call(POLTrackTime, fw_timer_a);
    call(POLLoop, 60 * POLNanosPerMSec);  // let it fire at least once
    for (int i = 0; i < 32; i++) {
        call(POLTrackTime, fw_timer_a);  // re-arm: must NOT push a duplicate
    }

    // Exactly one timer poller for this payload must exist. POLIgnoreTime
    // removes one timer; after that, no timer for this payload may remain.
    want(POLAny());           // the single timer is present
    call(POLIgnoreTime);      // remove the one timer
    want(!POLAny());          // queue now empty -> there was exactly one

    call(POLFree);
    done;
}

ok64 CURLfwtest() {
    sane(1);
    call(CURLfw_timer_no_dup);
    call(CURLfw_unreachable);
    done;
}

TEST(CURLfwtest);
