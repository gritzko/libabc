#ifndef LIBRDX_SHA_H
#define LIBRDX_SHA_H
#include <sodium.h>

#include "INT.h"

typedef struct {
    u8 data[crypto_hash_sha256_BYTES];
} sha256;

fun b8 sha256empty(sha256 const* sha) {
    // ABC-016: data is 1-aligned (mmapped records); u64 loads were UB
    static sha256 const zero = {};   // C++ includers need the explicit init
    return memcmp(sha->data, zero.data, sizeof(zero.data)) == 0;
}

// STD-26B0685n10: libsodium wants sodium_init() first (seeds randombytes,
// picks SIMD code); it is idempotent and thread-safe, the flag skips its lock
fun b8 SHAsodium() {
    static int inited = 0;
    if (__atomic_load_n(&inited, __ATOMIC_ACQUIRE)) return YES;
    if (sodium_init() < 0) return NO;
    __atomic_store_n(&inited, 1, __ATOMIC_RELEASE);
    return YES;
}

typedef crypto_hash_sha256_state SHAstate;

// SHA-256 needs no init to be correct: a failed sodium_init is ignored
fun void SHASum(sha256* hash, $cu8c from) {
    SHAsodium();
    crypto_hash_sha256(hash->data, *from, $len(from));
}

fun void SHAOpen(SHAstate* state) {
    SHAsodium();
    crypto_hash_sha256_init(state);
}

fun void SHAFeed(SHAstate* state, $cu8c data) {
    crypto_hash_sha256_update(state, *data, $len(data));
}

fun void SHAClose(SHAstate* state, sha256* hash) {
    crypto_hash_sha256_final(state, hash->data);
}

fun b8 sha256Z(sha256 const* a, sha256 const* b) {
    return memcmp(a, b, sizeof(sha256)) < 0;
}
con sha256 sha256Max = {{[0 ... crypto_hash_sha256_BYTES - 1] = 0xff}};

fun int sha256eq(sha256 const* a, sha256 const* b) {
    return memcmp(a, b, sizeof(sha256)) == 0;
}

#define X(M, n) M##sha256##n
#include "Bx.h"
#include "HEXx.h"
#undef X

#endif
