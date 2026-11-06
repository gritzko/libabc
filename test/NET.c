#include "NET.h"

#include "PRO.h"
#include "TCP.h"
#include "TEST.h"

// STD-26B068Nujy: an IPv6 literal resolves without its brackets
ok64 NETtestBrackets() {
    sane(1);
    URIstate uri = {};
    a_cstr(s, "tcp://[::1]:8080");
    call(URIutf8Drain, s, &uri);
    struct addrinfo *r = NULL;
    call(NETResolve, &r, &uri, YES);
    want(r != NULL && r->ai_family == AF_INET6);
    call(NETFreeAddress, &r);
    done;
}

// STD-26B068Nujy: an empty host is the AI_PASSIVE wildcard, listenable
ok64 NETtestWildcard() {
    sane(1);
    a_pad(u8, addr, 64);
    call(u8sPrintf, addr_idle, "tcp://:%d", NETRandomPort() + 21);
    int fd = -1;
    call(TCPListen, &fd, addr_datac);
    call(TCPClose, fd);
    a_pad(u8, a6, 64);
    call(u8sPrintf, a6_idle, "tcp://[::1]:%d", NETRandomPort() + 22);
    call(TCPListen, &fd, a6_datac);
    call(TCPClose, fd);
    done;
}

ok64 NETtest() {
    sane(1);
    call(NETtestBrackets);
    call(NETtestWildcard);
    done;
}

MAIN(NETtest);
