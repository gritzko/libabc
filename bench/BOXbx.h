//  abc/bench/BOXbx.h — BENCH-26B0559PCK: the BOXb.h entry points over one
//  BOXx.h instance (bb*), or with BB_PUP over one PUPx.h instance (pp*, its
//  runs in a fresh dir under $PUPBENCH_DIR, a commit per full memtable).

#define BB_T X(, )

#ifdef BB_PUP
#define BBx(name) X(pp, name)
#define BBx_H pphouse
#define BBx_GET X(PUP, Get)
#define BBx_RANGE X(PUP, Range)
#else
#define BBx(name) X(bb, name)
#define BBx_H bbhouse
#define BBx_GET X(BOX, Get)
#define BBx_RANGE X(BOX, Range)
#endif

fun X(, cs) **BBx(_h)(BBx_H *b) { return (X(, cs) **)b->h; }

//  The house: no PAST, the memtable mapped for the build and every later put.
fun ok64 BBx(_init)(BBx_H *b, size_t cap) {
    X(, cs) *cells = (X(, cs) *)b->cells;
    b->h[0] = b->h[1] = b->h[2] = cells;
    b->h[3] = cells + ABC_HIT_MAX_RUNS - 2;
    return X(, bMap)(X(BOX, Mem)(BBx(_h)(b)), cap);
}

#ifndef BB_PUP
ok64 X(bb, Init)(bbhouse *b, size_t n) {
    memset(b, 0, sizeof(*b));
    return BBx(_init)(b, 16 * n + (1 << 16));
}

void X(bb, Free)(bbhouse *b) { X(, bUnMap)(X(BOX, Mem)(BBx(_h)(b))); }

ok64 X(bb, Put)(bbhouse *b, BB_T const *rows, size_t n) {
    X(, cs) **h = BBx(_h)(b);
    for (size_t i = 0; i < n; i++) {
        ok64 o = X(BOX, Put)(h, rows + i);
        if (o == BOXNOROOM || o == BOXFULL) {
            X(BOX, Sort)(h, YES);
            o = X(BOX, Put)(h, rows + i);
        }
        if (o != OK) return o;
    }
    return OK;
}

ok64 X(bb, Compact)(bbhouse *b) { return X(BOX, Sort)(BBx(_h)(b), YES); }

size_t X(bb, Bytes)(bbhouse const *b) {
    BB_T *const *m = (BB_T *const *)b->h[3];
    return (size_t)(m[2] - m[0]) * sizeof(BB_T) + sizeof(*b);
}
#else
//  A 1 MB memtable, an in-cache BOX; the rows go to the files, `n` unused.
ok64 X(pp, Init)(pphouse *b, size_t n) {
    memset(b, 0, sizeof(*b));
    u64 **k = b->keys;
    k[0] = k[1] = k[2] = b->keyv;
    k[3] = b->keyv + ABC_HIT_MAX_RUNS - 2;
    char const *root = getenv("PUPBENCH_DIR");
    if (!root) root = "/dev/shm/pupbench";
    mkdir(root, 0700);
    snprintf(b->dir, sizeof(b->dir), "%s/XXXXXX", root);
    if (!mkdtemp(b->dir)) return FAILSANITY;
    a_path(dir, ((u8cs){(u8 *)b->dir, (u8 *)b->dir + strlen(b->dir)}));
    a_cstr(ext, ".pup");
    ok64 o = BBx(_init)(b, (1 << 20) / sizeof(BB_T));
    return o == OK ? X(PUP, Open)(BBx(_h)(b), b->keys, $path(dir), ext, u64max)
                   : o;
}

void X(pp, Free)(pphouse *b) {
    a_path(dir, ((u8cs){(u8 *)b->dir, (u8 *)b->dir + strlen(b->dir)}));
    a_cstr(ext, ".pup");
    X(, cs) **h = BBx(_h)(b);
    while (h[1] > h[0]) X(PUP, Drop)(h, b->keys, $path(dir), ext, 0);
    X(, bUnMap)(X(BOX, Mem)(h));
    char lock[300];
    snprintf(lock, sizeof(lock), "%s/.lock", b->dir);
    unlink(lock);
    rmdir(b->dir);
}

fun ok64 X(pp, _commit)(pphouse *b) {
    a_path(dir, ((u8cs){(u8 *)b->dir, (u8 *)b->dir + strlen(b->dir)}));
    a_cstr(ext, ".pup");
    ok64 o = X(PUP, Commit)(BBx(_h)(b), b->keys, $path(dir), ext, NO, NULL);
    return o == OK ? X(PUP, Ladder)(BBx(_h)(b), b->keys, $path(dir), ext, NULL,
                                    NULL)
                   : o;
}

ok64 X(pp, Put)(pphouse *b, BB_T const *rows, size_t n) {
    X(, cs) **h = BBx(_h)(b);
    for (size_t i = 0; i < n; i++) {
        ok64 o = X(PUP, Put)(h, rows + i);
        if (o == BOXNOROOM || o == BOXFULL) {
            o = X(pp, _commit)(b);
            if (o == OK) o = X(PUP, Put)(h, rows + i);
        }
        if (o != OK) return o;
    }
    return OK;
}

ok64 X(pp, Compact)(pphouse *b) { return X(pp, _commit)(b); }

//  The memtable rows plus every mapped run, its ToC included.
size_t X(pp, Bytes)(pphouse const *b) {
    BB_T *const *m = (BB_T *const *)b->h[3];
    size_t sum = (size_t)(m[2] - m[0]) * sizeof(BB_T) + sizeof(*b);
    for (X(, cs) *c = (X(, cs) *)b->h[0]; c < (X(, cs) *)b->h[1]; c++)
        sum += (size_t)((u8c *)(*c)[1] - (u8c *)(*c)[0]);
    return sum;
}
#endif

size_t BBx(Runs)(BBx_H const *b) {
    return (size_t)((X(, cs) *)b->h[2] - (X(, cs) *)b->h[0]);
}

size_t BBx(Get)(BBx_H *b, BB_T const *keys, size_t n, u64 *sum) {
    X(, cs) **h = BBx(_h)(b);
    size_t hits = 0;
    for (size_t i = 0; i < n; i++) {
        BB_T got;
        if (BBx_GET(&got, h, keys + i) != OK || X(bb, _dead)(&got)) continue;
        hits++;
        *sum += got.key;
    }
    return hits;
}

//  Range plus a HIT drain in 256-row chunks: Merge keeps its state in the
//  run slices, so a BOXNOROOM call resumes where it stopped.
size_t BBx(Scan)(BBx_H *b, BB_T const *lo, BB_T const *hi, u64 *sum) {
    a_pad(X(, cs), into, ABC_HIT_MAX_RUNS);
    if (BBx_RANGE(into_idle, BBx(_h)(b), lo, hi) != OK) return 0;
    BB_T chunk[256];
    size_t rows = 0;
    ok64 o;
    do {
        X(, s) out = {chunk, chunk + 256};
        o = X(HIT, Merge)(out, into_idle);
        for (BB_T const *p = chunk; p < out[0]; p++) {
            if (X(bb, _dead)(p)) continue;
            rows++;
            *sum += p->key;
        }
    } while (o == BOXNOROOM);
    return rows;
}

size_t BBx(Mixed)(BBx_H *b, BB_T const *rows, size_t n, u64 *sum) {
    size_t hits = 0;
    for (size_t i = 0; i < n; i++) {
        if (i % 10 == 9) {
            if (BBx(Put)(b, rows + i, 1) != OK) return 0;
        } else {
            hits += BBx(Get)(b, rows + i, 1, sum);
        }
    }
    return hits;
}

#undef BBx
#undef BBx_H
#undef BBx_GET
#undef BBx_RANGE
#undef BB_T
