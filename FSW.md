# FSW — file system watcher

Cross-platform directory watcher using inotify (Linux) or kqueue
(macOS/BSD). Returns a pollable fd suitable for `POL.h` integration.

## API

```c
int wfd;  i32 wd;
FSWInit(&wfd);              // create watcher fd
FSWDir(wfd, path, &wd);     // watch a directory (non-recursive), get its wd
FSWPoll(wfd, timeout_ms);   // block until change or timeout
FSWDrain(wfd, cb, ctx);     // cb(wd, name, ctx) per event
FSWClose(wfd);              // close watcher and all watches
```

One watcher fd serves a whole tree: `FSWDir` returns a watch descriptor
and every drained event carries the `wd` of the dir it fell in, so the
caller's `wd -> dir` map turns a bare basename into a path (JAB-032).
Re-watching a dir returns the same `wd` — inotify collapses duplicates.

## Platform notes

| | Linux (inotify) | macOS/BSD (kqueue) |
|---|---|---|
| Watch unit | directory path | open file descriptor |
| Filename in event | yes | no (dir-level only) |
| New files auto-reported | yes | need dir rescan |
| fd cost | 1 fd + 1 watch/dir | 1 kqueue fd + 1 fd/dir |

The callback path may be empty on kqueue — use it as a wake-up
signal and rescan the directory to find actual changes.  On Linux it
is empty when the event is about the watched dir itself (it was
deleted or renamed away, or its watch died).

`wd == FSWOVERFLOW` (-1) is not a dir: it says the KERNEL DROPPED
events because its queue overflowed (`max_queued_events`, 16384 by
default).  Anything derived from this watcher is now suspect — a
consumer that caches must discard everything, not just one dir.  A
burst of 16k events is one `ninja` build inside a watched tree, so
this is a routine signal, not a corner case (JAB-032).

Watches cannot be removed individually (no per-watch bookkeeping);
they live for the watcher's lifetime.  On kqueue each watch pins an
open dir fd that stays open until process exit — FSWClose only
closes the kqueue fd itself, and that pinned fd doubles as the `wd`.
