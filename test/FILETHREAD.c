// CODE-26B068lK6V: the per-fd FILE slots are touched only by the fd's
// holder, so threads mapping, booking, growing, buffering and unmapping
// their own files run with no locks; build with -fsanitize=thread to see races.
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "FILE.h"
#include "PRO.h"
#include "TEST.h"

#define NTHREADS 8
#define ROUNDS 32

static ok64 FILEbookRound(int id) {
    sane(1);
    char name[64];  // not PATHu8bAddTmp: its static seed is shared
    snprintf(name, sizeof(name), "/tmp/FILETHREADb.%d.%d", getpid(), id);
    a_path(path, $cstr(name));
    u8bp buf = NULL;
    call(FILEBookCreate, &buf, $path(path), 1 * MB, 4 * KB);
    want(FILEIsBooked(buf));
    u8 chunk[1000];
    memset(chunk, 'a' + id, sizeof(chunk));
    u8cs src = {chunk, chunk + sizeof(chunk)};
    for (int i = 0; i < 64; ++i) call(FILEBookFeed, buf, src);
    same(u8bDataLen(buf), 64 * sizeof(chunk));
    call(FILETrimBook, buf);
    call(FILEUnBook, buf);
    u8bp map = NULL;
    call(FILEMapRO, &map, $path(path));
    same(u8bDataLen(map), 64 * sizeof(chunk));
    same(*u8bDataC(map)[0], (u8)('a' + id));
    call(FILEUnMap, map);
    call(FILEUnLink, $path(path));
    done;
}

static ok64 FILEmapRound(int id) {
    sane(1);
    char name[64];  // not PATHu8bAddTmp: its static seed is shared
    snprintf(name, sizeof(name), "/tmp/FILETHREADm.%d.%d", getpid(), id);
    a_path(path, $cstr(name));
    u8bp map = NULL;
    call(FILEMapCreate, &map, $path(path), 4 * KB);
    call(u8bFeed1, map, (u8)id);
    call(FILEReMap, map, 16 * KB);
    same(u8bIdleLen(map), 16 * KB);
    call(FILEUnMap, map);
    call(FILEUnLink, $path(path));
    done;
}

static ok64 FILEstreamRound(int id) {
    sane(1);
    char name[64];  // not PATHu8bAddTmp: its static seed is shared
    snprintf(name, sizeof(name), "/tmp/FILETHREADs.%d.%d", getpid(), id);
    a_path(path, $cstr(name));
    int fd = FILE_CLOSED;
    call(FILECreate, &fd, $path(path));
    call(FILEBuffer, fd, 8 * KB);
    u8 chunk[1000];
    memset(chunk, 'A' + id, sizeof(chunk));
    u8cs src = {chunk, chunk + sizeof(chunk)};
    for (int i = 0; i < 64; ++i) {
        call(FILEBufFeed, fd, src);
        call(FILEFlush, &fd);
    }
    call(FILEClose, &fd);
    u8bp map = NULL;
    call(FILEMapRO, &map, $path(path));
    same(u8bDataLen(map), 64 * sizeof(chunk));
    same(u8bDataC(map)[0][63 * sizeof(chunk)], (u8)('A' + id));
    call(FILEUnMap, map);
    call(FILEUnLink, $path(path));
    done;
}

THREAD(FILEthread) {
    sane(1);
    int id = (int)(uintptr_t)arg;
    for (int r = 0; r < ROUNDS; ++r) {
        call(FILEbookRound, id);
        call(FILEmapRound, id);
        call(FILEstreamRound, id);
    }
    done;
}

ok64 FILETHREADtest() {
    sane(1);
    pthread_t t[NTHREADS];
    void *r = NULL;
    int bad = 0;
    for (int i = 0; i < NTHREADS; ++i)
        want(0 == pthread_create(t + i, NULL, FILEthread, (void *)(uintptr_t)i));
    for (int i = 0; i < NTHREADS; ++i)
        bad += pthread_join(t[i], &r) != 0 || r != NULL;
    same(bad, 0);
    done;
}

TEST(FILETHREADtest);
