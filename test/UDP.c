#include "UDP.h"

#include <fcntl.h>
#include <unistd.h>

#include "FILE.h"
#include "NET.h"
#include "PRO.h"

// Lowest free fd watermark: dup() returns the lowest unused descriptor, so it
// equals the count of currently-open fds. We close the probe immediately so it
// does not perturb the next measurement. A leak makes the watermark climb.
static int fdwatermark(void) {
    int probe = dup(0);
    if (probe == -1) return -1;
    close(probe);
    return probe;
}

ok64 UDPtest1() {
    sane(1);

    // ABC-017: random loopback port, no hardcoded 3456
    a_pad(u8, addr, 64);
    call(u8sPrintf, addr_idle, "udp://127.0.0.1:%d", NETRandomPort());

    int fd;
    call(UDPBind, &fd, addr_datac);

    int cfd;
    call(UDPConnect, &cfd, addr_datac);

    a$str(bubu, "BuBu");
    call(FILEFeedAll, cfd, bubu);

    aBpad2(u8, read, 128);
    aNETraw(sndaddr);
    call(UDPDrain, readidle, sndaddr, fd);
    $testeq(bubu, readdata);

    // ABC-017: cfd used to leak here
    call(UDPClose, cfd);
    call(UDPClose, fd);
    done;
}

// MEM-009: UDPBind must close the socket fd on a bind() failure. We hold a
// bound socket on a port, then repeatedly try to bind the same (in-use) port;
// each call must fail with UDPFAIL and must NOT leak a descriptor.
ok64 UDPtestBindLeak() {
    sane(1);
    // ABC-017: random loopback port (+9: clear of ABC-012's +5..+7)
    a_pad(u8, addr, 64);
    call(u8sPrintf, addr_idle, "udp://127.0.0.1:%d", NETRandomPort() + 9);

    int held = -1;
    call(UDPBind, &held, addr_datac);  // occupy the port

    int base = fdwatermark();
    test(base != -1, UDPFAIL);

    for (int i = 0; i < 64; ++i) {
        int fd = -1;
        ok64 rc = UDPBind(&fd, addr_datac);  // EADDRINUSE -> bind() fails
        test(rc == UDPFAIL, UDPFAIL);  // must report failure
        int now = fdwatermark();
        test(now != -1, UDPFAIL);
        // no descriptor must accumulate across iterations
        test(now <= base, UDPFAIL);
    }

    call(UDPClose, held);
    done;
}

// ABC-012: a datagram larger than the read slice used to be truncated
// silently with OK; MSG_TRUNC now surfaces it as NETNOSPACE.
ok64 UDPtestTrunc() {
    sane(1);
    char a[64];
    snprintf(a, sizeof(a), "udp://127.0.0.1:%i", NETRandomPort() + 5);
    a$str(baddr, a);
    a$str(caddr, a);

    int fd = -1;
    call(UDPBind, &fd, baddr);
    int cfd = -1;
    call(UDPConnect, &cfd, caddr);

    a$str(msg, "12345678");
    call(FILEFeedAll, cfd, msg);

    aBpad2(u8, read, 4);
    aNETraw(src);
    ok64 rc = UDPDrain(readidle, src, fd);
    if (rc != NETNOSPACE) fail(UDPFAIL);

    call(UDPClose, fd);
    call(UDPClose, cfd);
    done;
}

// ABC-012: EAGAIN on a nonblocking empty socket is a soft NETAGAIN
ok64 UDPtestAgain() {
    sane(1);
    char a[64];
    snprintf(a, sizeof(a), "udp://127.0.0.1:%i", NETRandomPort() + 6);
    a$str(baddr, a);

    int fd = -1;
    call(UDPBind, &fd, baddr);
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

    aBpad2(u8, read, 64);
    aNETraw(src);
    ok64 rc = UDPDrain(readidle, src, fd);
    if (rc != NETAGAIN) fail(UDPFAIL);

    call(UDPClose, fd);
    done;
}

// ABC-012: the bind walk must survive the first addrinfo failing and try
// the next one (dual-stack: v6 occupied/unavailable, v4 next).
ok64 UDPtestBindWalk() {
    sane(1);
    int port = NETRandomPort() + 7;
    char sport[16];
    snprintf(sport, sizeof(sport), "%i", port);

    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo("localhost", sport, &hints, &res) != 0) done;
    if (res == NULL || res->ai_next == NULL) {  // single-family host: skip
        if (res) freeaddrinfo(res);
        done;
    }

    // occupy the FIRST address the resolver returns
    int block = socket(res->ai_family, res->ai_socktype, 0);
    if (block == -1) fail(UDPFAIL);
    if (bind(block, res->ai_addr, res->ai_addrlen) != 0) fail(UDPFAIL);
    freeaddrinfo(res);

    char a[64];
    snprintf(a, sizeof(a), "udp://localhost:%i", port);
    a$str(addr, a);
    int fd = -1;
    ok64 rc = UDPBind(&fd, addr);
    close(block);
    if (rc != OK) fail(UDPFAIL);
    call(UDPClose, fd);
    done;
}

ok64 UDPtest() {
    sane(1);
    call(UDPtest1);
    call(UDPtestBindLeak);
    call(UDPtestTrunc);
    call(UDPtestAgain);
    call(UDPtestBindWalk);
    done;
}

MAIN(UDPtest);
