# FSW — file system watcher

Cross-platform directory watcher using inotify (Linux) or kqueue
(macOS/BSD). Returns a pollable fd suitable for `POL.h` integration.

## API

```c
int wfd;
FSWInit(&wfd);              // create watcher fd
FSWDir(wfd, path);          // watch a directory (non-recursive)
FSWPoll(wfd, timeout_ms);   // block until change or timeout
FSWDrain(wfd, cb, ctx);     // deliver changed filenames via callback
FSWClose(wfd);              // close watcher and all watches
```

## Platform notes

| | Linux (inotify) | macOS/BSD (kqueue) |
|---|---|---|
| Watch unit | directory path | open file descriptor |
| Filename in event | yes | no (dir-level only) |
| New files auto-reported | yes | need dir rescan |
| fd cost | 1 fd + 1 watch/dir | 1 kqueue fd + 1 fd/dir |

The callback path may be empty on kqueue — use it as a wake-up
signal and rescan the directory to find actual changes.
