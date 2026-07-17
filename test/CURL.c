#include <sys/wait.h>
#include <unistd.h>

#include "CURL.h"
#include "FILE.h"
#include "NET.h"
#include "POL.h"
#include "PRO.h"
#include "TCP.h"
#include "TEST.h"

static int test_complete = 0;
static long test_status = 0;
static size_t test_body_len = 0;

static void on_response(CURLreq *req, long status, u8cs body) {
    test_status = status;
    test_body_len = u8csLen(body);
    test_complete = 1;
    trace("CURL response: status=%ld len=%zu", status, test_body_len);
}

// ABC-017: one-shot loopback HTTP server child; replaces the live
// google.com dependency so the test is hermetic and deterministic
static void serve_one(int lfd) {
    int cfd = -1;
    aNETraw(caddr);
    if (TCPAccept(&cfd, caddr, lfd) != OK) _exit(1);
    a_pad(u8, req, 4096);
    (void)FILEdrain(req_idle, cfd);
    a_cstr(resp,
           "HTTP/1.1 200 OK\r\n"
           "Content-Length: 2\r\n"
           "Connection: close\r\n"
           "\r\n"
           "OK");
    if (FILEFeedAll(cfd, resp) != OK) _exit(1);
    close(cfd);
    close(lfd);
    _exit(0);
}

ok64 CURLtest() {
    sane(1);

    int port = NETRandomPort();
    a_pad(u8, addr, 64);
    call(u8sPrintf, addr_idle, "tcp://127.0.0.1:%d", port);

    int lfd;
    call(TCPListen, &lfd, addr_datac);

    pid_t srv = fork();
    test(srv >= 0, CURLFAIL);
    if (srv == 0) serve_one(lfd);
    close(lfd);

    call(POLInit, 64);
    call(CURLInit);

    a_pad(u8, url, 64);
    call(u8sPrintf, url_idle, "http://127.0.0.1:%d/", port);
    call(CURLGet, (const char *)url[0], on_response, NULL);

    // Run event loop until complete (max 10 seconds)
    u64 deadline = POLNow() + 10 * POLNanosPerSec;
    while (!test_complete && POLNow() < deadline) {
        POLLoop(100 * POLNanosPerMSec);
    }

    want(test_complete);
    want(test_status == 200);
    want(test_body_len > 0);

    CURLFree();
    POLFree();

    int st = 0;
    waitpid(srv, &st, 0);
    want(WIFEXITED(st) && WEXITSTATUS(st) == 0);

    done;
}

TEST(CURLtest);
