#ifndef ABC_TCP_H
#define ABC_TCP_H
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "NET.h"

con ok64 TCPFAIL = 0x1d3193ca495;

ok64 TCPListen(int *fd, u8cs addr);
ok64 TCPConnect(int *fd, u8csc addr, b8 nonblocking);

// NET-26A24GSaPD: request/response over TCP stalls ~40 ms per round trip on
// Nagle vs delayed ACK; disable Nagle on IP stream sockets (never AF_UNIX).
fun void TCPNoDelay(int fd, int family) {
    if (family != AF_INET && family != AF_INET6) return;
    int yes = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
}

fun ok64 TCPAccept(int *cfd, NETaddr addr, int sfd) {
    socklen_t len;
    int rc;
    do {  // ABC-012: EINTR retries in place, EAGAIN is a soft NETAGAIN
        len = (socklen_t)Blen(addr);
        rc = accept(sfd, (struct sockaddr *)*addr, &len);
    } while (rc == -1 && errno == EINTR);
    if (rc == -1)
        return errno == EAGAIN || errno == EWOULDBLOCK ? NETAGAIN : TCPFAIL;
    // ABC-012: expose the peer sockaddr via NETraw, cf. UDPDrain
    if (len > (socklen_t)Blen(addr)) len = (socklen_t)Blen(addr);
    if (len >= (socklen_t)sizeof(sa_family_t))
        TCPNoDelay(rc, ((struct sockaddr *)*addr)->sa_family);
    range64 range = {0, len};
    Bu8rewind(addr, range);
    *cfd = rc;
    return OK;
}

fun ok64 TCPClose(int fd) {
    int r = close(fd);
    return r == 0 ? OK : TCPFAIL;
}
#endif
