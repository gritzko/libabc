//  HASH-26A275ba4V: Move tests, one instantiation per record type.
//  Wants X, HASHx.h already instantiated for it, MLINE (its
//  ABC_HASH_LINE), MCONV (its ABC_HASH_CONVERGE) and X(M, of)(k, val):
//  the record keyed by k (nonzero), its value if val.
#define T X(, )

//  every key of keys is found, value and all
ok64 X(M, all)(X($, ) data, u64cs keys) {
    sane(1);
    $for(u64c, k, keys) {
        T g = X(M, of)(*k, NO), w = X(M, of)(*k, YES);
        call(X(HASH, Get), &g, data);
        want(0 == memcmp(&g, &w, sizeof(T)));
    }
    done;
}

size_t X(M, live)(X($, ) data) {
    size_t n = 0;
    $for(T, p, data) if (!X(, IsZero)(p)) ++n;
    return n;
}

//  CONVERGE: data is byte-equal to a table of its length Put-built
//  from keys in reverse order
ok64 X(M, same)(X($, ) data, u64cs keys) {
    sane(1);
    X(B, ) put = {};
    call(X(, bMap), put, $len(data));
    call(X(, bFed), put, $len(data));
    for (u64 const *k = keys[1]; k > keys[0];) {
        T r = X(M, of)(*--k, YES);
        call(X(HASH, Put), X(, bData)(put), &r);
    }
    want(0 == memcmp(data[0], put[1], $len(data) * sizeof(T)));
    call(X(, bUnMap), put);
    done;
}

//  the source: MLINE*64 slots, keys whose line under the length small
//  holds < MLINE/2 keys (so a Move into small fits), until n keys
ok64 X(M, src)(X(B, ) src, Bu64 keys, size_t small, size_t n) {
    sane(1);
    size_t len = MLINE * 64;
    u32 cnt[64] = {};
    call(X(, bMap), src, len);
    call(X(, bFed), src, len);
    u64 x = 0x9e3779b97f4a7c15ULL;
    while (u64bDataLen(keys) < n) {
        x = x * 6364136223846793005ULL + 1442695040888963407ULL;
        if (!x) continue;
        T r = X(M, of)(x, YES);
        size_t line = (X(, Hash)(&r) & (small - 1)) / MLINE;
        if (cnt[line] >= MLINE / 2) continue;
        size_t ndx = 0;
        if (FULL == X(HASH, _find)(&ndx, X(, bData)(src), &r)) continue;
        call(X(HASH, Put), X(, bData)(src), &r);
        call(u64bFeed1, keys, x);
        ++cnt[line];
    }
    done;
}

//  Move src into a fresh table of length len: OK, every key found, as
//  many entries, src intact; CONVERGE: the Put-built layout
ok64 X(M, into)(X(B, ) src, u64cs keys, size_t len) {
    sane(1);
    X(B, ) dst = {}, cpy = {};
    size_t slen = X(, bDataLen)(src);
    call(X(, bMap), cpy, slen);
    memcpy(cpy[0], src[1], slen * sizeof(T));
    call(X(, bMap), dst, len);
    call(X(, bFed), dst, len);
    call(X(HASH, Move), dst, src);
    want(X(, bDataLen)(dst) == len);
    call(X(M, all), X(, bData)(dst), keys);
    testeqv((long long)X(M, live)(X(, bData)(dst)), (long long)$len(keys),
            "%lld");
    want(0 == memcmp(cpy[0], src[1], slen * sizeof(T)));
    if (MCONV) call(X(M, same), X(, bData)(dst), keys);
    call(X(, bUnMap), dst);
    call(X(, bUnMap), cpy);
    done;
}

//  bigger, same, smaller (fits by construction of the keys)
ok64 X(M, sizes)() {
    sane(1);
    size_t small = MLINE * 16;
    X(B, ) src = {};
    Bu64 keys = {};
    call(u64bMap, keys, small);
    call(X(M, src), src, keys, small, small / 3);
    size_t len = X(, bDataLen)(src);
    call(X(M, into), src, u64bDataC(keys), len * 4);
    call(X(M, into), src, u64bDataC(keys), len);
    call(X(M, into), src, u64bDataC(keys), small);
    call(X(, bUnMap), src);
    call(u64bUnMap, keys);
    done;
}

//  smaller that does not fit: one line, more keys than slots: FULL,
//  the target full of src's records (each found, value and all), src
//  intact; then a Move of the same into the same is all upserts: no room
//  needed for the keys already in, FULL again at the same key
ok64 X(M, noroom)() {
    sane(1);
    X(B, ) src = {}, dst = {}, cpy = {};
    Bu64 keys = {};
    call(u64bMap, keys, MLINE * 4);
    call(X(M, src), src, keys, MLINE * 64, MLINE * 4);
    size_t slen = X(, bDataLen)(src);
    call(X(, bMap), cpy, slen);
    memcpy(cpy[0], src[1], slen * sizeof(T));
    call(X(, bMap), dst, MLINE);
    call(X(, bFed), dst, MLINE);
    want(FULL == X(HASH, Move)(dst, src));
    want(X(M, live)(X(, bData)(dst)) == MLINE);
    size_t found = 0;
    $for(u64c, k, u64bDataC(keys)) {
        T g = X(M, of)(*k, NO), w = X(M, of)(*k, YES);
        if (OK != X(HASH, Get)(&g, X(, bData)(dst))) continue;
        want(0 == memcmp(&g, &w, sizeof(T)));
        ++found;
    }
    want(found == MLINE);
    want(0 == memcmp(cpy[0], src[1], slen * sizeof(T)));
    memcpy(cpy[0], dst[1], MLINE * sizeof(T));
    want(FULL == X(HASH, Move)(dst, src));
    want(0 == memcmp(cpy[0], dst[1], MLINE * sizeof(T)));
    call(X(, bUnMap), cpy);
    call(X(, bUnMap), dst);
    call(X(, bUnMap), src);
    call(u64bUnMap, keys);
    done;
}

//  sanity: a non-power-of-2 or overlapping target is BADARG
ok64 X(M, insane)() {
    sane(1);
    X(B, ) buf = {};
    call(X(, bMap), buf, MLINE * 4);
    call(X(, bFed), buf, MLINE * 3);
    X(B, ) a = {buf[0], buf[0], buf[0] + MLINE * 3, buf[3]};
    X(B, ) b = {buf[0], buf[0], buf[0] + MLINE * 2, buf[3]};
    want(BADARG == X(HASH, Move)(a, b));  // 3 lines: not pow2
    want(BADARG == X(HASH, Move)(b, a));  // overlap
    call(X(, bUnMap), buf);
    done;
}

ok64 X(M, test)() {
    sane(1);
    call(X(M, sizes));
    call(X(M, noroom));
    call(X(M, insane));
    done;
}

#undef T
#undef MLINE
#undef MCONV
