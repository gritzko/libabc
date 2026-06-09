#include "POL.h"

#include <fcntl.h>
#include <unistd.h>

#include "FILE.h"
#include "PRO.h"
#include "TEST.h"

// MEM-013 repro: POLLoop must not mutate a stale poller slot after a
// re-entrant callback reorders/ejects heap elements for a *different* fd.
//
// No network: we use self-pipes as pollable fds. A callback on one fd
// calls POLIgnoreEvents on another, which sUp/sDown-swaps or shrinks the
// heap. The post-callback test/clear/eject must act on the *intended* fd
// (re-resolved by fd), not on whatever poller now sits in the stale slot.
//
// POLloop_reentrant covers the poll-results loop (the deterministic
// fail-first repro). POLloop_timer_reentrant is an ASAN/sanity exercise of
// the timeout-loop re-resolution path (see its note).

// --- pipe fixtures -----------------------------------------------------

typedef struct {
    int rd;
    int wr;
} pipe2fd;

static ok64 pipe_make(pipe2fd* p) {
    int fds[2];
    if (pipe(fds) != 0) return FILEFAIL;
    p->rd = fds[0];
    p->wr = fds[1];
    return OK;
}

static void pipe_close(pipe2fd* p) {
    if (p->rd >= 0) close(p->rd);
    if (p->wr >= 0) close(p->wr);
    p->rd = p->wr = -1;
}

static void pipe_arm(pipe2fd* p) {
    u8 byte = 'x';
    ssize_t w = write(p->wr, &byte, 1);
    (void)w;
}

// --- callback bookkeeping ---------------------------------------------

// Which fd each callback was actually invoked with (to catch wrong-slot
// callback dispatch) and what action it took.
static int cb_fired_fd[8];
static int cb_fired_n = 0;

// The fd that callback #0 should eject mid-loop (the "victim").
static int victim_fd = -1;
// The fd whose callback ejects the victim (the "trigger"); the trigger
// itself asks to be removed (returns 0), so after the loop it must be
// gone and any non-victim, non-trigger fd must survive.
static int trigger_fd = -1;

static void drain(int fd) {
    u8 buf[16];
    ssize_t r = read(fd, buf, sizeof(buf));
    (void)r;
}

static short cb_record(int fd, poller* p) {
    (void)p;
    if (cb_fired_n < 8) cb_fired_fd[cb_fired_n++] = fd;
    drain(fd);  // not ready again -> single poll pass
    if (fd == trigger_fd && victim_fd >= 0) {
        // Re-entrant mutation for a *different* fd: ejects the victim,
        // which swaps heap slots out from under POLLoop's captured idx.
        POLIgnoreEvents(victim_fd);
        POLStop();  // exit after this pass; no pointless second poll-block
    }
    // request removal of self
    return 0;
}

// keep-alive callback: never asks to be removed
static short cb_keep(int fd, poller* p) {
    (void)p;
    if (cb_fired_n < 8) cb_fired_fd[cb_fired_n++] = fd;
    drain(fd);  // not ready again -> single poll pass
    return POLLIN;
}

static b8 fired(int fd) {
    for (int i = 0; i < cb_fired_n; i++)
        if (cb_fired_fd[i] == fd) return YES;
    return NO;
}

// present(fd): OK if fd still tracked, POLNONE if absent. Consumes it.
static b8 present(int fd) { return POLIgnoreEvents(fd) == OK; }

static ok64 track(int fd, pollcb cb) {
    sane(1);
    poller p = {.callback = cb, .tofd = fd, .events = POLLIN};
    call(POLTrackEvents, fd, p);
    done;
}

// Scenario: trigger + victim + survivor, all readable. The trigger's
// callback ejects the victim mid-loop; trigger asks to remove itself.
// Correct outcome: trigger gone, victim gone, survivor kept, and no
// callback fired on the wrong fd.
ok64 POLloop_reentrant() {
    sane(1);
    pipe2fd trig = {-1, -1}, vict = {-1, -1}, surv = {-1, -1};

    call(POLInit, 16);

    call(pipe_make, &trig);
    call(pipe_make, &vict);
    call(pipe_make, &surv);

    // All three readable so poll() returns them ready.
    pipe_arm(&trig);
    pipe_arm(&vict);
    pipe_arm(&surv);

    trigger_fd = trig.rd;
    victim_fd = vict.rd;

    cb_fired_n = 0;

    call(track, surv.rd, cb_keep);     // survivor: must remain
    call(track, vict.rd, cb_keep);     // victim: ejected by trigger cb
    call(track, trig.rd, cb_record);   // trigger: ejects victim, self-removes

    // One pass: poll fires, callbacks run, post-callback ejection happens.
    call(POLLoop, 10 * POLNanosPerMSec);

    // No callback may have fired on a fd we never tracked, nor must the
    // survivor's callback have been skipped due to slot confusion. The
    // load-bearing check: post-loop queue membership is exactly correct.
    want(fired(trig.rd));   // trigger callback ran on the right fd

    b8 trig_present = present(trig.rd);    // must be gone (self-removed)
    b8 vict_present = present(vict.rd);    // must be gone (ejected in cb)
    b8 surv_present = present(surv.rd);    // must remain (keep-alive)

    pipe_close(&trig);
    pipe_close(&vict);
    pipe_close(&surv);
    call(POLFree);

    // The bug: stale-idx eject removes the WRONG slot, so either the
    // survivor disappears or the trigger/victim lingers.
    want(!trig_present);   // trigger must be removed
    want(!vict_present);   // victim must be removed
    want(surv_present);    // survivor must survive

    done;
}

// --- timer (timeout-loop) re-entrancy smoke -------------------------

// The timeout loop (deadline<=now timer dispatch) has the same shape: it
// captured the root `at` before the callback and re-derefed it after. A
// timer whose callback mutates the poll set (inserts/ejects pollers) must
// not corrupt the heap. A clean *failing* repro here is impractical via the
// public API: the min-heap self-heals over turns and timers re-fire
// periodically, so the wrong-slot write does not survive as an observable
// count. This case is therefore an ASAN/sanity exercise of the timeout-loop
// re-resolution path (mixed timers + a re-entrant insert), not a fail-first
// repro; the timer-path correctness itself is covered by POLtest1.
static int tfire = 0;

static short t_oneshot(int fd, poller* p) {
    (void)fd;
    tfire++;
    p->callback = NULL;  // one-shot
    return 0;
}

static b8 t_inserted = NO;
static short t_insert(int fd, poller* p) {
    (void)fd;
    tfire++;
    p->callback = NULL;  // self-clear
    if (!t_inserted) {
        t_inserted = YES;
        // Re-entrant insert with an earlier deadline -> sUp-swaps to root.
        poller c = {.callback = t_oneshot, .tofd = -1};
        POLTrackEvents(-1, c);
    }
    if (tfire >= 8) POLStop();  // guard against any pathological spin
    return 0;
}

ok64 POLloop_timer_reentrant() {
    sane(1);
    call(POLInit, 16);

    tfire = 0;
    t_inserted = NO;

    poller a = {.callback = t_insert, .tofd = -2};   // due first (now+2ms)
    poller b = {.callback = t_oneshot, .tofd = -9};  // far child (now+9ms)
    call(POLTrackEvents, -2, a);
    call(POLTrackEvents, -9, b);

    // Must terminate cleanly under ASAN with no heap corruption / spin.
    call(POLLoop, 50 * POLNanosPerMSec);
    want(tfire >= 1 && tfire < 8);

    call(POLFree);
    done;
}

ok64 POLlooptest() {
    sane(1);
    call(POLloop_reentrant);
    call(POLloop_timer_reentrant);
    done;
}

TEST(POLlooptest);
