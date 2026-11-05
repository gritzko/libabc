// abc/fuzz/HIT.cpp — FUZZ-26B04IqDqU: HITx vs std::map and std::vector.
//
// The input builds up to 66 strictly ascending runs of {u16 key, u16 val}
// (key 0 and the zero row allowed; past 64 runs Load is FULL), then every
// op runs once on a copy of the runs: Merge and MergeBag into a short `into`
// (NOROOM, resume), Seek, SeekRange, Intersect, sIntersectMerge, Compact.
// The kv lane wants the youngest row, the fold lane the oldest-first compose
// (HIT.c), MergeBag every row, youngest first among equal keys.

#include <algorithm>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include "HITfz.h"
#include "PRO.h"
#include "TEST.h"

#define HF_MAX_INPUT 4096
#define HF_MAX_RUNS 66
#define HF_ROWS 2048

typedef std::vector<hfrun> hfruns;
typedef std::map<u16, u16> hfmodel;

static u8 hf_byte(u8cs in) { return $empty(in) ? 0 : *in[0]++; }

static u16 hf_u16(u8cs in) {
    u16 hi = hf_byte(in);
    return (u16)(hi << 8 | hf_byte(in));
}

static u16 hf_fold(u16 acc, u16 nxt) {
    u8 a = acc >> 8, b = acc & 0xff, na = nxt >> 8, nb = nxt & 0xff;
    return (u16)((u8)(na * a) << 8 | (u8)(na * b + nb));
}

//  The lane's view of runs [from, to): youngest wins or the oldest-first fold.
static hfmodel hf_model(hfruns const &runs, int lane, size_t from = 0) {
    hfmodel m;
    for (size_t i = from; i < runs.size(); i++)
        for (hk const *p = runs[i].lo; p < runs[i].hi; p++) {
            auto it = m.find(p->key);
            if (it == m.end()) m[p->key] = p->val;
            else it->second = lane ? hf_fold(it->second, p->val) : p->val;
        }
    return m;
}

static b8 hf_eq(hfmodel const &m, std::vector<hk> const &out, u32 lo = 0,
                u32 hi = 1 << 16) {
    size_t i = 0;
    for (auto it = m.lower_bound((u16)(lo < 0xffff ? lo : 0xffff));
         it != m.end() && it->first < hi && lo < (1u << 16); ++it, ++i)
        if (i >= out.size() || out[i].key != it->first ||
            out[i].val != it->second)
            return NO;
    return i == out.size();
}

//  The 1/8 ladder (HIT.h) restated: how many youngest runs to collapse.
//  PUPS-26B05Ei2nT: an empty run is out of proportion with any older one.
static b8 hf_compact(std::vector<u64> const &l) {
    for (size_t i = 0; i + 1 < l.size(); i++)
        if (l[i + 1] == 0 || l[i + 1] * 8 > l[i]) return NO;
    return YES;
}

//  The fewest youngest runs whose merge leaves the ladder compact, by trial.
static size_t hf_ladder(hfruns const &runs) {
    std::vector<u64> l;
    for (auto const &r : runs) l.push_back((u64)(r.hi - r.lo));
    if (hf_compact(l)) return 0;
    for (size_t m = 2; m < l.size(); m++) {
        std::vector<u64> t(l.begin(), l.end() - m);
        u64 sum = 0;
        for (size_t i = l.size() - m; i < l.size(); i++) sum += l[i];
        t.push_back(sum);
        if (hf_compact(t)) return m;
    }
    return l.size();
}

//  Merge or MergeBag `chunk` rows at a time, resuming on NOROOM.
static ok64 hf_drain(ok64 *code, std::vector<hk> &out, int lane, b8 bag,
                     hfruns runs, size_t chunk, size_t total) {
    sane(1);
    size_t n = runs.size(), noroom = 0;
    ok64 o = NOROOM;
    out.clear();
    std::vector<hk> buf(chunk);
    while (o == NOROOM) {
        size_t len = chunk;
        o = bag ? hfMergeBag(lane, runs.data(), &n, buf.data(), &len)
                : hfMerge(lane, runs.data(), &n, buf.data(), &len);
        must(len <= chunk, "into overrun");
        out.insert(out.end(), buf.begin(), buf.begin() + len);
        if (o == NOROOM) must(len == chunk && ++noroom <= total, "NOROOM");
    }
    *code = o;
    if (o == OK) must(noroom == (out.empty() ? 0 : (out.size() - 1) / chunk),
                      "NOROOM count");
    done;
}

FUZZ(u8, HITfuzz) {
    sane(1);
    if ($len(input) > HF_MAX_INPUT) input[1] = input[0] + HF_MAX_INPUT;
    a_dup(u8c, in, input);
    size_t chunk = 1 + hf_byte(in);
    u16 seek = hf_u16(in), lo = hf_u16(in), hi = hf_u16(in);
    if (lo > hi) std::swap(lo, hi);
    size_t ccap = 8 * (size_t)hf_byte(in);
    //  build the rows, then copy them to an exact array: ASAN sees overruns
    std::vector<hk> rows;
    std::vector<std::pair<size_t, size_t>> spans;
    while (!$empty(in)) {
        u8 op = hf_byte(in);
        if ((op & 3) == 0 || spans.empty()) {
            if (spans.size() == HF_MAX_RUNS) break;
            spans.push_back({rows.size(), rows.size()});
            if ((op & 3) == 0) continue;
        }
        size_t n = (op & 3) == 3 ? hf_byte(in) : 1;
        u32 step = 1 + ((op >> 2) & 7);
        u16 val = hf_u16(in);
        for (size_t i = 0; i < n && rows.size() < HF_ROWS; i++) {
            auto &s = spans.back();
            u32 key = s.second > s.first ? rows.back().key + step : op >> 5;
            if (key > 0xffff) break;
            rows.push_back(hk{(u16)key, (u16)(val + i)});
            s.second++;
        }
    }
    std::unique_ptr<hk[]> arr(new hk[rows.size() + 1]);
    std::copy(rows.begin(), rows.end(), arr.get());
    hfruns runs;
    for (auto const &s : spans)
        runs.push_back(hfrun{arr.get() + s.first, arr.get() + s.second});
    size_t n = runs.size(), total = rows.size();
    if (n == 0) done;  //  $for over a NULL stack is UB (FUZZ-26B04Iucge)
    b8 full = n > 64;
    hfmodel mk = hf_model(runs, HF_KV), mf = hf_model(runs, HF_FOLD);
    std::vector<hk> out;
    ok64 o = OK;

    //  Merge, both lanes; MergeBag: every row, youngest first per key
    call(hf_drain, &o, out, HF_KV, NO, runs, chunk, total);
    must(full ? o == FULL && out.empty() : o == OK && hf_eq(mk, out), "Merge");
    call(hf_drain, &o, out, HF_FOLD, NO, runs, chunk, total);
    must(full ? o == FULL : o == OK && hf_eq(mf, out), "fold Merge");
    std::vector<std::tuple<u16, size_t, u16>> bag;
    for (size_t r = 0; r < n; r++)
        for (hk const *p = runs[r].lo; p < runs[r].hi; p++)
            bag.push_back({p->key, n - r, p->val});
    std::sort(bag.begin(), bag.end());
    call(hf_drain, &o, out, HF_KV, YES, runs, chunk, total);
    must(full ? o == FULL : o == OK && out.size() == bag.size(), "MergeBag");
    for (size_t i = 0; !full && i < bag.size(); i++)
        must(out[i].key == std::get<0>(bag[i]) &&
                 out[i].val == std::get<2>(bag[i]),
             "MergeBag row");

    //  Seek, then Merge the rest: keys >= seek
    for (int lane = 0; lane < 2; lane++) {
        hfmodel const &m = lane ? mf : mk;
        hfruns c = runs;
        size_t cn = n;
        o = hfSeek(lane, c.data(), &cn, seek);
        b8 any = m.lower_bound(seek) != m.end();
        must(full ? o == FULL : o == (any ? OK : NONE), "Seek code");
        if (full) continue;
        c.resize(cn);
        call(hf_drain, &o, out, lane, NO, c, total + 1, total);
        must(o == OK && hf_eq(m, out, seek), "Seek Merge");
    }
    //  SeekRange [lo, hi), then Merge
    for (int lane = 0; lane < 2; lane++) {
        hfmodel const &m = lane ? mf : mk;
        hfruns c = runs;
        size_t cn = n;
        o = hfSeekRange(lane, c.data(), &cn, lo, hi);
        auto it = m.lower_bound(lo);
        b8 any = it != m.end() && it->first < hi;
        must(full ? o == FULL : o == (any ? OK : NONE), "SeekRange code");
        if (full) continue;
        c.resize(cn);
        call(hf_drain, &o, out, lane, NO, c, total + 1, total);
        must(o == OK && hf_eq(m, out, lo, hi), "SeekRange Merge");
    }
    //  Intersect: keys in every run, the youngest row
    {
        std::map<u16, std::pair<size_t, u16>> cnt;
        for (auto const &r : runs)
            for (hk const *p = r.lo; p < r.hi; p++)
                cnt[p->key].first++, cnt[p->key].second = p->val;
        hfmodel want;
        for (auto const &c : cnt)
            if (c.second.first == n) want[c.first] = c.second.second;
        hfruns c = runs;
        size_t cn = n, len = total + 1;
        out.resize(len);
        o = hfIntersect(c.data(), &cn, n, out.data(), &len);
        out.resize(len);
        must(full ? o == FULL : o == OK && hf_eq(want, out), "Intersect");
    }
    //  sIntersectMerge of the two halves, keys only
    if (n >= 2 && !full) {
        size_t half = n / 2;
        hfmodel a = hf_model(hfruns(runs.begin(), runs.begin() + half), 0),
                b = hf_model(hfruns(runs.begin() + half, runs.end()), 0);
        std::vector<u16> want;
        //  an empty inner HIT is dropped, not intersected (HIT17)
        for (auto const &kv : a.empty() ? b : a)
            if (a.empty() || b.empty() || b.count(kv.first))
                want.push_back(kv.first);
        hfruns c = runs;
        size_t len = total + 1;
        out.resize(len);
        o = hfIntersectMerge(c.data(), n, half, out.data(), &len);
        must(o == OK && len == want.size(), "sIntersectMerge length");
        for (size_t i = 0; i < len; i++)
            must(out[i].key == want[i], "sIntersectMerge key");
    }
    //  Compact into `ccap` rows: the ladder tail collapses, the view stays
    if (!full) {
        size_t m = hf_ladder(runs), tail = 0;
        for (size_t i = n - m; i < n; i++) tail += runs[i].hi - runs[i].lo;
        must(hfCompactRuns(runs.data(), n) == m, "CompactRuns");
        std::vector<u64> lens;
        for (auto const &r : runs) lens.push_back((u64)(r.hi - r.lo));
        b8 ladder = hf_compact(lens);
        must(hfIsCompact(runs.data(), n) == ladder, "IsCompact");
        for (int lane = 0; lane < 2; lane++) {
            hfruns c = runs;
            size_t cn = n, len = ccap;
            std::vector<hk> into(ccap + 1);
            o = hfCompact(lane, c.data(), &cn, into.data(), &len);
            if (m < 2 || tail > ccap) {
                must(o == (m < 2 ? OK : NOROOM) && cn == n && len == 0,
                     "Compact refused");
                continue;
            }
            must(o == OK && cn == n - m + 1, "Compact");
            c.resize(cn);
            call(hf_drain, &o, out, lane, NO, c, total + 1, total);
            must(o == OK && hf_eq(lane ? mf : mk, out), "Compact view");
        }
    }
    done;
}
