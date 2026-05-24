#ifndef ABC_KV_H
#define ABC_KV_H
#include "01.h"

typedef struct kv32 {
    u32 key;
    u32 val;
} kv32;

fun u64 kv32hash(kv32 const *v) { return mix32(v->key); }

fun b8 kv32Z(kv32 const *a, kv32 const *b) { return a->key < b->key; }
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
fun b8 kv64hashEq(kv64 const *a, kv64 const *b) { return a->key == b->key; }

#define X(M, name) M##kv64##name
#include "Bx.h"
#include "QSORTx.h"
#undef X

#endif
