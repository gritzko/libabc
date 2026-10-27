#ifndef ABC_KV_H
#define ABC_KV_H
#include "01.h"
#include "INT.h"

typedef struct kv32 {
    u32 key;
    u32 val;
} kv32;

fun u64 kv32hash(kv32 const *v) { return mix32(v->key); }

fun b8 kv32Z(kv32 const *a, kv32 const *b) { return a->key < b->key; }
//  BOX-26A26EbjLj: the max record (BOXx tombstone, SSTx sentinel)
con kv32 kv32max = {u32max, u32max};
fun b8 kv32hashEq(kv32 const *a, kv32 const *b) { return a->key == b->key; }

#define X(M, name) M##kv32##name
#include "Bx.h"
#undef X

typedef struct kv64 {
    u64 key;
    u64 val;
} kv64;

fun u64 kv64hash(kv64 const *v) { return mix64(v->key); }

fun b8 kv64Z(kv64 const *a, kv64 const *b) { return a->key < b->key; }
con kv64 kv64max = {u64max, u64max};
fun b8 kv64hashEq(kv64 const *a, kv64 const *b) { return a->key == b->key; }

#define X(M, name) M##kv64##name
#include "Bx.h"
#include "QSORTx.h"
#undef X

typedef struct kv128 {
    u128 key;
    u128 val;
} kv128;

//  KV-26A273cxFy: key only; the all-zero record is empty, kv128max the tombstone
fun u64 kv128hash(kv128 const *v) { return u128hash(&v->key); }

fun b8 kv128Z(kv128 const *a, kv128 const *b) { return u128Z(&a->key, &b->key); }
con kv128 kv128max = {{{u64max, u64max}}, {{u64max, u64max}}};
fun b8 kv128hashEq(kv128 const *a, kv128 const *b) {
    return u128hashEq(&a->key, &b->key);
}

#define X(M, name) M##kv128##name
#include "Bx.h"
#include "QSORTx.h"
#undef X

//  KV-26A274c3W6: a 16-byte map record, a 96-bit key (k0 low, k1 high)
//  and a 32-bit value.  hash/hashEq/Z read the key only, never val
//  (u128hash would read all 128 bits).  All-zero is empty, all-ones
//  k96v32max the tombstone.
typedef struct k96v32 {
    u64 k0;
    u32 k1;
    u32 val;
} k96v32;

fun u64 k96v32hash(k96v32 const *v) { return u64hash2(v->k0, v->k1); }

fun b8 k96v32Z(k96v32 const *a, k96v32 const *b) {
    if (a->k1 != b->k1) return a->k1 < b->k1;
    return a->k0 < b->k0;
}
con k96v32 k96v32max = {u64max, u32max, u32max};
fun b8 k96v32hashEq(k96v32 const *a, k96v32 const *b) {
    return a->k0 == b->k0 && a->k1 == b->k1;
}

#define X(M, name) M##k96v32##name
#include "Bx.h"
#include "QSORTx.h"
#undef X

#endif
