#include "POL.h"

#include <time.h>

#include "B.h"
#include "INT.h"
#include "OK.h"
#include "PRO.h"
#include "RON.h"

fun b8 pollerZ(poller const* a, poller const* b) {
    if (a->callback == NULL) return NO;
    if (b->callback == NULL) return YES;
    return a->deadline < b->deadline;
}

#define X(M, name) M##poller##name
#include "Bx.h"
#undef X

#define X(M, name) M##poller##name
#include "HEAPx.h"
#undef X

thread_local Bpoller POL_QUEUE = {};
thread_local b8 POL_STOP = NO;
thread_local int POL_MAXFD = 0;

int POLMaxFiles() { return POL_MAXFD; }

b8 POLAny() { return !pollerbEmpty(POL_QUEUE); }

ok64 POLInit(int max_fd) {
    pollerbAllocate(POL_QUEUE, max_fd);
    POL_MAXFD = max_fd;
    return OK;
}

ok64 POLFree() {
    pollerbFree(POL_QUEUE);
    POL_STOP = NO;
    POL_MAXFD = 0;
    return OK;
}

ok64 POLStop() {
    POL_STOP = YES;
    return OK;
}

// Find poller by tofd in heap, returns index or -1
fun int POLFind(int tofd) {
    poller** data = pollerbData(POL_QUEUE);
    size_t len = $len(data);
    for (size_t i = 0; i < len; i++) {
        if ((*data)[i].tofd == tofd) return (int)i;
    }
    return -1;
}

// Re-resolve a poller after a re-entrant callback that may have
// sUp/sDown-swapped or ejected heap slots. `key` is the pre-callback
// snapshot. File pollers keep a stable tofd; timer callbacks rewrite
// tofd, so timers are matched by their stable (callback,payload) pair.
// Returns the current index, or -1 if the poller is gone.
fun int POLRefind(poller const* key) {
    poller** data = pollerbData(POL_QUEUE);
    size_t len = $len(data);
    b8 timer = key->tofd < 0;
    for (size_t i = 0; i < len; i++) {
        poller const* p = *data + i;
        if (timer) {
            if (p->callback == key->callback && p->payload == key->payload &&
                p->tofd < 0)
                return (int)i;
        } else if (p->tofd == key->tofd) {
            return (int)i;
        }
    }
    return -1;
}

ok64 POLTrackEvents(int fd, poller p) {
    sane(p.callback != NULL);  // tofd can be 0 for stdin
    u64 now = POLNow();
    p.deadline = now + (p.tofd < 0 ? -p.tofd : 1000) * POLNanosPerMSec;

    // Check if already tracked
    int idx = POLFind(p.tofd);
    if (idx >= 0) {
        // Update existing entry
        poller** data = pollerbData(POL_QUEUE);
        (*data)[idx] = p;
        pollersUpAtZ(data, idx, pollerZ);
        pollersDownAtZ(data, idx, pollerZ);
    } else {
        // Add new entry
        call(HEAPpollerPushZ, POL_QUEUE, &p, pollerZ);
    }
    done;
}

ok64 POLAddEvents(int fd, short events) {
    int idx = POLFind(fd);
    if (idx < 0) return POLNONE;
    poller** data = pollerbData(POL_QUEUE);
    (*data)[idx].events |= events;
    return OK;
}

ok64 POLIgnoreEvents(int fd) {
    int idx = POLFind(fd);
    if (idx < 0) return POLNONE;
    return HEAPpollerEjectAtZ(POL_QUEUE, idx, pollerZ);
}

fun int POLFindTimer(void* payload);

short POLTimer(int fd, struct poller* p) {
    if (p->callback == NULL || p->payload == NULL) return 0;
    void* payload = p->payload;  // stable identity; survives heap reorder
    timercb timer = payload;
    u32 ms = timer(p->deadline);
    // The user timer callback may have re-entered POL (POLTrackTime /
    // POLAddTime / POLIgnoreEvents from libcurl's multi-timer callback) and
    // sUp/sDown-swapped or ejected heap slots, so `p` may no longer point at
    // THIS timer -- it could now alias a file (curl socket) poller. Writing
    // `p->tofd` blindly here is the residual ABC-001 corruption: it stamps a
    // negative tofd onto a file poller, which is then driven as a bogus fd
    // into curl_multi_socket_action (wild-pointer SEGV). Re-resolve our own
    // slot by stable (POLTimer, payload) identity before mutating it.
    poller** data = pollerbData(POL_QUEUE);
    int idx = POLFindTimer(payload);
    if (idx < 0) return 0;  // we were ejected during the callback
    poller* self = *data + idx;
    // If the timer requested "never" (>= 1 hour), clear it for removal.
    if (ms >= 3600000) {
        self->callback = NULL;
        return 0;
    }
    self->tofd = -(int)ms;  // negative for timer period
    return 0;
}

ok64 POLTrackTime(timercb callback) {
    sane(callback != NULL);
    // A POLTimer poller's tofd drifts to -period_ms on every fire, so it is
    // NOT a stable key. Dedup by the timer's stable (POLTimer, payload)
    // identity, never by tofd: matching by tofd lets a repeated TrackTime
    // (e.g. libcurl re-arming its multi timer every tick) push DUPLICATE
    // timer pollers, which then collide in POLRefind, scramble the heap and
    // ultimately stamp a negative tofd onto a *file* poller -> a bogus fd is
    // driven into curl_multi_socket_action (wild-pointer SEGV). One timer
    // per payload, always.
    u64 now = POLNow();
    int idx = POLFindTimer((void *)callback);
    if (idx >= 0) {
        // Already tracked: re-arm at the -1ms initial cadence, re-heapify.
        poller **data = pollerbData(POL_QUEUE);
        poller *t = *data + idx;
        t->tofd = -1;
        t->deadline = now + 1 * POLNanosPerMSec;
        pollersUpAtZ(data, (size_t)idx, pollerZ);
        pollersDownAtZ(data, (size_t)idx, pollerZ);
        done;
    }
    poller t = {
        .callback = &POLTimer,
        .payload = (void *)callback,
        .tofd = -1,  // -1ms initial
        .deadline = now + 1 * POLNanosPerMSec,
    };
    call(HEAPpollerPushZ, POL_QUEUE, &t, pollerZ);
    done;
}

// Find timer by callback payload (since timers use POLTimer as callback)
fun int POLFindTimer(void* payload) {
    poller** data = pollerbData(POL_QUEUE);
    size_t len = $len(data);
    for (size_t i = 0; i < len; i++) {
        if ((*data)[i].callback == POLTimer && (*data)[i].payload == payload)
            return (int)i;
    }
    return -1;
}

ok64 POLAddTime(int ms) {
    // For legacy single-timer API, find any timer
    poller** data = pollerbData(POL_QUEUE);
    size_t len = $len(data);
    for (size_t i = 0; i < len; i++) {
        if ((*data)[i].tofd >= 0) continue;  // skip file pollers
        poller* t = &(*data)[i];
        t->tofd = -ms;
        u64 now = POLNow();
        u64 deadline = now + ms * POLNanosPerMSec;
        if (t->deadline < deadline) return OK;
        t->deadline = deadline;
        pollersUpAtZ(data, i, pollerZ);
        return OK;
    }
    return POLNONE;
}

ok64 POLIgnoreTime() {
    // Remove first timer found
    poller** data = pollerbData(POL_QUEUE);
    size_t len = $len(data);
    for (size_t i = 0; i < len; i++) {
        if ((*data)[i].tofd < 0) {
            return HEAPpollerEjectAtZ(POL_QUEUE, i, pollerZ);
        }
    }
    return POLNONE;
}

ok64 POLSleep(u64 ns) {
    struct timespec duration = {.tv_sec = ns / POLNanosPerSec,
                                .tv_nsec = ns % POLNanosPerSec};
    nanosleep(&duration, NULL);
    return OK;
}

ok64 POLLoop(u64 timens) {
    u64 now = POLNow();
    u64 till = timens == POLNever ? POLNever : now + timens;

    // Local poll vector - exactly POL_MAXFD entries
    struct pollfd* vec = calloc(POL_MAXFD, sizeof(struct pollfd));
    if (!vec) return NOROOM;

    while (now < till && !pollerbEmpty(POL_QUEUE) && !POL_STOP) {
        now = POLNow();
        poller** data = pollerbData(POL_QUEUE);

        // Deliver timeouts and remove dead pollers
        while (!$empty(data) && (*data)->deadline <= now) {
            poller* at = *data;
            if (at->callback == NULL) {
                poller dummy;
                HEAPpollerPopZ(&dummy, POL_QUEUE, pollerZ);
                data = pollerbData(POL_QUEUE);
                continue;
            }
            at->deadline = now;
            poller key = *at;  // stable identity snapshot for re-resolution
            at->callback(key.tofd, at);
            // The callback may have sUp/sDown-swapped or ejected slots for
            // other pollers, so `at`/root are stale now: re-resolve by stable
            // identity before testing/clearing/re-heapifying.
            int idx = POLRefind(&key);
            if (idx < 0) continue;  // callback already removed us
            data = pollerbData(POL_QUEUE);
            at = *data + idx;
            // Remove if callback cleared itself
            if (at->callback == NULL) {
                HEAPpollerEjectAtZ(POL_QUEUE, idx, pollerZ);
                data = pollerbData(POL_QUEUE);
                continue;
            }
            int period_ms = at->tofd < 0 ? -at->tofd : 1000;
            at->deadline = now + period_ms * POLNanosPerMSec;
            pollersUpAtZ(data, idx, pollerZ);
            pollersDownAtZ(data, idx, pollerZ);
        }

        if ($empty(data)) break;

        // Remove file pollers with no events
        for (size_t i = 0; i < $len(data);) {
            poller* p = *data + i;
            if (p->tofd >= 0 && p->events == 0) {
                HEAPpollerEjectAtZ(POL_QUEUE, i, pollerZ);
                data = pollerbData(POL_QUEUE);
            } else {
                i++;
            }
        }
        if ($empty(data)) break;

        u64 next_timeout = (*data)->deadline;

        // Build poll vector from file descriptors (positive tofd)
        size_t len = $len(data);
        int pollscount = 0;
        for (size_t i = 0; i < len && pollscount < POL_MAXFD; i++) {
            poller* p = *data + i;
            if (p->tofd < 0 || p->events == 0) continue;
            vec[pollscount++] = (struct pollfd){.fd = p->tofd, .events = p->events};
        }

        if (pollscount == 0) {
            // No file descriptors, just timers
            u64 sleep = next_timeout - now;
            if (sleep > POLNanosPerMonth) break;
            POLSleep(sleep);
            continue;
        }

        int pollms = (next_timeout - now) / POLNanosPerMSec;
        poll(vec, pollscount, pollms);

        // Process poll results
        for (int i = 0; i < pollscount; i++) {
            if (!vec[i].revents) continue;
            int fd = vec[i].fd;
            int idx = POLFind(fd);
            if (idx < 0) continue;
            data = pollerbData(POL_QUEUE);
            poller* at = *data + idx;
            if (at->callback == NULL) continue;
            at->revents = vec[i].revents;
            short events = at->callback(fd, at);
            // The callback may have sUp/sDown-swapped or ejected slots for
            // other fds (POLTrackEvents/POLIgnoreEvents/POLAddTime), so idx/at
            // are stale now: re-resolve this fd before testing/clearing/ejecting.
            idx = POLFind(fd);
            if (idx < 0) continue;  // callback already removed us
            data = pollerbData(POL_QUEUE);
            at = *data + idx;
            // Remove if callback cleared itself or no events requested
            if (at->callback == NULL || (at->tofd >= 0 && events == 0)) {
                HEAPpollerEjectAtZ(POL_QUEUE, idx, pollerZ);
            } else {
                at->events = events;
                int period_ms = at->tofd < 0 ? -at->tofd : 1000;
                at->deadline = now + period_ms * POLNanosPerMSec;
            }
        }

    }
    free(vec);
    return OK;
}

