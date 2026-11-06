// CODE-26B065M0M4: a THREAD() entry maps its own ABC_BASS, so library
// calls that carve BASS scratch (URIutf8FeedSafe) work off the main
// thread; a raw pthread has no BASS and fails.
#include <pthread.h>

#include "PRO.h"
#include "TEST.h"
#include "URI.h"

#define NTHREADS 4

static ok64 URIround(void *arg) {
    sane(1);
    u8 buf[256];
    u8s into = {buf, buf + sizeof(buf)};
    uri u = {};
    a_cstr(s, "http://user@host:8080/a/b?q=1#f");
    a_dup(u8c, in, s);
    call(URIutf8Drain, in, &u);
    for (int i = 0; i < 64; ++i) {
        into[0] = buf;
        call(URIutf8FeedSafe, into, &u);
    }
    done;
}

THREAD(URIthread) {
    sane(ABC_BASS[0] != NULL && ABC_BASS[0] != (u8 *)arg);
    call(URIround, arg);
    want(_pro_depth == 0);
    done;
}

static void *URIraw(void *arg) { return (void *)(uintptr_t)URIround(arg); }

ok64 THREADtest() {
    sane(1);
    pthread_t t[NTHREADS];
    void *r = NULL;
    int bad = 0;
    for (int i = 0; i < NTHREADS; ++i)
        want(0 == pthread_create(t + i, NULL, URIthread, ABC_BASS[0]));
    for (int i = 0; i < NTHREADS; ++i)
        bad += pthread_join(t[i], &r) != 0 || r != NULL;
    same(bad, 0);
    want(0 == pthread_create(t, NULL, URIraw, NULL));
    want(0 == pthread_join(t[0], &r));
    same((ok64)(uintptr_t)r, BNOROOM);
    call(URIround, NULL);
    done;
}

TEST(THREADtest);
