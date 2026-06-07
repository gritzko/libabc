#include "UDP.h"

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

    a$str(addr, "udp://127.0.0.1:3456");

    int fd;
    call(UDPBind, &fd, addr);

    int cfd;
    call(UDPConnect, &cfd, addr);

    a$str(bubu, "BuBu");
    call(FILEFeedAll, cfd, bubu);

    aBpad2(u8, read, 128);
    aNETraw(sndaddr);
    call(UDPDrain, readidle, sndaddr, fd);
    $testeq(bubu, readdata);

    call(UDPClose, fd);
    done;
}

// MEM-009: UDPBind must close the socket fd on a bind() failure. We hold a
// bound socket on a port, then repeatedly try to bind the same (in-use) port;
// each call must fail with UDPFAIL and must NOT leak a descriptor.
ok64 UDPtestBindLeak() {
    sane(1);
    a$str(addr, "udp://127.0.0.1:3499");

    int held = -1;
    call(UDPBind, &held, addr);  // occupy the port

    int base = fdwatermark();
    test(base != -1, UDPFAIL);

    for (int i = 0; i < 64; ++i) {
        int fd = -1;
        ok64 rc = UDPBind(&fd, addr);  // EADDRINUSE -> bind() fails
        test(rc == UDPFAIL, UDPFAIL);  // must report failure
        int now = fdwatermark();
        test(now != -1, UDPFAIL);
        // no descriptor must accumulate across iterations
        test(now <= base, UDPFAIL);
    }

    call(UDPClose, held);
    done;
}

ok64 UDPtest() {
    sane(1);
    call(UDPtest1);
    call(UDPtestBindLeak);
    done;
}

MAIN(UDPtest);
