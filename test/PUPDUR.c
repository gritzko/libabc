//  CODE-26A23C8gS9, CODE-26A23CSStk: crash durability of a run.
//  (1) a durable create fsyncs the run before its rename and the directory
//      after it; the plain create keeps the fast path (no fsync at all);
//  (2) Open never returns OK with a run missing: a 0-byte run (what a
//      crashed non-durable create leaves) fails; PUPS-26A276Xh2e: a run holds
//      no fd, so an fd budget smaller than the stack reads it whole.
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

#define X(M, name) M##kv64##name
#include "PUPx.h"
#undef X

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
    kv64 row = {1, 10};
    kv64cs b = {&row, &row + 1};
    a_pup(kv64, h);
    u64 const key[5] = {1000, 0, 1001, 0, 0};
    b8 const dur[5] = {NO, NO, YES, YES, NO};
    for (int i = 0; i < 5; i++) {
        dur_reset();
        call(PUPkv64Create, h, h_keys, $path(dir), ext, b, key[i], dur[i],
             NULL);
        want(dur_file_syncs == dur[i] && dur_dir_syncs == dur[i]);
    }
    want(Bpastlen(h) == 5);
    PUPkv64Close(h, h_keys);
    TESTBErmrf(tmp);
    done;
}

// --- (2) Open fails loudly -------------------------------------

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

    a_pup(kv64, h);
    ok64 o = PUPkv64Open(h, h_keys, $path(dir), ext, u64max);
    want(o == PUPEMPTY);
    PUPkv64Close(h, h_keys);
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
//  PUPS-26A276Xh2e: runs hold no fd now, so all 60 open under 40 fds.
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
        a_pup(kv64, h);
        ok64 o = PUPkv64Open(h, h_keys, $path(dir), ext, u64max);
        u32 n = (u32)Bpastlen(h);
        PUPkv64Close(h, h_keys);
        if (o != OK || n != (u32)N) {
            fprintf(stderr, "PUPDurNoFile: %u of %d runs\n", n, N);
            _exit(1);
        }
        _exit(0);
    }
    int st = 0;
    want(waitpid(pid, &st, 0) == pid);
    TESTBErmrf(tmp);
    want(WIFEXITED(st) && WEXITSTATUS(st) == 0);
    done;
}

ok64 PUPDURtest() {
    sane(1);
    call(PUPDurCreate);
    call(PUPDurEmptyRun);
    call(PUPDurNoFile);
    done;
}

TEST(PUPDURtest);
