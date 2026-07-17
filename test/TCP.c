#include "TCP.h"

#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "FILE.h"
#include "INT.h"
#include "NET.h"
#include "PRO.h"
#include "TEST.h"

// Lowest free fd watermark: dup() returns the lowest unused descriptor, so it
// equals the count of currently-open fds. We close the probe immediately so it
// does not perturb the next measurement. A leak makes the watermark climb.
static int fdwatermark(void) {
    int probe = dup(0);
    if (probe == -1) return -1;
    close(probe);
    return probe;
}

void garble($u8 data) {
    srandom(time(NULL));
    for (int i = 0; i < $len(data); ++i) {
        int b = random() % $len(data);
        u8Swap($atp(data, i), $atp(data, b));
    }
}

ok64 TCPtest1() {
    sane(1);

    // ABC-017: random loopback port, no hardcoded 12345
    a_pad(u8, addr, 64);
    call(u8sPrintf, addr_idle, "tcp://127.0.0.1:%d", NETRandomPort());

    int fd;
    call(TCPListen, &fd, addr_datac);

    int cfd;
    call(TCPConnect, &cfd, addr_datac, 0);

    int sfd;
    aNETraw(caddr);
    call(TCPAccept, &sfd, caddr, fd);

    a$str(bubu, "BuBu");
    call(FILEFeedAll, cfd, bubu);

    aBpad2(u8, read, 128);
    call(FILEdrain, readidle, sfd);
    $testeq(bubu, readdata);

    call(TCPClose, fd);
    call(TCPClose, cfd);
    call(TCPClose, sfd);
    done;
}

// MEM-009: TCPListen must close the socket fd on a bind() failure. We hold a
// listener on a port, then repeatedly try to bind the same (in-use) port; each
// call must fail with TCPFAIL and must NOT leak a descriptor.
ok64 TCPtestBindLeak() {
    sane(1);
    // ABC-017: random loopback port (+9: clear of ABC-012's +1..+7)
    a_pad(u8, addr, 64);
    call(u8sPrintf, addr_idle, "tcp://127.0.0.1:%d", NETRandomPort() + 9);

    int held = -1;
    call(TCPListen, &held, addr_datac);  // occupy the port

    int base = fdwatermark();
    test(base != -1, TCPFAIL);

    for (int i = 0; i < 64; ++i) {
        int fd = -1;
        ok64 rc = TCPListen(&fd, addr_datac);  // EADDRINUSE -> bind() fails
        test(rc == TCPFAIL, TCPFAIL);    // must report failure
        int now = fdwatermark();
        test(now != -1, TCPFAIL);
        // no descriptor must accumulate across iterations
        test(now <= base, TCPFAIL);
    }

    call(TCPClose, held);
    done;
}

// ABC-012: EAGAIN on a nonblocking listener is a soft NETAGAIN, not TCPFAIL
ok64 TCPtestAcceptAgain() {
    sane(1);
    char a[64];
    snprintf(a, sizeof(a), "tcp://127.0.0.1:%i", NETRandomPort() + 1);
    a$str(addr, a);

    int fd = -1;
    call(TCPListen, &fd, addr);
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

    int cfd = -1;
    aNETraw(caddr);
    ok64 rc = TCPAccept(&cfd, caddr, fd);
    want(rc == NETAGAIN);

    call(TCPClose, fd);
    done;
}

// ABC-012: the nonblocking param must be honored and TCPAccept must rewind
// the peer address buffer (it used to stay empty).
ok64 TCPtestNonblock() {
    sane(1);
    char a[64];
    snprintf(a, sizeof(a), "tcp://127.0.0.1:%i", NETRandomPort() + 2);
    a$str(laddr, a);
    a$str(caddr, a);

    int fd = -1;
    call(TCPListen, &fd, laddr);

    int cfd = -1;
    call(TCPConnect, &cfd, caddr, YES);
    want(fcntl(cfd, F_GETFL, 0) & O_NONBLOCK);

    // wait for the nonblocking connect to complete, then verify it
    struct pollfd pfd = {.fd = cfd, .events = POLLOUT};
    want(poll(&pfd, 1, 2000) == 1);
    int err = -1;
    socklen_t el = sizeof(err);
    want(getsockopt(cfd, SOL_SOCKET, SO_ERROR, &err, &el) == 0 && err == 0);

    int sfd = -1;
    aNETraw(peer);
    call(TCPAccept, &sfd, peer, fd);
    want(!$empty(NETraw(peer)));

    call(TCPClose, fd);
    call(TCPClose, cfd);
    call(TCPClose, sfd);
    done;
}

// ABC-012: the bind walk must survive the first addrinfo failing and try
// the next one (dual-stack: v6 occupied/unavailable, v4 next).
ok64 TCPtestBindWalk() {
    sane(1);
    int port = NETRandomPort() + 3;
    char sport[16];
    snprintf(sport, sizeof(sport), "%i", port);

    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo("localhost", sport, &hints, &res) != 0) done;
    if (res == NULL || res->ai_next == NULL) {  // single-family host: skip
        if (res) freeaddrinfo(res);
        done;
    }

    // occupy the FIRST address the resolver returns; listening blocks any
    // rebind of it, so TCPListen must fall through to the next entry
    int block = socket(res->ai_family, res->ai_socktype, 0);
    want(block != -1);
    want(bind(block, res->ai_addr, res->ai_addrlen) == 0);
    want(listen(block, 1) == 0);
    freeaddrinfo(res);

    char a[64];
    snprintf(a, sizeof(a), "tcp://localhost:%i", port);
    a$str(addr, a);
    int fd = -1;
    ok64 rc = TCPListen(&fd, addr);
    close(block);
    want(rc == OK);
    call(TCPClose, fd);
    done;
}

// ABC-012: SO_REUSEADDR: an instant listener restart on the same port must
// not fail EADDRINUSE while the old connection sits in TIME_WAIT.
ok64 TCPtestReuseAddr() {
    sane(1);
    char a[64];
    snprintf(a, sizeof(a), "tcp://127.0.0.1:%i", NETRandomPort() + 4);
    a$str(laddr, a);
    a$str(caddr, a);
    a$str(raddr, a);

    int fd = -1;
    call(TCPListen, &fd, laddr);
    int cfd = -1;
    call(TCPConnect, &cfd, caddr, NO);
    int sfd = -1;
    aNETraw(peer);
    call(TCPAccept, &sfd, peer, fd);

    call(TCPClose, sfd);  // server closes first -> TIME_WAIT holds the port
    call(TCPClose, cfd);
    call(TCPClose, fd);
    usleep(100 * 1000);  // let the FIN exchange land in TIME_WAIT

    int fd2 = -1;
    call(TCPListen, &fd2, raddr);
    call(TCPClose, fd2);
    done;
}

ok64 TCPtest() {
    sane(1);
    call(TCPtest1);
    call(TCPtestBindLeak);
    call(TCPtestAcceptAgain);
    call(TCPtestNonblock);
    call(TCPtestBindWalk);
    call(TCPtestReuseAddr);
    done;
}

MAIN(TCPtest);
