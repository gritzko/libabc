//
// FSW.h — File system watcher (inotify on Linux, kqueue on macOS)
//
// Provides a pollable fd that wakes on directory changes.
// Designed to integrate with POL.h — the watcher fd can be
// added to the event loop like any other file descriptor.
//
// Usage:
//   int wfd;  i32 wd;
//   FSWInit(&wfd);
//   FSWDir(wfd, path, &wd);   // one wfd watches many dirs; wd names each
//   // poll wfd for POLLIN, or:
//   FSWPoll(wfd, timeout_ms);
//   FSWClose(wfd);
//

#ifndef ABC_FSW_H
#define ABC_FSW_H

#include "OK.h"
#include "BUF.h"

con ok64 FSWFAIL = 0xf7203ca495;
con ok64 FSWNOROOM = 0xf7205d86d8616;

// Create a watcher. Returns a pollable fd via *wfd.
ok64 FSWInit(int *wfd);

// Watch a directory for changes (create, delete, modify, rename, and the
// dir itself going away). path: null-terminated or u8cs. Non-recursive —
// watch one dir level. Returns the watch descriptor via *wd: it names the
// dir in every FSWDrain record, so ONE wfd serves a whole tree (JAB-032).
// Re-watching a dir returns the SAME wd (inotify collapses duplicates).
ok64 FSWDir(int wfd, u8csc path, i32 *wd);

//  ABC-013: FSWUndir was a no-op lie on both platforms — deleted.  Watches
//  live for the watcher's lifetime; on kqueue each watch pins a dir fd.

// Block until a change event occurs or timeout_ms expires.
// timeout_ms < 0: block forever. 0: non-blocking check.
// Returns OK on event, FSWFAIL on error/timeout.
ok64 FSWPoll(int wfd, int timeout_ms);

// Drain pending events. Call after FSWPoll/poll returns readable.
// Calls cb(wd, path, ctx) for each event; wd is the FSWDir descriptor of
// the dir it fell in, path the BARE basename inside that dir.
// path may be empty: on kqueue always (no filename info — rescan the dir),
// on Linux when the event is about the watched dir itself.
// JAB-032: wd == FSWOVERFLOW means the KERNEL DROPPED events (inotify's
// queue overflowed) — every cache over this watcher is now suspect.
con i32 FSWOVERFLOW = -1;
typedef ok64 (*FSWcb)(i32 wd, u8cs path, void *ctx);
ok64 FSWDrain(int wfd, FSWcb cb, void *ctx);

// Close the watcher and all watches.
void FSWClose(int wfd);

#endif
