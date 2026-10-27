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

#endif
