// abc/fuzz/BOX.cpp — FUZZ-26B04IqDqU: the house BOX vs std::map.
//
// A byte program of Put, PutBatch, Get, Sort, Range over {u16 key, u16 val};
// the kv lane (youngest wins) and the sum lane (ABC_X_MERGE) run it in step.
// Per op only codes (and Gets) are checked, a failed write by its snapshot;
// at the end a full Range+Merge, Gets and Sort(all) against the map.  The
// header bytes pick the PAST cells (few DATA cells: BOXFULL) and a small
// memtable (BOXNOROOM).  wiki/Containers.mkd is the contract; the C side: BOX.c.

#include <map>
#include <vector>

#include "BOXfz.h"
#include "HIT.h"
#include "PRO.h"
#include "TEST.h"

#define BF_MAX_INPUT 4096

typedef std::map<u16, u16> bfmodel;

static u8 bf_byte(u8cs in) { return $empty(in) ? 0 : *in[0]++; }

//  Mostly a 1-byte key (folds), from 0xf0 on a full u16 (u16max = BOXBADARG).
static u16 bf_key(u8cs in) {
    u8 b = bf_byte(in);
    if (b < 0xf0) return b;
    u16 hi = bf_byte(in);
    return (u16)(hi << 8 | bf_byte(in));
}

static void bf_apply(bfmodel &m, int lane, bk const &r) {
    if (lane == BF_SUM) m[r.key] += r.val;
    else m[r.key] = r.val;
}

static ok64 bf_put(ok64 *code, bfmodel &m, int lane, bk r) {
    sane(1);
    b8 noroom = bfNoRoom(lane), nocell = bfNoCell(lane), same = NO;
    ok64 o = bfPut(lane, r, &same);
    *code = o;
    if (r.key == u16max) {
        must(o == BOXBADARG, "Put Max");
    } else if (o == OK) {
        bf_apply(m, lane, r);
    } else {
        must(o == BOXNOROOM ? noroom : o == BOXFULL && nocell, "Put code");
    }
    if (o != OK) must(same, "failed Put changed the BOX");
    done;
}

static ok64 bf_batch(ok64 *code, bfmodel &m, int lane,
                     std::vector<bk> const &rows) {
    sane(1);
    b8 seg = bfSeg(lane), max = NO, same = NO;
    for (bk const &r : rows) max |= r.key == u16max;
    ok64 o = bfBatch(lane, rows.data(), rows.size(), &same);
    *code = o;
    if (max) {
        must(o == BOXBADARG, "PutBatch Max");
    } else if (o == OK) {
        for (bk const &r : rows) bf_apply(m, lane, r);
    } else {
        must(o == BOXNOROOM || o == BOXFULL, "PutBatch code");
    }
    //  a refused batch may have sealed the pending segment (Containers.mkd)
    if (o != OK && (max || !seg)) must(same, "failed PutBatch changed BOX");
    done;
}

static ok64 bf_get(bfmodel const &m, int lane, u16 k) {
    sane(1);
    bk got = {};
    ok64 o = bfGet(lane, k, &got);
    auto it = m.find(k);
    if (it != m.end()) {
        must(o == OK && got.key == k && got.val == it->second, "Get");
    } else {
        must(o == BOXNONE, "Get absent");
    }
    done;
}

static bk OUT[BF_ROWS];

static ok64 bf_range(ok64 *code, bfmodel const &m, int lane, u16 lo, u16 hi,
                     size_t slots) {
    sane(1);
    size_t n = 0;
    ok64 o = bfRange(lane, lo, hi, slots, OUT, &n);
    *code = o;
    size_t runs = bfRuns(lane);
    if (o == BOXNOROOM) must(runs > slots, "Range BOXNOROOM");
    if (o != OK) {
        must(o == BOXNOROOM || o == BOXFULL, "Range code");
        done;
    }
    must(runs <= slots, "Range runs");
    size_t i = 0;
    for (auto it = m.lower_bound(lo); it != m.end() && it->first < hi; ++it) {
        must(i < n && OUT[i].key == it->first && OUT[i].val == it->second,
             "Range row");
        i++;
    }
    must(i == n, "Range extra rows");
    done;
}

FUZZ(u8, BOXfuzz) {
    sane(1);
    if ($len(input) > BF_MAX_INPUT) input[1] = input[0] + BF_MAX_INPUT;
    a_dup(u8c, in, input);
    u8 lay = bf_byte(in);
    size_t past = (lay & 0x40) ? 56 + lay % 7 : 0;  //  62: no DATA cell
    size_t cap = 4 + 4 * (size_t)bf_byte(in);
    if (cap > BF_ROWS) cap = BF_ROWS;
    bfInit(past, cap);
    bfmodel mk, mc;
    ok64 ok = OK, oc = OK;
    while (!$empty(in)) {
        u8 op = bf_byte(in);
        switch (op & 7) {
            case 0 ... 3: {
                bk r = {bf_key(in), bf_byte(in)};
                call(bf_put, &ok, mk, BF_KV, r);
                call(bf_put, &oc, mc, BF_SUM, r);
            } break;
            case 4: {
                size_t n = (op >> 3) & 15;
                std::vector<bk> kb, cb;
                for (size_t i = 0; i < n; i++) {
                    bk r = {bf_key(in), bf_byte(in)};
                    bk q = r;
                    //  a batch folds equal keys in no order: one value each
                    for (bk const &e : kb)
                        if (e.key == r.key) q = e;
                    kb.push_back(q);
                    cb.push_back(r);
                }
                call(bf_batch, &ok, mk, BF_KV, kb);
                call(bf_batch, &oc, mc, BF_SUM, cb);
            } break;
            case 5: {
                u16 k = bf_key(in);
                call(bf_get, mk, BF_KV, k);
                call(bf_get, mc, BF_SUM, k);
                ok = oc;
            } break;
            case 6: {
                b8 same = NO;
                ok = bfSort(BF_KV, (op & 8) != 0, &same);
                if (ok != OK) must(ok == BOXFULL && same, "Sort");
                oc = bfSort(BF_SUM, (op & 8) != 0, &same);
            } break;
            case 7: {
                u16 lo = bf_key(in), hi = bf_key(in);
                if (lo > hi) std::swap(lo, hi);
                call(bf_range, &ok, mk, BF_KV, lo, hi, op >> 3);
                call(bf_range, &oc, mc, BF_SUM, lo, hi, op >> 3);
            } break;
        }
        must(ok == oc, "lanes disagree");
    }
    //  the final compare: all below Max, every key and its next, Sort(all)
    ok64 rk = OK, rc = OK;
    call(bf_range, &rk, mk, BF_KV, 0, u16max, ABC_HIT_MAX_RUNS);
    call(bf_range, &rc, mc, BF_SUM, 0, u16max, ABC_HIT_MAX_RUNS);
    must(rk == rc && (rk == OK || rk == BOXFULL), "final Range");
    for (auto const &kv : mk) {
        call(bf_get, mk, BF_KV, kv.first);
        call(bf_get, mk, BF_KV, (u16)(kv.first + 1));
        call(bf_get, mc, BF_SUM, kv.first);
        call(bf_get, mc, BF_SUM, (u16)(kv.first + 1));
    }
    b8 same = NO;
    for (int lane = 0; lane < 2; lane++) {
        if (bfSort(lane, YES, &same) != OK) continue;
        must(bfRuns(lane) <= 1, "one run");
        call(bf_range, &rk, lane ? mc : mk, lane, 0, u16max, 1);
        must(rk == OK, "one-run Range");
    }
    done;
}
