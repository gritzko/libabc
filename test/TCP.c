#include "TCP.h"

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

    a_cstr(addr, "tcp://127.0.0.1:12345");

    int fd;
    call(TCPListen, &fd, addr);

    int cfd;
    call(TCPConnect, &cfd, addr, 0);

    int sfd;
    aNETraw(caddr);
    call(TCPAccept, &sfd, caddr, fd);

    a$str(bubu, "BuBu");
    call(FILEFeedAll, cfd, bubu);

    aBpad2(u8, read, 128);
    aNETraw(sndaddr);
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
    a_cstr(addr, "tcp://127.0.0.1:12399");

    int held = -1;
    call(TCPListen, &held, addr);  // occupy the port

    int base = fdwatermark();
    test(base != -1, TCPFAIL);

    for (int i = 0; i < 64; ++i) {
        int fd = -1;
        ok64 rc = TCPListen(&fd, addr);  // EADDRINUSE -> bind() fails
        test(rc == TCPFAIL, TCPFAIL);    // must report failure
        int now = fdwatermark();
        test(now != -1, TCPFAIL);
        // no descriptor must accumulate across iterations
        test(now <= base, TCPFAIL);
    }

    call(TCPClose, held);
    done;
}

ok64 TCPtest() {
    sane(1);
    call(TCPtest1);
    call(TCPtestBindLeak);
    done;
}

MAIN(TCPtest);
