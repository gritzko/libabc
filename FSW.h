//
// FSW.h — File system watcher (inotify on Linux, kqueue on macOS)
//
// Provides a pollable fd that wakes on directory changes.
// Designed to integrate with POL.h — the watcher fd can be
// added to the event loop like any other file descriptor.
//
// Usage:
//   int wfd;
//   FSWInit(&wfd);
//   FSWDir(wfd, path);
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

// Watch a directory for changes (create, delete, modify, rename).
// path: null-terminated or u8cs. Non-recursive — watch one dir level.
ok64 FSWDir(int wfd, u8csc path);

// Remove a directory watch.
ok64 FSWUndir(int wfd, u8csc path);

// Block until a change event occurs or timeout_ms expires.
// timeout_ms < 0: block forever. 0: non-blocking check.
// Returns OK on event, FSWFAIL on error/timeout.
ok64 FSWPoll(int wfd, int timeout_ms);

// Drain pending events. Call after FSWPoll/poll returns readable.
// Calls cb(path, ctx) for each changed path (relative to watched dir).
// path may be empty on kqueue (no filename info).
typedef ok64 (*FSWcb)(u8cs path, void *ctx);
ok64 FSWDrain(int wfd, FSWcb cb, void *ctx);

// Close the watcher and all watches.
void FSWClose(int wfd);

#endif
