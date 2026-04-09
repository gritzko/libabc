//
// FSW.c — File system watcher
//

#include "FSW.h"
#include "PRO.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <unistd.h>

// ============================================================
//  Linux: inotify
// ============================================================
#ifdef __linux__

#include <sys/inotify.h>

ok64 FSWInit(int *wfd) {
    sane(wfd != NULL);
    int fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (fd < 0) return FSWFAIL;
    *wfd = fd;
    done;
}

ok64 FSWDir(int wfd, u8csc path) {
    sane(wfd >= 0 && $ok(path));
    // inotify needs null-terminated path
    char buf[4096];
    u64 len = $len(path);
    if (len >= sizeof(buf)) return FSWNOROOM;
    memcpy(buf, path[0], len);
    buf[len] = 0;

    int wd = inotify_add_watch(wfd, buf,
                                IN_CREATE | IN_DELETE | IN_MODIFY |
                                    IN_MOVED_FROM | IN_MOVED_TO |
                                    IN_CLOSE_WRITE);
    if (wd < 0) return FSWFAIL;
    done;
}

ok64 FSWUndir(int wfd, u8csc path) {
    sane(wfd >= 0);
    (void)path;
    done;
}

ok64 FSWPoll(int wfd, int timeout_ms) {
    sane(wfd >= 0);
    struct pollfd pfd = {.fd = wfd, .events = POLLIN};
    int r = poll(&pfd, 1, timeout_ms);
    if (r < 0) return FSWFAIL;
    if (r == 0) return FSWFAIL;  // timeout
    done;
}

ok64 FSWDrain(int wfd, FSWcb cb, void *ctx) {
    sane(wfd >= 0);
    u8 buf[4096]
        __attribute__((aligned(__alignof__(struct inotify_event))));

    for (;;) {
        i64 n = read(wfd, buf, sizeof(buf));
        if (n <= 0) break;

        u8 *p = buf;
        while (p < buf + n) {
            struct inotify_event *ev = (struct inotify_event *)p;
            if (cb && ev->len > 0) {
                u8cs name = {(u8c *)ev->name,
                             (u8c *)ev->name + strlen(ev->name)};
                ok64 o = cb(name, ctx);
                if (o != OK) return o;
            }
            p += sizeof(struct inotify_event) + ev->len;
        }
    }
    done;
}

void FSWClose(int wfd) {
    if (wfd >= 0) close(wfd);
}

// ============================================================
//  macOS / BSD: kqueue
// ============================================================
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
    defined(__NetBSD__)

#include <sys/event.h>
#include <sys/types.h>

ok64 FSWInit(int *wfd) {
    sane(wfd != NULL);
    int kq = kqueue();
    if (kq < 0) return FSWFAIL;
    *wfd = kq;
    done;
}

ok64 FSWDir(int wfd, u8csc path) {
    sane(wfd >= 0 && $ok(path));
    char buf[4096];
    u64 len = $len(path);
    if (len >= sizeof(buf)) return FSWNOROOM;
    memcpy(buf, path[0], len);
    buf[len] = 0;

    int fd = open(buf, O_RDONLY | O_DIRECTORY);
    if (fd < 0) return FSWFAIL;

    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_VNODE, EV_ADD | EV_CLEAR,
           NOTE_WRITE | NOTE_DELETE | NOTE_RENAME | NOTE_ATTRIB, 0, NULL);

    if (kevent(wfd, &ev, 1, NULL, 0, NULL) < 0) {
        close(fd);
        return FSWFAIL;
    }
    // fd stays open — kqueue needs it alive
    done;
}

ok64 FSWUndir(int wfd, u8csc path) {
    sane(wfd >= 0);
    (void)path;
    done;
}

ok64 FSWPoll(int wfd, int timeout_ms) {
    sane(wfd >= 0);
    struct timespec ts = {.tv_sec = timeout_ms / 1000,
                          .tv_nsec = (timeout_ms % 1000) * 1000000L};
    struct kevent ev;
    int r = kevent(wfd, NULL, 0, &ev, 1, timeout_ms < 0 ? NULL : &ts);
    if (r < 0) return FSWFAIL;
    if (r == 0) return FSWFAIL;  // timeout
    done;
}

ok64 FSWDrain(int wfd, FSWcb cb, void *ctx) {
    sane(wfd >= 0);
    struct kevent evs[64];
    struct timespec zero = {0, 0};

    for (;;) {
        int n = kevent(wfd, NULL, 0, evs, 64, &zero);
        if (n <= 0) break;
        if (cb) {
            // kqueue doesn't provide filenames — report empty path
            u8cs empty = {(u8c *)"", (u8c *)""};
            for (int i = 0; i < n; i++) {
                ok64 o = cb(empty, ctx);
                if (o != OK) return o;
            }
        }
    }
    done;
}

void FSWClose(int wfd) {
    if (wfd >= 0) close(wfd);
}

#else
#error "FSW: unsupported platform (need inotify or kqueue)"
#endif
