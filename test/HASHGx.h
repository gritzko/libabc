//  HASH-26A275TmIX: in-place Grow tests, one instantiation per record
//  type.  Wants X, HASHx.h already instantiated for it, GLINE (its
//  ABC_HASH_LINE), GCONV (its ABC_HASH_CONVERGE) and X(G, of)(k, val):
//  the record keyed by k (nonzero), its value if val.
#define T X(, )

//  every key of keys is found, value and all
ok64 X(G, all)(X($, ) data, u64cs keys) {
    sane(1);
    $for(u64c, k, keys) {
        T g = X(G, of)(*k, NO), w = X(G, of)(*k, YES);
        call(X(HASH, Get), &g, data);
        want(0 == memcmp(&g, &w, sizeof(T)));
    }
    done;
}

size_t X(G, live)(X($, ) data) {
    size_t n = 0;
    $for(T, p, data) if (!X(, IsZero)(p)) ++n;
    return n;
}

//  the convergent layout depends on the key set only: a table Put-built
//  in reverse order at the same length is the very same bytes
ok64 X(G, same)(X($, ) data, u64cs keys) {
    sane(1);
    X(B, ) put = {};
    call(X(, bMap), put, $len(data));
    call(X(, bFed), put, $len(data));
    for (u64 const *k = keys[1]; k > keys[0];) {
        T r = X(G, of)(*--k, YES);
        call(X(HASH, Put), X(, bData)(put), &r);
    }
    want(0 == memcmp(data[0], put[1], $len(data) * sizeof(T)));
    call(X(, bUnMap), put);
    done;
}

//  100k random keys from GLINE slots up, every key checked after every
//  Grow; no stale copies left; CONVERGE: the Put-built layout
ok64 X(G, grow)() {
    sane(1);
    Bu64 keys = {};
    X(B, ) buf = {};
    call(u64bMap, keys, 100000);
    call(X(, bMap), buf, 1 << 22);
    call(X(, bFed), buf, GLINE);
    u64 x = 0x9e3779b97f4a7c15ULL;
    size_t grows = 0;
    while (u64bIdleLen(keys)) {
        x = x * 6364136223846793005ULL + 1442695040888963407ULL;
        if (!x) continue;
        T r = X(G, of)(x, YES);
        size_t ndx = 0;
        while (HASHNOROOM == X(HASH, find)(&ndx, X(, bData)(buf), &r)) {
            size_t was = X(, bDataLen)(buf);
            call(X(HASH, Grow), buf);
            want(X(, bDataLen)(buf) == was * 2);
            ++grows;
            call(X(G, all), X(, bData)(buf), u64bDataC(keys));
        }
        call(X(HASH, Put), X(, bData)(buf), &r);
        call(u64bFeed1, keys, x);
    }
    want(grows >= 10);
    call(X(G, all), X(, bData)(buf), u64bDataC(keys));
    testeqv((long long)X(G, live)(X(, bData)(buf)), 100000LL, "%lld");
    if (GCONV) call(X(G, same), X(, bData)(buf), u64bDataC(keys));
    call(X(, bUnMap), buf);
    call(u64bUnMap, keys);
    done;
}

//  the phases one by one: after phase 1 (twice: idempotent) the old
//  length still finds every key, after the flip and after every single
//  step of phase 3 the new length does; a re-run of phase 3 is a no-op
ok64 X(G, phases)() {
    sane(1);
    Bu64 keys = {};
    X(B, ) buf = {};
    call(u64bMap, keys, 1 << 16);
    call(X(, bMap), buf, 1 << 16);
    call(X(, bFed), buf, GLINE * 16);
    u64 x = 0x2545f4914f6cdd1dULL;
    for (;;) {  // fill until the first full line
        x = x * 6364136223846793005ULL + 1442695040888963407ULL;
        T r = X(G, of)(x | 1, YES);
        size_t ndx = 0;
        if (HASHNOROOM == X(HASH, find)(&ndx, X(, bData)(buf), &r)) break;
        call(X(HASH, Put), X(, bData)(buf), &r);
        call(u64bFeed1, keys, x | 1);
    }
    size_t len = X(, bDataLen)(buf), n = u64bDataLen(keys);
    X($, ) wide = {buf[1], buf[2] + len};
    X(HASH, GrowCopy)(wide);
    X(HASH, GrowCopy)(wide);
    call(X(G, all), X(, bData)(buf), u64bDataC(keys));
    want(X(G, live)(X(, bData)(buf)) == n);
    want(X(G, live)(wide) > n);  // the stale copies
    call(X(, bFed), buf, len);
    call(X(G, all), X(, bData)(buf), u64bDataC(keys));
    size_t steps = 0, was = 1;
    while (was != steps) {  // sweep until clean, as GrowClean does
        was = steps;
        for (size_t i = 0; i < len; ++i)
            while (OK == X(HASH, unstale)(wide, i)) {
                ++steps;
                call(X(G, all), X(, bData)(buf), u64bDataC(keys));
            }
    }
    want(steps > 0);
    testeqv((long long)X(G, live)(wide), (long long)n, "%lld");
    X(B, ) cpy = {};
    call(X(, bMap), cpy, len * 2);
    memcpy(cpy[0], wide[0], len * 2 * sizeof(T));
    X(HASH, GrowClean)(wide);
    want(0 == memcmp(cpy[0], wide[0], len * 2 * sizeof(T)));
    if (GCONV) call(X(G, same), wide, u64bDataC(keys));
    call(X(, bUnMap), cpy);
    call(X(, bUnMap), buf);
    call(u64bUnMap, keys);
    done;
}

//  Grow never maps: IDLE shorter than DATA is BNOROOM, table untouched
ok64 X(G, room)() {
    sane(1);
    X(B, ) buf = {};
    call(X(, bMap), buf, GLINE * 4 + GLINE / 2);
    call(X(, bFed), buf, GLINE);
    T r = X(G, of)(7, YES);
    call(X(HASH, Put), X(, bData)(buf), &r);
    call(X(HASH, Grow), buf);
    call(X(HASH, Grow), buf);
    want(X(, bDataLen)(buf) == GLINE * 4);
    want(BNOROOM == X(HASH, Grow)(buf));
    want(X(, bDataLen)(buf) == GLINE * 4);
    T g = X(G, of)(7, NO);
    call(X(HASH, Get), &g, X(, bData)(buf));
    want(0 == memcmp(&g, &r, sizeof(T)));
    call(X(, bUnMap), buf);
    done;
}

//  Del from a full line (no empty slot stops the shift), each key in
//  turn: every other key is still found (the shift now may run past
//  one lap)
ok64 X(G, fulldel)() {
    sane(1);
    Bu64 keys = {};
    X(B, ) buf = {}, full = {};
    call(u64bMap, keys, GLINE);
    call(X(, bMap), buf, GLINE);
    call(X(, bFed), buf, GLINE);
    call(X(, bMap), full, GLINE);
    for (u64 k = 1; u64bIdleLen(keys); ++k) {
        T r = X(G, of)(k, YES);
        size_t ndx = 0;
        if (HASHNOROOM == X(HASH, find)(&ndx, X(, bData)(buf), &r)) continue;
        call(X(HASH, Put), X(, bData)(buf), &r);
        call(u64bFeed1, keys, k);
    }
    want(X(G, live)(X(, bData)(buf)) == GLINE);
    memcpy(full[0], buf[1], GLINE * sizeof(T));
    for (size_t i = 0; i < GLINE; ++i) {
        memcpy(buf[1], full[0], GLINE * sizeof(T));
        T d = X(G, of)(keys[1][i], NO);
        call(X(HASH, Del), X(, bData)(buf), &d);
        want(X(G, live)(X(, bData)(buf)) == GLINE - 1);
        u64cs pre = {keys[1], keys[1] + i}, post = {keys[1] + i + 1, keys[2]};
        call(X(G, all), X(, bData)(buf), pre);
        call(X(G, all), X(, bData)(buf), post);
    }
    call(X(, bUnMap), full);
    call(X(, bUnMap), buf);
    call(u64bUnMap, keys);
    done;
}

ok64 X(G, test)() {
    sane(1);
    call(X(G, fulldel));
    call(X(G, grow));
    call(X(G, phases));
    call(X(G, room));
    done;
}

#undef T
#undef GLINE
#undef GCONV
