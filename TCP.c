#include "TCP.h"

#include <fcntl.h>
#include <sys/socket.h>

#include "NET.h"
#include "PRO.h"
#include "S.h"

ok64 TCPConnect(int *fd, u8csc address, b8 nonblocking) {
    sane(fd != NULL && !$empty(address));
    int sfd = -1;
    struct addrinfo *result = NULL, *rp;

    URIstate uri = {};
    a_dup(u8c, addr, address);
    call(URIutf8Drain, addr, &uri);
    call(NETResolve, &result, &uri, YES);

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) continue;
        // ABC-012: honor nonblocking; EINPROGRESS is a connect in progress
        if (nonblocking) fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL, 0) | O_NONBLOCK);

        int rc = connect(sfd, rp->ai_addr, rp->ai_addrlen);
        if (rc == 0 || (nonblocking && errno == EINPROGRESS)) break;
        trace("connect failed: %s\n", strerror(errno));

        close(sfd);
        sfd = -1;
    }

    NETFreeAddress(&result);

    if (rp == NULL || sfd == -1) return TCPFAIL;
    *fd = sfd;
    return OK;
}

ok64 TCPListen(int *fd, u8cs addr) {
    sane(fd != NULL && !$empty(addr));
    int sfd = -1;
    struct addrinfo *result = NULL, *rp;

    URIstate uri = {};
    call(URIutf8Drain, addr, &uri);
    call(NETResolve, &result, &uri, YES);

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, 0);
        if (sfd == -1) continue;
        // ABC-012: allow instant restart while old sockets sit in TIME_WAIT
        int yes = 1;
        setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        // ABC-012: keep walking the list (dual-stack: v6 may fail, v4 next)
        trace("bind failed: %s\n", strerror(errno));
        close(sfd);
        sfd = -1;
    }

    NETFreeAddress(&result);

    test(sfd != -1, TCPFAIL);
    int lrc = listen(sfd, 128);
    if (lrc != 0) {
        close(sfd);
        fail(TCPFAIL);
    }

    *fd = sfd;
    done;
}
