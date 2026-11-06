#include <NACL.h>
#include <PRO.h>
#include <TEST.h>
#include <pthread.h>

#define NTHREADS 4

ok64 NACLround() {
    sane(1);
    edpub256 publicKey;
    edsec512 secretKey;
    edpub256 publicKey2;
    edsec512 secretKey2;
    call(NACLed25519create, &publicKey, &secretKey);
    call(NACLed25519create, &publicKey2, &secretKey2);
    u8cs value = $u8str("Hello world!\n");
    sha256 hash = {};
    SHASum(&hash, value);
    edsig512 sign;
    call(NACLed25519sign, &sign, &hash, &secretKey);
    call(NACLed25519verify, &sign, &hash, &publicKey);
    ok64 o = NACLed25519verify(&sign, &hash, &publicKey2);
    testeqv((long long)(o), (long long)(NACLBAD), "%lld");
    done;
}

THREAD(NACLthread) {
    sane(1);
    (void)arg;
    for (int i = 0; i < 16; ++i) call(NACLround);
    done;
}

// STD-26B0685n10: the first keypair runs sodium_init (1 = done before);
// then sign/verify from several threads at once
ok64 NACLtest() {
    sane(1);
    edpub256 pub;
    edsec512 sec;
    call(NACLed25519create, &pub, &sec);
    same(sodium_init(), 1);
    pthread_t t[NTHREADS];
    void *r = NULL;
    int bad = 0;
    for (int i = 0; i < NTHREADS; ++i)
        want(0 == pthread_create(t + i, NULL, NACLthread, NULL));
    for (int i = 0; i < NTHREADS; ++i)
        bad += pthread_join(t[i], &r) != 0 || r != NULL;
    same(bad, 0);
    done;
}

TEST(NACLtest);
