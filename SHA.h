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

typedef crypto_hash_sha256_state SHAstate;

fun void SHASum(sha256* hash, $cu8c from) {
    crypto_hash_sha256(hash->data, *from, $len(from));
}

fun void SHAOpen(SHAstate* state) { crypto_hash_sha256_init(state); }

fun void SHAFeed(SHAstate* state, $cu8c data) {
    crypto_hash_sha256_update(state, *data, $len(data));
}

fun void SHAClose(SHAstate* state, sha256* hash) {
    crypto_hash_sha256_final(state, hash->data);
}

fun b8 sha256Z(sha256 const* a, sha256 const* b) {
    return memcmp(a, b, sizeof(sha256)) < 0;
}

fun int sha256eq(sha256 const* a, sha256 const* b) {
    return memcmp(a, b, sizeof(sha256)) == 0;
}

#define X(M, n) M##sha256##n
#include "Bx.h"
#include "HEXx.h"
#undef X

#endif
