//  abc/fuzz/PUPSx.h — PUPS-26B04CThJv: one lane of the PUPS house fuzz, a
//  PUPx instance T (key, val) driven op by op against a plain array model:
//  committed and pending (memtable) values per u16 key.  PF_SUM: the lane
//  sums vals (ABC_X_MERGE), else the youngest row wins and PF_TOMB is dead.
//  Every lane call takes the house, its keys, the dir and the run ext.

#define PF_ARGS X(, csb) h, u64b keys, path8s dir, u8cs ext
#define PF_PASS h, keys, dir, ext
#define PF_KEYS (1u << 16)

static u64 X(PF, _cv)[PF_KEYS], X(PF, _pv)[PF_KEYS];
static u8 X(PF, _has)[PF_KEYS];         //  1: committed, 2: pending
static u64 X(PF, _lb)[PF_KEYS / 64];    //  live keys, so checks walk rows
static u16 X(PF, _pk)[PF_KEYS];         //  the pending keys, unordered
static u32 X(PF, _npk);
static X(, ) X(PF, _buf)[PF_KEYS + 1];

fun void X(PF, Reset)(void) {
    memset(X(PF, _has), 0, sizeof X(PF, _has));
    memset(X(PF, _lb), 0, sizeof X(PF, _lb));
    X(PF, _npk) = 0;
}

//  The model's live row of `k`: pending over committed (summed with PF_SUM).
fun b8 X(PF, _live)(u64 *v, u32 k) {
    u8 has = X(PF, _has)[k];
    if (!has) return NO;
#ifdef PF_SUM
    *v = (has & 1 ? X(PF, _cv)[k] : 0) + (has & 2 ? X(PF, _pv)[k] : 0);
    return YES;
#else
    *v = has & 2 ? X(PF, _pv)[k] : X(PF, _cv)[k];
    return *v != PF_TOMB;
#endif
}

fun void X(PF, _mark)(u32 k) {
    u64 v = 0, bit = 1ULL << (k & 63);
    if (X(PF, _live)(&v, k)) X(PF, _lb)[k >> 6] |= bit;
    else X(PF, _lb)[k >> 6] &= ~bit;
}

//  Live keys in [lo, hi).
fun u64 X(PF, _count)(u32 lo, u32 hi) {
    u64 n = 0;
    u64 const *lb = X(PF, _lb);
    u32 k = lo;
    for (; k < hi && (k & 63); k++) n += lb[k >> 6] >> (k & 63) & 1;
    for (; k + 64 <= hi; k += 64) n += (u64)__builtin_popcountll(lb[k >> 6]);
    for (; k < hi; k++) n += lb[k >> 6] >> (k & 63) & 1;
    return n;
}

fun void X(PF, _pend)(u32 k, u64 v) {
    if (!(X(PF, _has)[k] & 2)) {
        X(PF, _has)[k] |= 2;
        X(PF, _pv)[k] = 0;
        X(PF, _pk)[X(PF, _npk)++] = (u16)k;
    }
#ifdef PF_SUM
    X(PF, _pv)[k] += v;
#else
    X(PF, _pv)[k] = v;
#endif
    X(PF, _mark)(k);
}

//  A file row: older than any memtable row, younger than every other file.
fun void X(PF, _file)(u32 k, u64 v) {
#ifdef PF_SUM
    if (X(PF, _has)[k] & 1) v += X(PF, _cv)[k];
#endif
    X(PF, _cv)[k] = v;
    X(PF, _has)[k] |= 1;
    X(PF, _mark)(k);
}

//  `commit` YES: the memtable went to a file; NO: it was dropped (reopen).
fun void X(PF, _settle)(b8 commit) {
    for (u32 i = 0; i < X(PF, _npk); i++) {
        u16 k = X(PF, _pk)[i];
        X(PF, _has)[k] &= 1;
        if (commit) X(PF, _file)(k, X(PF, _pv)[k]);
        X(PF, _mark)(k);
    }
    X(PF, _npk) = 0;
}

//  Merged rows (dead ones in) vs the model's live keys in [lo, hi).
fun ok64 X(PF, _rows)(X(, csc) rows, u32 lo, u32 hi) {
    sane(1);
    u64 n = 0, v = 0;
    $for(X(, ) const, r, rows) {
        must(r->key >= lo && r->key < hi, "rows: out of range");
        if (!X(PF, _live)(&v, (u32)r->key)) {
#ifndef PF_SUM
            if (r->val == PF_TOMB) continue;
#endif
            must(NO, "rows: ghost row");
        }
        must(r->val == v, "rows: value");
        n++;
    }
    must(n == X(PF, _count)(lo, hi), "rows: row missing");
    done;
}

//  Point lookup vs the model.
fun ok64 X(PF, Get)(PF_ARGS, u32 k) {
    sane(1);
    X(, ) key = {k, 0}, got = {};
    u64 v = 0;
    ok64 o = X(PUP, Get)(&got, h, &key);
    if (X(PF, _live)(&v, k)) {
        must(o == OK && got.key == k && got.val == v, "Get: row");
    } else {
        must(o == PUPNONE, "Get: ghost row");
    }
    done;
}

//  Merge the house's [lo, hi) and compare with the model, if `deep`.  BOXFULL: the house
//  has no cell to seal the segment in, so there is no view to check.
fun ok64 X(PF, Range)(PF_ARGS, u32 lo, u32 hi, b8 deep) {
    sane(1);
    X(, ) l = {lo, 0}, r = {hi, 0};
    X(, cs) runs[ABC_HIT_MAX_RUNS];
    X(, css) into = {runs, runs + ABC_HIT_MAX_RUNS};
    ok64 o = X(PUP, Range)(into, h, &l, &r);
    if (o == BOXFULL && Bidlelen(h) == 0) done;
    must(o == OK, "Range");
    if (!deep) done;
    X(, s) out = {X(PF, _buf), X(PF, _buf) + PF_KEYS + 1};
    call(X(HIT, Merge), out, into);
    X(, csc) rows = {X(PF, _buf), out[0]};
    call(X(PF, _rows), rows, lo, hi);
    done;
}

//  The whole merged view (Read) vs the model.
fun ok64 X(PF, Check)(PF_ARGS) {
    sane(1);
    X(, s) out = {X(PF, _buf), X(PF, _buf) + PF_KEYS + 1};
    ok64 o = X(PUP, Read)(out, h);
    if (o == BOXFULL && Bidlelen(h) == 0) done;
    must(o == OK, "Read");
    X(, csc) rows = {X(PF, _buf), out[0]};
    call(X(PF, _rows), rows, 0, PF_KEYS);
    done;
}

//  The runs the ladder would merge now; 0 when its tail rule holds.
fun size_t X(PF, _over)(X(, csb) h) {
    a_pad(u64, lens, ABC_HIT_MAX_RUNS);
    X(, cs) rows = {}, toc = {};
    for (X(, cs) const *c = h[0]; c < h[1]; c++) {
        X(PUP, Split)(rows, toc, *c);
        u64bFeed1(lens, (u64)$len(rows));
    }
    return HITLadderOverRuns(u64bDataC(lens));
}

fun ok64 X(PF, Ladder)(PF_ARGS) {
    sane(1);
    call(X(PUP, Ladder), PF_PASS, NULL, NULL);
    must(X(PF, _over)(h) == 0, "Ladder: tail left");
    done;
}

//  PUPS-26B05AeY1d: at the cap Commit is PUPFULL; the caller ladders, retries.
fun ok64 X(PF, Commit)(PF_ARGS, b8 durable) {
    sane(1);
    ok64 o = X(PUP, Commit)(PF_PASS, durable, NULL);
    if (o == PUPFULL) {
        call(X(PF, Ladder), PF_PASS);
        o = X(PUP, Commit)(PF_PASS, durable, NULL);
    }
    must(o == OK, "Commit");
    X(PF, _settle)(YES);
    must(X(BOX, Mem)(h)[2] == X(BOX, Mem)(h)[0], "Commit: memtable left");
    call(X(PF, Ladder), PF_PASS);  //  PUPS-26B04KwMzK: Commit ladders no more
    done;
}

//  D-1: the owner commits a full memtable (BOXNOROOM) and ladders a full house.
fun ok64 X(PF, Put)(PF_ARGS, u32 k, u64 v) {
    sane(1);
    X(, ) row = {k, v};
    ok64 o = X(PUP, Put)(h, &row);
    if (o == BOXNOROOM) {
        call(X(PF, Commit), PF_PASS, NO);
        o = X(PUP, Put)(h, &row);
    }
    if (o == BOXFULL) {
        call(X(PF, Ladder), PF_PASS);
        o = X(PUP, Put)(h, &row);
    }
    must(o == OK, "Put");
    X(PF, _pend)(k, v);
    done;
}

//  `n` rows from `k` by `step` as one run (no ladder), so the cap is reached.
fun ok64 X(PF, Create)(PF_ARGS, u32 k, u32 step, u32 n, u64 v) {
    sane(n <= 64 && step > 0);
    X(, ) rows[64];
    u32 m = 0;
    for (; m < n && k + m * step < PF_KEYS; m++)
        rows[m] = (X(, )){k + m * step, v + m};
    X(, csc) rs = {rows, rows + m};
    ok64 o = X(PUP, Create)(PF_PASS, rs, 0, NO, NULL);
    if (o == PUPFULL) {
        call(X(PF, Ladder), PF_PASS);
        o = X(PUP, Create)(PF_PASS, rs, 0, NO, NULL);
    }
    must(o == OK, "Create");
    for (u32 i = 0; i < m; i++) X(PF, _file)(rows[i].key, rows[i].val);
    done;
}

//  Close drops the memtable; every committed row is back after the open.
fun ok64 X(PF, Reopen)(PF_ARGS) {
    sane(1);
    X(PUP, Close)(h, keys);
    X(PF, _settle)(NO);
    call(X(PUP, Open), PF_PASS, u64max);
    done;
}

//  The one compare after the program: Read and Range of all, Get of 1/64.
fun ok64 X(PF, Final)(PF_ARGS, u32 seed) {
    sane(1);
    call(X(PF, Check), PF_PASS);
    call(X(PF, Range), PF_PASS, 0, PF_KEYS, YES);
    for (u32 k = seed & 63; k < PF_KEYS; k += 64) call(X(PF, Get), PF_PASS, k);
    done;
}

#undef PF_ARGS
#undef PF_PASS
#undef PF_KEYS
#undef PF_SUM
