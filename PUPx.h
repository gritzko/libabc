//  PUPx.h — the Pup template for one record type T, instantiated as abc does:
//  `#define X(M, name) M##u64##name`, `#include "abc/PUPx.h"`, `#undef X`.
//  PUPS-26B045Ffpb: every call takes the house `h` (a_pup, PUPS.h): PAST the
//  mapped file runs, oldest first, DATA and the memtable BOX's; file keys ride
//  along in `keys`.  Needs T, T##Z, T##KeyEq, T##Max, Bx, QSORTx; it brings
//  its own BOXx and HITx.
//  Knobs: PUPx_DEAD(row) the tombstone test (PUPS-001), PUPx_ERR an ok64
//  read after each merge, before the seal (PUPS.md); ABC_X_MERGE (HIT.h).

#include "FILE.h"
#include "HIT.h"
#include "PRO.h"
#include "PUPS.h"

#ifndef ABC_X_NEST
#define ABC_X_NEST
#define PUP_X_OUTER
#endif
#include "BOXx.h"

#ifndef PUPx_DEAD
#define PUPx_DEAD(r) 0
#endif
#ifndef PUPx_ERR
#define PUPx_ERR OK
#endif

//  The one way to read a PAST cell: its whole mapping cut into rows and ToC.
fun void X(PUP, Split)(X(, cs) rows, X(, cs) toc, X(, csc) cell) {
    u8cs raw = {(u8c *)cell[0], (u8c *)cell[1]}, d = {}, t = {};
    PUPTocSplit(d, t, raw, (u32)sizeof(X(, )));
    rows[0] = (X(, ) const *)d[0];
    rows[1] = rows[0] + (size_t)u8csLen(d) / sizeof(X(, ));
    toc[0] = (X(, ) const *)t[0];
    toc[1] = toc[0] + (size_t)u8csLen(t) / sizeof(X(, ));
}

//  D-1: a put is a BOX put; NOROOM says the memtable is full, commit it.
fun ok64 X(PUP, Put)(X(, csb) h, X(, ) const *rec) {
    return X(BOX, Put)(h, rec);
}

//  The file bytes a run of `len` record bytes may take, ToC included.
fun size_t X(PUP, _bound)(size_t len) {
    size_t per = PUPTocRows((u32)sizeof(X(, )));
    return len + (per ? len / per + 2 * sizeof(X(, )) : 0) + PUP_TOC_TAIL_W;
}

//  House `run` under `key` at its sorted spot in PAST; DATA shifts up a cell.
//  The run is the whole mapping, ToC and tail too: PUPBADARG on a misaligned base.
fun ok64 X(PUP, _ins)(X(, csb) h, u64b keys, u8csc run, u64 key) {
    sane(h != NULL && u64bOK(keys));
    X(, cs) **b = (X(, cs) **)h;
    test(b[2] < b[3], PUPNOROOM);
    size_t at = (size_t)(u64sFindGE(u64bDataC(keys), &key) - u64bDataHead(keys));
    u64cs one = {&key, &key + 1};
    test((uintptr_t)run[0] % alignof(X(, )) == 0, PUPBADARG);
    call(u64bSplice, keys, at, 0, one);
    memmove(b[0] + at + 1, b[0] + at,
            ((size_t)(b[2] - b[0]) - at) * sizeof(X(, cs)));
    b[0][at][0] = (X(, ) const *)run[0];
    b[0][at][1] = (X(, ) const *)run[1];
    b[1]++, b[2]++;
    done;
}

//  Unhouse the PAST cells [at, at+m) and their keys; DATA shifts down.
fun void X(PUP, _cut)(X(, csb) h, u64b keys, size_t at, size_t m) {
    X(, cs) **b = (X(, cs) **)h;
    memmove(b[0] + at, b[0] + at + m,
            ((size_t)(b[2] - b[0]) - at - m) * sizeof(X(, cs)));
    b[1] -= m, b[2] -= m;
    u64 const *none[2] = {};
    u64bSplice(keys, at, m, none);
}

//  Look `key` up in one PAST run, ToC-guided: the last ToC cell not above the
//  key names the one page to search (DOG-035).
fun ok64 X(PUP, _find)(X(, ) *out, X(, csc) cell, X(, ) const *key) {
    X(, cs) rows = {}, toc = {};
    X(PUP, Split)(rows, toc, cell);
    if (!$empty(toc)) {
        X(, ) const *ge = X(, sFindGE)(toc, key);
        size_t i = (size_t)(ge - toc[0]), per = PUPTocRows(sizeof(X(, )));
        if (ge < toc[1] && X(, KeyEq)(ge, key)) {
            *out = *ge;
            return OK;
        }
        if (i == 0) return NONE;
        rows[0] += (i - 1) * per;
        if ((size_t)$len(rows) > per) rows[1] = rows[0] + per;
    }
    X(, sc) rr = {(X(, ) *)rows[0], (X(, ) *)rows[1]};
    X(, ) const *p = X(, sBinSearch)(key, rr);
    if (!p) return NONE;
    *out = *p;
    return OK;
}

//  Point lookup: the memtable and DATA (BOX Get), then PAST youngest first,
//  or with ABC_X_MERGE the fold of every version, oldest first, as Read
//  does.  A dead row is NONE.
fun ok64 X(PUP, Get)(X(, ) *out, X(, csb) h, X(, ) const *key) {
#ifdef ABC_X_MERGE
    b8 hit = NO;
    X(, ) row;
    for (X(, cs) const *c = h[0]; c < h[1]; c++)
        if (X(PUP, _find)(&row, *c, key) == OK) X(BOX, _add)(out, &row, &hit);
    ok64 o = X(BOX, _getx)(out, h, key, hit) ? OK : NONE;
#else
    ok64 o = X(BOX, Get)(out, h, key);
    for (X(, cs) const *c = h[1]; o == NONE && c > h[0]; c--)
        o = X(PUP, _find)(out, c[-1], key);
#endif
    return o == OK && PUPx_DEAD(out) ? NONE : o;
}

//  Feed `out` the read view, oldest first: ToC-clipped PAST, then DATA after
//  a seal of the segment (BOX Sort).  PUPNOROOM when `out` is short.
fun ok64 X(PUP, Runs)(X(, csb) out, X(, csb) h) {
    sane(out != NULL && h != NULL);
    call(X(BOX, Sort), h, NO);
    X(, cs) **o = (X(, cs) **)out;
    X(, cs) toc = {};
    for (X(, cs) const *c = h[0]; c < h[2]; c++, o[2]++) {
        test(o[2] < o[3], PUPNOROOM);
        if (c < h[1]) X(PUP, Split)(*o[2], toc, *c);
        else X(, csMv)(*o[2], *c);
    }
    done;
}

//  Range view: fill `into` (at least the run count long) with every run
//  trimmed to [lo, hi), a HITx input as it is; dead rows are the drain's.
fun ok64 X(PUP, Range)(X(, css) into, X(, csb) h, X(, ) const *lo,
                       X(, ) const *hi) {
    X(, cs) *b[4] = {into[0], into[0], into[0], into[1]};
    ok64 o = X(PUP, Runs)(b, h);
    if (o != OK) return o;
    into[1] = b[2];
    o = X(HIT, SeekRange)(into, lo, hi);
    return o == NONE ? OK : o;
}

//  Merge `runs` (consumed) into the IDLE of `into`, the younger row wins;
//  `bottom`: the runs are the whole stack, so dead rows go (PUPS-001).
fun ok64 X(PUP, Merge)(X(, b) into, X(, css) runs, b8 bottom) {
    X(, s) idle = {into[2], into[3]};
    ok64 o = X(HIT, Merge)(idle, runs);
    X(, ) *w = into[2];
    for (X(, ) *r = into[2]; r < idle[0]; r++)
        if (!bottom || !PUPx_DEAD(r)) *w++ = *r;
    X(, bFed)(into, (size_t)(w - into[2]));
    return o == OK ? PUPx_ERR : o;
}

//  The merged view of the whole house, newest row per key, dead rows skipped.
fun ok64 X(PUP, Read)(X(, s) into, X(, csb) h) {
    sane(into != NULL);
    a_pad(X(, cs), srcs, ABC_HIT_MAX_RUNS);
    call(X(PUP, Runs), srcs, h);
    X(, ) *base[4] = {into[0], into[0], into[0], into[1]};
    call(X(PUP, Merge), base, srcs_data, YES);
    into[0] = base[2];
    done;
}

//  Map in the runs other writers sealed since (DOG-046), not those `opt`
//  holds dead: the key pick and the ladder weigh the real stack.
fun ok64 X(PUP, _refresh)(X(, csb) h, u64b keys, path8s dir, u8cs ext,
                          pupopt const *opt) {
    sane(h != NULL && u64bOK(keys));
    a_pad(u64, found, ABC_HIT_MAX_RUNS - 2);
    u64 const *none[2] = {};
    u64 const *const *dead = opt && opt->dead ? u64bDataC(opt->dead) : none;
    call(PUPScan, found, dir, ext, u64max, dead);
    $for(u64c, k, found_datac) {
        b8 held = NO;
        $for(u64c, q, u64bDataC(keys)) held |= *q == *k;
        if (held) continue;
        u8cs run = {};
        call(PUPMap, run, dir, ext, *k);
        try(X(PUP, _ins), h, keys, run, *k);
        nedo {
            PUPUnMap(run);
            fail(__);
        }
    }
    done;
}

fun void X(PUP, _took)(pupopt *opt, u64 key) {
    if (opt && opt->floor <= key) opt->floor = key + 1;
}

//  ToC and seal a filled `tmp` as run `key`, unless PUPx_ERR says otherwise.
fun ok64 X(PUP, _seal)(u8csp run, u8bp tmp, path8s dir, u8cs ext, u64 key,
                       b8 durable) {
    sane(run != NULL && u8bOK(tmp));
    a_dup(u8c, data, u8bDataC(tmp));
    ok64 o = PUPx_ERR;
    if (o == OK) o = PUPTocFeed(tmp, data, (u32)sizeof(X(, )));
    if (o != OK) {
        PUPTmpDrop(tmp, dir, ext, key);
        fail(o);
    }
    call(PUPTmpSeal, run, tmp, dir, ext, key, durable);
    done;
}

//  Write `rows` as a run under `key` and house it, the lock held by the caller.
fun ok64 X(PUP, _create)(X(, csb) h, u64b keys, path8s dir, u8cs ext,
                         X(, csc) rows, u64 key, b8 durable) {
    sane(h != NULL && u64bOK(keys));
    test(Bidlelen(h) > 0 && u64bIdleLen(keys) > 0, PUPNOROOM);
    u8bp tmp = NULL;
    u8cs bytes = {(u8c *)rows[0], (u8c *)rows[1]}, run = {};
    call(PUPTmpMap, &tmp, dir, ext, key, X(PUP, _bound)(u8csLen(bytes)));
    if (!$empty(bytes)) try(u8bFeed, tmp, bytes);
    nedo {
        PUPTmpDrop(tmp, dir, ext, key);
        fail(__);
    }
    call(X(PUP, _seal), run, tmp, dir, ext, key, durable);
    call(X(PUP, _ins), h, keys, run, key);
    done;
}

//  Merge the youngest runs the 1/8 ladder picks into one run, until compact.
//  PUPS-005: the merged run takes the youngest key + 1, never a source's name;
//  the sources go after its rename, oldest first, or to `opt->dead`.
fun ok64 X(PUP, _ladder)(X(, csb) h, u64b keys, path8s dir, u8cs ext,
                         pupopt *opt) {
    sane(h != NULL && u64bOK(keys));
    for (;;) {
        size_t n = (size_t)Bpastlen(h);
        a_pad(X(, cs), srcs, ABC_HIT_MAX_RUNS);
        a_pad(u64, lens, ABC_HIT_MAX_RUNS);
        X(, cs) toc = {};
        for (X(, cs) const *c = h[0]; c < h[1]; c++, srcs[2]++) {
            X(PUP, Split)(*srcs[2], toc, *c);
            u64bFeed1(lens, (u64)$len(*srcs[2]));
        }
        size_t m = HITLadderOverRuns(u64bDataC(lens));
        if (m < 2) done;
        test(!opt || !opt->dead || u64bIdleLen(opt->dead) >= m, PUPNOROOM);
        u64 key = u64bDataHead(keys)[n - 1] + 1;
        if (opt && key < opt->floor) key = opt->floor;
        for (b8 taken = YES;; key++) {
            call(PUPKeyTaken, &taken, u64bDataC(keys), dir, ext, key);
            if (!taken) break;
        }
        size_t total = 0;
        for (size_t i = n - m; i < n; i++) total += u64bDataHead(lens)[i];
        u8bp tmp = NULL;
        call(PUPTmpMap, &tmp, dir, ext, key,
             X(PUP, _bound)(total * sizeof(X(, ))));
        X(, ) *base = (X(, ) *)u8bIdleHead(tmp);
        X(, ) *into[4] = {base, base, base, base + total};
        X(, css) tail = {srcs[2] - m, srcs[2]};
        try(X(PUP, Merge), into, tail, m == n);
        then try(u8bFed, tmp, (size_t)(into[2] - base) * sizeof(X(, )));
        nedo {
            PUPTmpDrop(tmp, dir, ext, key);
            fail(__);
        }
        u8cs run = {};
        if (into[2] == base) {
            call(PUPTmpDrop, tmp, dir, ext, key);
        } else {
            call(X(PUP, _seal), run, tmp, dir, ext, key, YES);
        }
        X(PUP, _took)(opt, key);
        //  house the merged run first: a failed unlink is then a crash past
        //  the rename, which the stack tolerates (PUPS-005)
        a_pad(X(, cs), gone, ABC_HIT_MAX_RUNS);
        a_pad(u64, dk, ABC_HIT_MAX_RUNS);
        X(, cs) const *const cells[2] = {h[1] - m, h[1]};
        u64 const *kp = u64bDataHead(keys) + (n - m);
        u64cs ks = {kp, kp + m};
        memcpy(gone[2], cells[0], m * sizeof(X(, cs)));
        gone[2] += m;
        call(u64bFeed, dk, ks);
        X(PUP, _cut)(h, keys, n - m, m);
        if (!$empty(run)) call(X(PUP, _ins), h, keys, run, key);
        $for(X(, cs), c, gone_data) {
            u8cs src = {(u8c *)(*c)[0], (u8c *)(*c)[1]};
            PUPUnMap(src);
        }
        if (opt && opt->dead) {
            call(u64bFeed, opt->dead, dk_datac);
        } else {
            call(PUPUnlink, dir, ext, dk_data);
        }
    }
}

//  Map `dir`'s runs keyed at or below `ceil` (u64max: all) into the house
//  PAST, oldest first; DATA must be empty.  A run it cannot map fails the open
//  and stays on disk (CODE-26A23CSStk); what is mapped is Close's.
fun ok64 X(PUP, Open)(X(, csb) h, u64b keys, path8s dir, u8cs ext,
                      u64 ceil) {
    sane(h != NULL && u64bOK(keys) && Bdatalen(h) == 0);
    a_pad(u64, found, ABC_HIT_MAX_RUNS - 2);
    u64 const *none[2] = {};
    call(PUPScan, found, dir, ext, ceil, none);
    $for(u64c, k, found_datac) {
        u8cs run = {};
        test(Bidlelen(h) > 0, PUPNOROOM);
        call(PUPMap, run, dir, ext, *k);
        call(X(PUP, _ins), h, keys, run, *k);
    }
    done;
}

//  Unmap every file run and empty the house; the memtable mapping is the
//  owner's (D-2), its rows are dropped.
fun void X(PUP, Close)(X(, csb) h, u64b keys) {
    X(, cs) **b = (X(, cs) **)h;
    for (X(, cs) const *c = h[0]; c < h[1]; c++) {
        u8cs run = {(u8c *)(*c)[0], (u8c *)(*c)[1]};
        PUPUnMap(run);
    }
    b[1] = b[2] = b[0];
    X(, bReset)(X(BOX, Mem)(h));
    u64bReset(keys);
}

//  The 1/8 ladder under the write lock, after a refresh (DOG-046).
fun ok64 X(PUP, Ladder)(X(, csb) h, u64b keys, path8s dir, u8cs ext,
                        pupopt *opt) {
    sane(h != NULL && u64bOK(keys));
    int lk = -1;
    call(PUPLock, &lk, dir);
    try(X(PUP, _refresh), h, keys, dir, ext, opt);
    then try(X(PUP, _ladder), h, keys, dir, ext, opt);
    PUPUnlock(&lk);
    done;
}

//  Write `rows` (sorted, one per key) as a run, under `key` or, with 0, the
//  next free one past `opt->floor`; PUPTAKEN when a run or a file has `key`.
//  CODE-26A23C8gS9: `durable` fsyncs the run and the dir; no ladder.
fun ok64 X(PUP, Create)(X(, csb) h, u64b keys, path8s dir, u8cs ext,
                        X(, csc) rows, u64 key, b8 durable, pupopt *opt) {
    sane(h != NULL && u64bOK(keys));
    int lk = -1;
    call(PUPLock, &lk, dir);
    b8 taken = NO;
    if (key == 0) {
        try(PUPNextKey, &key, u64bDataC(keys), dir, ext, opt ? opt->floor : 0);
    } else {
        try(PUPKeyTaken, &taken, u64bDataC(keys), dir, ext, key);
    }
    if (__ == OK && taken) __ = PUPTAKEN;
    then try(X(PUP, _create), h, keys, dir, ext, rows, key, durable);
    PUPUnlock(&lk);
    nedo fail(__);
    X(PUP, _took)(opt, key);
    done;
}

//  Seal the memtable into one run under the write lock: refresh, BOX Sort all,
//  feed the run to a tmp, seal, house it in PAST, empty the memtable, ladder.
//  No `bottom`: a seal's tombstones still shadow older files.
fun ok64 X(PUP, _commit)(X(, csb) h, u64b keys, path8s dir, u8cs ext,
                         b8 durable, pupopt *opt) {
    sane(h != NULL && u64bOK(keys));
    call(X(PUP, _refresh), h, keys, dir, ext, opt);
    call(X(BOX, Sort), h, YES);
    test(u64bIdleLen(keys) > 0, PUPNOROOM);
    u64 key = 0;
    call(PUPNextKey, &key, u64bDataC(keys), dir, ext, opt ? opt->floor : 0);
    u8cs bytes = {(u8c *)h[1][0][0], (u8c *)h[1][0][1]}, run = {};
    u8bp tmp = NULL;
    call(PUPTmpMap, &tmp, dir, ext, key, X(PUP, _bound)(u8csLen(bytes)));
    try(u8bFeed, tmp, bytes);
    nedo {
        PUPTmpDrop(tmp, dir, ext, key);
        fail(__);
    }
    call(X(PUP, _seal), run, tmp, dir, ext, key, durable);
    X(PUP, _took)(opt, key);
    ((X(, cs) **)h)[2] = h[1];
    X(, bReset)(X(BOX, Mem)(h));
    call(X(PUP, _ins), h, keys, run, key);
    call(X(PUP, _ladder), h, keys, dir, ext, opt);
    done;
}

//  An empty memtable commits nothing.  `durable` NO is the bulk path: the
//  rename stays atomic, a crash leaves a prefix (DOG-032).
fun ok64 X(PUP, Commit)(X(, csb) h, u64b keys, path8s dir, u8cs ext,
                        b8 durable, pupopt *opt) {
    sane(h != NULL && u64bOK(keys));
    X(, ) **mem = X(BOX, Mem)(h);
    if (Bdatalen(h) == 0 && mem[2] == mem[1]) done;
    int lk = -1;
    call(PUPLock, &lk, dir);
    try(X(PUP, _commit), h, keys, dir, ext, durable, opt);
    PUPUnlock(&lk);
    done;
}

//  Unmap and unlink PAST run `i` (oldest first), close the gap; the audit path
//  for a run whose family found no marker (DOG-027).
fun ok64 X(PUP, Drop)(X(, csb) h, u64b keys, path8s dir, u8cs ext, size_t i) {
    sane(h != NULL && u64bOK(keys));
    test(i < (size_t)Bpastlen(h), PUPBADARG);
    u8cs run = {(u8c *)h[0][i][0], (u8c *)h[0][i][1]};
    PUPUnMap(run);
    u64 key = u64bDataHead(keys)[i];
    X(PUP, _cut)(h, keys, i, 1);
    u64s one = {&key, &key + 1};
    call(PUPUnlink, dir, ext, one);
    done;
}

#ifdef PUP_X_OUTER
#undef PUP_X_OUTER
#undef ABC_X_NEST
#undef ABC_X_MERGE
#endif
#undef PUPx_ERR
#undef PUPx_DEAD
