//  CODE-26A23C8gS9, CODE-26A23CSStk: crash durability of a run.
//  (1) a durable create fsyncs the run before its rename and the directory
//      after it; the plain create keeps the fast path (no fsync at all);
//  (2) PUPOpenAll never returns OK with a run missing: a 0-byte run (what a
//      crashed non-durable create leaves), an fd budget too small for the
//      stack, and more runs than one scan holds all fail.
#include "PUPS.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "FILE.h"
#include "PRO.h"
#include "TEST.h"

#include "TESTBE.h"

//  fsync shim: libabc is linked statically, so FILESync's fsync() binds to
//  this definition; it counts file and directory syncs, then syncs for real.
static int dur_file_syncs, dur_dir_syncs;

int fsync(int fd) {
    struct stat st;
    if (fstat(fd, &st) == 0) {
        if (S_ISDIR(st.st_mode)) dur_dir_syncs++;
        else dur_file_syncs++;
    }
    int (*real)(int) = (int (*)(int))dlsym(RTLD_NEXT, "fsync");
    return real ? real(fd) : 0;
}

static void dur_reset(void) { dur_file_syncs = dur_dir_syncs = 0; }

//  Write `len` bytes of junk onto `<dir>/<key><ext>`, bypassing PUPS.
static ok64 dur_put(char const *tmp, u8cs ext, u64 key, size_t len) {
    sane(tmp != NULL);
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    a_pad(u8, name, PUP_SEQNO_W + 32);
    call(RONu8sFeedPad, u8bIdle(name), (ok64)key, PUP_SEQNO_W);
    call(u8bFeed, name, ext);
    a_path(fp, $path(dir), u8bDataC(name));
    int fd = open((char *)u8bDataHead(fp), O_CREAT | O_WRONLY | O_TRUNC, 0600);
    want(fd >= 0);
    char row[16] = "sixteen-byte-row";
    for (size_t i = 0; i < len; i += sizeof row)
        want(write(fd, row, len - i < sizeof row ? len - i : sizeof row) > 0);
    close(fd);
    done;
}

// --- (1) durable create --------------------------------------------

static ok64 PUPDurCreate(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    a_cstr(ext, ".dur.idx");
    a_cstr(body, "sixteen-byte-row");
    a_dup(u8c, b, body);
    Bkv64 pups = {};
    call(kv64bAllocate, pups, 8);

    dur_reset();
    call(PUPCreateAt, pups, $path(dir), ext, b, 1000);
    call(PUPCreate, pups, $path(dir), ext, b);
    want(dur_file_syncs == 0 && dur_dir_syncs == 0);  //  the fast path

    dur_reset();
    call(PUPCreateAtAs, pups, $path(dir), ext, b, 1001, YES);
    want(dur_file_syncs == 1 && dur_dir_syncs == 1);
    dur_reset();
    call(PUPCreateAs, pups, $path(dir), ext, b, YES);
    want(dur_file_syncs == 1 && dur_dir_syncs == 1);
    dur_reset();
    call(PUPCreateAs, pups, $path(dir), ext, b, NO);
    want(dur_file_syncs == 0 && dur_dir_syncs == 0);
    want(PUPCount(pups) == 5);

    call(PUPClose, pups);
    TESTBErmrf(tmp);
    done;
}

// --- (2) PUPOpenAll fails loudly -------------------------------------

//  A crashed non-durable create: the rename survived, the bytes did not.
static ok64 PUPDurEmptyRun(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    a_cstr(ext, ".dur.idx");
    call(dur_put, tmp, ext, 1000, 32);
    call(dur_put, tmp, ext, 1001, 0);
    call(dur_put, tmp, ext, 1002, 32);

    Bkv64 pups = {};
    call(kv64bAllocate, pups, 8);
    ok64 o = PUPOpenAll(pups, $path(dir), ext);
    want(o == PUPEMPTY);
    call(PUPClose, pups);
    //  the run is left on disk for the owner to judge, never unlinked
    {
        a_pad(u8, name, PUP_SEQNO_W + 32);
        call(RONu8sFeedPad, u8bIdle(name), (ok64)1001, PUP_SEQNO_W);
        call(u8bFeed, name, ext);
        a_path(fp, $path(dir), u8bDataC(name));
        want(FILEExists($path(fp)) == OK);
    }
    TESTBErmrf(tmp);
    done;
}

//  60 runs, 40 fds: the review's scanskip probe returned OK with 37 runs.
static ok64 PUPDurNoFile(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_cstr(ext, ".dur.idx");
    enum { N = 60, LIM = 40 };
    for (int i = 0; i < N; i++) call(dur_put, tmp, ext, 1000 + i, 32);

    pid_t pid = fork();
    want(pid >= 0);
    if (pid == 0) {
        struct rlimit rl = {.rlim_cur = LIM, .rlim_max = LIM};
        if (setrlimit(RLIMIT_NOFILE, &rl) != 0) _exit(3);
        a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
        Bkv64 pups = {};
        if (kv64bAllocate(pups, N + 4) != OK) _exit(3);
        ok64 o = PUPOpenAll(pups, $path(dir), ext);
        u32 n = PUPCount(pups);
        PUPClose(pups);
        if (o == OK) {
            fprintf(stderr, "PUPDurNoFile: OK with %u of %d runs\n", n, N);
            _exit(1);
        }
        _exit(o == PUPNOMAP ? 0 : 2);
    }
    int st = 0;
    want(waitpid(pid, &st, 0) == pid);
    TESTBErmrf(tmp);
    want(WIFEXITED(st) && WEXITSTATUS(st) == 0);
    done;
}

//  More runs than one scan holds: the rest used to be silently not scanned.
static ok64 PUPDurTooMany(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    a_cstr(ext, ".dur.idx");
    enum { N = FILE_MAX_OPEN + 1 };
    for (int i = 0; i < N; i++) call(dur_put, tmp, ext, 1000 + i, 16);
    Bkv64 pups = {};
    call(kv64bAllocate, pups, N + 4);
    ok64 o = PUPOpenAll(pups, $path(dir), ext);
    want(o == PUPNOROOM);
    call(PUPClose, pups);
    TESTBErmrf(tmp);
    done;
}

ok64 PUPDURtest() {
    sane(1);
    call(PUPDurCreate);
    call(PUPDurEmptyRun);
    call(PUPDurNoFile);
    call(PUPDurTooMany);
    done;
}

TEST(PUPDURtest);
