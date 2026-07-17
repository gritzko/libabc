#ifndef ABC_UDP_H
#define ABC_UDP_H
#include <errno.h>

#include "NET.h"

con ok64 UDPFAIL = 0x1e3593ca495;

#define a$c(name, s, len)          \
    char name[len] = {0};          \
    if (!$empty(s)) {              \
        int l = $len(s);           \
        if (l >= len) l = len - 1; \
        memcpy(name, *s, l);       \
        name[l] = 0;               \
    }

ok64 UDPBind(int *fd, u8cs addr);

ok64 UDPConnect(int *fd, u8cs addr);

fun ok64 UDPDrain($u8 into, NETaddr addr, int fd) {
    socklen_t len;
    ssize_t nread;
    do {  // ABC-012: EINTR retries in place, EAGAIN is a soft NETAGAIN
        len = (socklen_t)Blen(addr);
        nread = recvfrom(fd, *into, $len(into), MSG_TRUNC,
                         (struct sockaddr *)*addr, &len);
    } while (nread == -1 && errno == EINTR);
    if (nread == -1)
        return errno == EAGAIN || errno == EWOULDBLOCK ? NETAGAIN : UDPFAIL;
    if (len > (socklen_t)Blen(addr)) len = (socklen_t)Blen(addr);
    range64 range = {0, len};
    Bu8rewind(addr, range);
    // ABC-012: MSG_TRUNC reports the real datagram size; refuse silent cuts
    if ((size_t)nread > $len(into)) return NETNOSPACE;
    *into += nread;
    return OK;
}

fun ok64 UDPFeed(int fd, NETaddr addr, u8cs data) {
    u8$ raw = NETraw(addr);
    ssize_t sz =
        sendto(fd, *data, $len(data), 0, (struct sockaddr *)*raw, $len(raw));
    if (sz == -1) return UDPFAIL;
    data[0] += sz;
    return OK;
}

fun ok64 UDPClose(int fd) {
    int r = close(fd);
    return r == 0 ? OK : UDPFAIL;
}
#endif
