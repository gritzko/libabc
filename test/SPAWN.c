#include "FILE.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "PRO.h"
#include "TEST.h"

// `true` exits 0, `false` exits 1, `sh` runs scripts, `cat` round-
// trips stdin.  Bare names rely on PATH; macOS GHA runners ship
// these under /usr/bin (no /bin/true), Linux usually has both.

// Build a u8css argv from a list of u8slit() initializers.
#define ARGV(name, ...)                                                    \
    u8cs _##name##_a[] = { __VA_ARGS__ };                                  \
    u8css name = {_##name##_a,                                             \
                  _##name##_a + sizeof(_##name##_a) / sizeof(u8cs)}

ok64 SPAWNTestExit0() {
    sane(1);
    a_cstr(path, "true");
    ARGV(args, u8slit("true"));
    pid_t pid = 0;
    call(FILESpawn, path, args, NULL, NULL, &pid);
    int rc = -1;
    call(FILEReap, pid, &rc);
    testeqv(0, rc, "%d");
    done;
}

ok64 SPAWNTestExitNonZero() {
    sane(1);
    a_cstr(path, "false");
    ARGV(args, u8slit("false"));
    pid_t pid = 0;
    call(FILESpawn, path, args, NULL, NULL, &pid);
    int rc = -1;
    call(FILEReap, pid, &rc);
    testeqv(1, rc, "%d");
    done;
}

ok64 SPAWNTestExitCustom() {
    sane(1);
    a_cstr(path, "sh");
    ARGV(args, u8slit("sh"), u8slit("-c"), u8slit("exit 42"));
    pid_t pid = 0;
    call(FILESpawn, path, args, NULL, NULL, &pid);
    int rc = -1;
    call(FILEReap, pid, &rc);
    testeqv(42, rc, "%d");
    done;
}

ok64 SPAWNTestExecMissing() {
    sane(1);
    a_cstr(path, "/no/such/binary/here");
    ARGV(args, u8slit("nope"));
    pid_t pid = 0;
    call(FILESpawn, path, args, NULL, NULL, &pid);
    int rc = -1;
    call(FILEReap, pid, &rc);
    testeqv(127, rc, "%d");  // _exit(127) on exec failure
    done;
}

ok64 SPAWNTestStdoutPipe() {
    sane(1);
    a_cstr(path, "sh");
    ARGV(args, u8slit("sh"), u8slit("-c"), u8slit("echo hello"));
    pid_t pid = 0;
    int rfd = -1;
    call(FILESpawn, path, args, NULL, &rfd, &pid);

    char buf[64] = {};
    ssize_t n = 0;
    while (1) {
        ssize_t r = read(rfd, buf + n, sizeof(buf) - 1 - (size_t)n);
        if (r <= 0) break;
        n += r;
    }
    close(rfd);

    int rc = -1;
    call(FILEReap, pid, &rc);
    testeqv(0, rc, "%d");
    test(n == 6, FAIL);  // "hello\n"
    test(memcmp(buf, "hello\n", 6) == 0, FAIL);
    done;
}

ok64 SPAWNTestStdinPipe() {
    sane(1);
    a_cstr(path, "cat");
    ARGV(args, u8slit("cat"));
    pid_t pid = 0;
    int wfd = -1, rfd = -1;
    call(FILESpawn, path, args, &wfd, &rfd, &pid);

    const char *msg = "round trip\n";
    ssize_t w = write(wfd, msg, strlen(msg));
    test(w == (ssize_t)strlen(msg), FAIL);
    close(wfd);  // EOF for cat

    char buf[64] = {};
    ssize_t n = 0;
    while (1) {
        ssize_t r = read(rfd, buf + n, sizeof(buf) - 1 - (size_t)n);
        if (r <= 0) break;
        n += r;
    }
    close(rfd);

    int rc = -1;
    call(FILEReap, pid, &rc);
    testeqv(0, rc, "%d");
    test(n == (ssize_t)strlen(msg), FAIL);
    test(memcmp(buf, msg, strlen(msg)) == 0, FAIL);
    done;
}

//  FILESpawnFds: shell-style `producer | pager` pipeline.  One pipe
//  for the producer→pager hop, another for the pager→test capture.
//  FD_CLOEXEC on the inner pipe ends ensures the unused dup'd copy
//  in each child auto-closes at exec, so EOF propagates the moment
//  the producer exits and the pager doesn't hang reading.
ok64 SPAWNTestFdsPipeline() {
    sane(1);

    int p[2]    = {-1, -1};                  // echo → cat
    int outp[2] = {-1, -1};                  // cat → test
    test(pipe(p)    == 0, FAIL);
    test(pipe(outp) == 0, FAIL);
    (void)fcntl(p[0],    F_SETFD, FD_CLOEXEC);
    (void)fcntl(p[1],    F_SETFD, FD_CLOEXEC);
    (void)fcntl(outp[0], F_SETFD, FD_CLOEXEC);
    (void)fcntl(outp[1], F_SETFD, FD_CLOEXEC);

    //  Producer: `echo hello` → p[1].  Inherits stdin from parent.
    a_cstr(echo_path, "echo");
    ARGV(echo_args, u8slit("echo"), u8slit("hello"));
    pid_t echo_pid = 0;
    call(FILESpawnFds, echo_path, echo_args, -1, p[1], &echo_pid);

    //  Pager: `cat` reads from p[0], writes to outp[1].
    a_cstr(cat_path, "cat");
    ARGV(cat_args, u8slit("cat"));
    pid_t cat_pid = 0;
    call(FILESpawnFds, cat_path, cat_args, p[0], outp[1], &cat_pid);

    //  Parent drops every pipe end it doesn't read from; otherwise
    //  pager's read on the inner pipe never sees EOF.
    close(p[0]);    close(p[1]);
    close(outp[1]);

    char buf[64] = {};
    ssize_t n = 0;
    while (1) {
        ssize_t r = read(outp[0], buf + n, sizeof(buf) - 1 - (size_t)n);
        if (r <= 0) break;
        n += r;
    }
    close(outp[0]);

    int echo_rc = -1, cat_rc = -1;
    call(FILEReap, echo_pid, &echo_rc);
    call(FILEReap, cat_pid,  &cat_rc);
    testeqv(0, echo_rc, "%d");
    testeqv(0, cat_rc,  "%d");
    test(n == 6, FAIL);                       // "hello\n"
    test(memcmp(buf, "hello\n", 6) == 0, FAIL);
    done;
}

//  FILESpawnFds with both fds = -1 should inherit parent's stdin/
//  stdout unchanged.  Use `true` (exit 0) — we don't care about its
//  output, only that spawn + reap round-trips cleanly without any
//  pipe plumbing.
ok64 SPAWNTestFdsInherit() {
    sane(1);
    a_cstr(path, "true");
    ARGV(args, u8slit("true"));
    pid_t pid = 0;
    call(FILESpawnFds, path, args, -1, -1, &pid);
    int rc = -1;
    call(FILEReap, pid, &rc);
    testeqv(0, rc, "%d");
    done;
}

ok64 SPAWNTestSignalDeath() {
    sane(1);
    a_cstr(path, "sh");
    ARGV(args, u8slit("sh"), u8slit("-c"), u8slit("kill -TERM $$"));
    pid_t pid = 0;
    call(FILESpawn, path, args, NULL, NULL, &pid);
    int sig = -1;
    ok64 o = FILEReap(pid, &sig);
    test(o == FILESIGNAL, FAIL);
    testeqv(15, sig, "%d");  // SIGTERM
    done;
}

//  Print the sub-test name before running so the last line in
//  stderr names the failing case when a testeqv triggers FAILEQ
//  (the harness only reports `main:<line>` of TEST() itself).
#define RUN(f) do { fprintf(stderr, "  - " #f "\n"); call(f); } while (0)

ok64 SPAWNtest() {
    sane(1);
    RUN(SPAWNTestExit0);
    RUN(SPAWNTestExitNonZero);
    RUN(SPAWNTestExitCustom);
    RUN(SPAWNTestExecMissing);
    RUN(SPAWNTestStdoutPipe);
    RUN(SPAWNTestStdinPipe);
    RUN(SPAWNTestFdsPipeline);
    RUN(SPAWNTestFdsInherit);
    RUN(SPAWNTestSignalDeath);
    done;
}

TEST(SPAWNtest);
