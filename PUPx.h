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

//  D-1: a put is a BOX put; BOXNOROOM says the memtable is full, commit it.
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
        if (i == 0) return PUPNONE;
        rows[0] += (i - 1) * per;
        if ((size_t)$len(rows) > per) rows[1] = rows[0] + per;
    }
    X(, sc) rr = {(X(, ) *)rows[0], (X(, ) *)rows[1]};
    X(, ) const *p = X(, sBinSearch)(key, rr);
    if (!p) return PUPNONE;
    *out = *p;
    return OK;
}

//  Point lookup: the memtable and DATA (BOX Get), then PAST youngest first,
//  or with ABC_X_MERGE the fold of every version, oldest first, as Read
//  does.  A miss or a dead row is PUPNONE.
fun ok64 X(PUP, Get)(X(, ) *out, X(, csb) h, X(, ) const *key) {
#ifdef ABC_X_MERGE
    b8 hit = NO;
    X(, ) row;
    for (X(, cs) const *c = h[0]; c < h[1]; c++)
        if (X(PUP, _find)(&row, *c, key) == OK) X(BOX, _add)(out, &row, &hit);
    ok64 o = X(BOX, _getx)(out, h, key, hit) ? OK : PUPNONE;
#else
    ok64 o = X(BOX, Get)(out, h, key);
    for (X(, cs) const *c = h[1]; ok64is(o, NONE) && c > h[0]; c--)
        o = X(PUP, _find)(out, c[-1], key);
#endif
    return (o == OK && PUPx_DEAD(out)) || ok64is(o, NONE) ? PUPNONE : o;
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
    return o == HITNONE ? OK : o;
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

//  PUPS-26A309qbG9: a caller's `floor`, if any: new runs key at or above it,
//  and it is left past the last key used, one counter across stacks.
fun void X(PUP, _took)(u64 *floor, u64 key) {
    if (floor && *floor <= key) *floor = key + 1;
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

//  Write `rows` as run `key`, published without clobbering (PUPTmpSeal);
//  PUPTAKEN when another writer holds the name or its tmp.
fun ok64 X(PUP, _write)(u8csp run, path8s dir, u8cs ext, X(, csc) rows,
                        u64 key, b8 durable) {
    sane(run != NULL);
    u8bp tmp = NULL;
    u8cs bytes = {(u8c *)rows[0], (u8c *)rows[1]};
    call(PUPTmpMap, &tmp, dir, ext, key, X(PUP, _bound)(u8csLen(bytes)));
    if (!$empty(bytes)) try(u8bFeed, tmp, bytes);
    nedo {
        PUPTmpDrop(tmp, dir, ext, key);
        fail(__);
    }
    call(X(PUP, _seal), run, tmp, dir, ext, key, durable);
    done;
}

//  Write `rows` under the next free key at or past `lo`; a key taken in the
//  meantime moves it on (PUPS-26B04KwMzK), so no lock is needed.
fun ok64 X(PUP, _fresh)(u8csp run, u64 *key, u64csc keys, path8s dir,
                        u8cs ext, X(, csc) rows, b8 durable, u64 lo) {
    sane(run != NULL && key != NULL);
    for (ok64 o = PUPTAKEN; o == PUPTAKEN; lo = *key + 1) {
        call(PUPNextKey, key, keys, dir, ext, lo);
        o = X(PUP, _write)(run, dir, ext, rows, *key, durable);
        if (o != OK && o != PUPTAKEN) fail(o);
    }
    done;
}

//  Write `rows` as a run under `key` (0: a fresh one) and house it.
fun ok64 X(PUP, _create)(X(, csb) h, u64b keys, path8s dir, u8cs ext,
                         X(, csc) rows, u64 *key, b8 durable, u64 lo) {
    sane(h != NULL && u64bOK(keys));
    test(h[2] - h[0] < ABC_PUP_MAX_RUNS, PUPFULL);
    test(u64bIdleLen(keys) > 0, PUPNOROOM);
    u8cs run = {};
    b8 taken = NO;
    if (*key == 0) {
        call(X(PUP, _fresh), run, key, u64bDataC(keys), dir, ext, rows,
             durable, lo);
    } else {
        call(PUPKeyTaken, &taken, u64bDataC(keys), dir, ext, *key);
        test(!taken, PUPTAKEN);
        call(X(PUP, _write), run, dir, ext, rows, *key, durable);
    }
    call(X(PUP, _ins), h, keys, run, *key);
    done;
}

//  Merge the youngest runs the 1/8 ladder picks into one run, until compact;
//  at ABC_PUP_MAX_RUNS housed, enough of them to get under it (PUPS-26B05AeY1d).
//  PUPS-005: the merged run takes the youngest key + 1, never a source's name;
//  the sources go after its link, oldest first, or to `dead`.  Only
//  runs in `keys` merge; a key another writer took moves the pick on.
fun ok64 X(PUP, Ladder)(X(, csb) h, u64b keys, path8s dir, u8cs ext,
                        u64b dead, u64 *floor) {
    sane(h != NULL && u64bOK(keys));
    for (u64 lo = 0;;) {
        size_t n = (size_t)Bpastlen(h);
        a_pad(X(, cs), srcs, ABC_HIT_MAX_RUNS);
        a_pad(u64, lens, ABC_HIT_MAX_RUNS);
        X(, cs) toc = {};
        for (X(, cs) const *c = h[0]; c < h[1]; c++, srcs[2]++) {
            X(PUP, Split)(*srcs[2], toc, *c);
            u64bFeed1(lens, (u64)$len(*srcs[2]));
        }
        size_t m = HITLadderOverRuns(u64bDataC(lens));
        size_t over = (size_t)(h[2] - h[0]) + 2;
        if (over > ABC_PUP_MAX_RUNS + m) m = over - ABC_PUP_MAX_RUNS;
        if (m > n) m = n;
        if (m < 2) done;
        test(!dead || u64bIdleLen(dead) >= m, PUPNOROOM);
        u64 key = u64bDataHead(keys)[n - 1] + 1;
        if (floor && key < *floor) key = *floor;
        if (key < lo) key = lo;
        for (b8 taken = YES;; key++) {
            call(PUPKeyTaken, &taken, u64bDataC(keys), dir, ext, key);
            if (!taken) break;
        }
        size_t total = 0;
        for (size_t i = n - m; i < n; i++) total += u64bDataHead(lens)[i];
        u8bp tmp = NULL;
        lo = key + 1;
        ok64 o = PUPTmpMap(&tmp, dir, ext, key,
                           X(PUP, _bound)(total * sizeof(X(, ))));
        if (o == PUPTAKEN) continue;
        if (o != OK) fail(o);
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
        //  PUPS-26B05Ei2nT: all-empty sources leave one empty run, the marker
        if (into[2] == base && total > 0) {
            call(PUPTmpDrop, tmp, dir, ext, key);
        } else {
            o = X(PUP, _seal)(run, tmp, dir, ext, key, YES);
            if (o == PUPTAKEN) continue;
            if (o != OK) fail(o);
        }
        X(PUP, _took)(floor, key);
        //  house the merged run first: a failed unlink is then a crash past
        //  the link, which the stack tolerates (PUPS-005)
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
        if (dead) {
            call(u64bFeed, dead, dk_datac);
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

//  Write `rows` (sorted, one per key) as a run, under `key` or, with 0, the
//  next free one past `*floor`; PUPTAKEN when a run or a file has `key`.
//  CODE-26A23C8gS9: `durable` fsyncs the run and the dir; no ladder.
fun ok64 X(PUP, Create)(X(, csb) h, u64b keys, path8s dir, u8cs ext,
                        X(, csc) rows, u64 key, b8 durable, u64 *floor) {
    sane(h != NULL && u64bOK(keys));
    call(X(PUP, _create), h, keys, dir, ext, rows, &key, durable,
         floor ? *floor : 0);
    X(PUP, _took)(floor, key);
    done;
}

//  Seal the memtable into one run under a fresh key: BOX Sort all, write,
//  house it in PAST, empty the memtable.  No ladder (PUPS-26B04KwMzK), no
//  `bottom`: a seal's tombstones still shadow older files.  An empty memtable
//  commits nothing; `durable` NO is the bulk path, a crash leaves a prefix.
fun ok64 X(PUP, Commit)(X(, csb) h, u64b keys, path8s dir, u8cs ext,
                        b8 durable, u64 *floor) {
    sane(h != NULL && u64bOK(keys));
    X(, ) **mem = X(BOX, Mem)(h);
    if (Bdatalen(h) == 0 && mem[2] == mem[1]) done;
    test(h[2] - h[0] < ABC_PUP_MAX_RUNS, PUPFULL);
    call(X(BOX, Sort), h, YES);
    test(u64bIdleLen(keys) > 0, PUPNOROOM);
    X(, csc) rows = {h[1][0][0], h[1][0][1]};
    u8cs run = {};
    u64 key = 0;
    call(X(PUP, _fresh), run, &key, u64bDataC(keys), dir, ext, rows, durable,
         floor ? *floor : 0);
    X(PUP, _took)(floor, key);
    ((X(, cs) **)h)[2] = h[1];
    X(, bReset)(X(BOX, Mem)(h));
    call(X(PUP, _ins), h, keys, run, key);
    done;
}

//  PUPS-26B04KwMzK: house run `key` of `dir`, typically another writer's
//  found by PUPScan; PUPTAKEN when the house holds it already.
fun ok64 X(PUP, Map)(X(, csb) h, u64b keys, path8s dir, u8cs ext, u64 key) {
    sane(h != NULL && u64bOK(keys));
    $for(u64c, k, u64bDataC(keys)) test(*k != key, PUPTAKEN);
    test(Bidlelen(h) > 0 && u64bIdleLen(keys) > 0, PUPNOROOM);
    u8cs run = {};
    call(PUPMap, run, dir, ext, key);
    try(X(PUP, _ins), h, keys, run, key);
    nedo {
        PUPUnMap(run);
        fail(__);
    }
    done;
}

//  PUPS-26B04KwMzK: unmap PAST run `i` (oldest first) and close the gap; the
//  file stays, e.g. another writer merged it and will unlink it.
fun ok64 X(PUP, Forget)(X(, csb) h, u64b keys, size_t i) {
    sane(h != NULL && u64bOK(keys));
    test(i < (size_t)Bpastlen(h), PUPBADARG);
    u8cs run = {(u8c *)h[0][i][0], (u8c *)h[0][i][1]};
    PUPUnMap(run);
    X(PUP, _cut)(h, keys, i, 1);
    done;
}

//  Forget PAST run `i` and unlink its file; the audit path for a run whose
//  family found no marker (DOG-027).
fun ok64 X(PUP, Drop)(X(, csb) h, u64b keys, path8s dir, u8cs ext, size_t i) {
    sane(h != NULL && u64bOK(keys));
    test(i < (size_t)Bpastlen(h), PUPBADARG);
    u64 key = u64bDataHead(keys)[i];
    call(X(PUP, Forget), h, keys, i);
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
