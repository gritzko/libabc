//  FUZZ-26B04IZDFD: HASHx diffed against std::unordered_map.  A byte program
//  of Put/PutGrow/Del/Get/At/erase-while-iterating/Grow/Move over u16 keys
//  (0 included) runs on a growable table.  Every return code must match the
//  model's prediction (HASHFULL: the key's line holds LINE keys; HASHNOROOM: no IDLE
//  to double into), a failed write must leave the table byte-equal to a
//  snapshot, and table and model are compared in full once, at the end.
//  Lanes: kv32 at the default line; sum32 at line 16 with ABC_HASH_MERGE.
//  Checks trap, NDEBUG or not.
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

#include "INT.h"
#include "KV.h"
#include "PRO.h"

ABC_INIT

#define X(M, name) M##kv32##name
#include "HASHx.h"
#undef X

typedef struct sum32 {
    u32 key;
    u32 val;
} sum32;
fun u64 sum32Hash(sum32 const *v) { return mix32(v->key); }
fun b8 sum32Z(sum32 const *a, sum32 const *b) { return a->key < b->key; }
fun b8 sum32KeyEq(sum32 const *a, sum32 const *b) { return a->key == b->key; }
fun void sum32Merge(sum32 *acc, sum32 const *nxt) { acc->val += nxt->val; }
#define X(M, name) M##sum32##name
#include "Bx.h"
#define ABC_HASH_LINE 16
#define ABC_HASH_MERGE
#include "HASHx.h"
#undef X

#define CHECK(c)                       \
    do {                               \
        if (!(c)) __builtin_trap();    \
    } while (0)

// Overloads route the Lane template to each instantiation.
#define OPS(R)                                                              \
    fun u64 Hash(R const *r) { return R##Hash(r); }                         \
    fun ok64 Put(R **d, R const *r) { return HASH##R##Put(d, r); }          \
    fun ok64 PutGrow(R **b, R const *r) { return HASH##R##PutGrow(b, r); }  \
    fun ok64 Get(R *r, R **d) { return HASH##R##Get(r, d); }                \
    fun ok64 At(R const **s, R **d, R const *r) {                           \
        return HASH##R##At(s, d, r);                                        \
    }                                                                       \
    fun ok64 Del(R **d, R const *r) { return HASH##R##Del(d, r); }          \
    fun ok64 DelAt(R **d, size_t i) { return HASH##R##DelAt(d, i); }        \
    fun ok64 Next(size_t *i, R **d) { return HASH##R##Next(i, d); }         \
    fun ok64 Grow(R **b) { return HASH##R##Grow(b); }                       \
    fun ok64 Move(R **into, R **from) { return HASH##R##Move(into, from); } \
    fun b8 IsZero(R const *r) { return R##IsZero(r); }                     \
    fun ok64 Map(R **b, size_t n) { return R##bMap(b, n); }
OPS(kv32)
OPS(sum32)

template <class R, size_t LINE, bool MERGE>
struct Lane {
    static constexpr size_t CAP = 1024;  // slots mapped per buffer
    R *tab[4] = {}, *aux[4] = {};
    size_t len = 0;
    std::unordered_map<u16, u32> ref;
    std::vector<u32> lines;  // ref keys homed in each line of tab
    std::vector<R> snap;

    Lane() { CHECK(OK == Map(tab, CAP) && OK == Map(aux, CAP)); }
    // a table of n zeroed slots in buffer b; the room behind stays dirty
    static void reset(R **b, size_t n) {
        memset((void *)b[0], 0, n * sizeof(R));
        b[1] = b[0];
        b[2] = b[0] + n;
    }
    R *data[2];
    R **D() { return data[0] = tab[1], data[1] = tab[2], data; }
    size_t line(u16 k, size_t n) {
        R r = {k, 0};
        return (Hash(&r) & (n - 1)) / LINE;
    }
    void relines() {
        lines.assign(len / LINE, 0);
        for (auto &kv : ref) lines[line(kv.first, len)]++;
    }
    void save() { snap.assign(tab[1], tab[1] + len); }
    bool same() {
        return 0 == memcmp((void *)snap.data(), (void *)tab[1], len * sizeof(R));
    }
    bool full(u16 k) { return !ref.count(k) && lines[line(k, len)] == LINE; }
    void model_put(u16 k, u32 v) {
        auto it = ref.find(k);
        if (it == ref.end()) {
            ref[k] = v;
            lines[line(k, len)]++;
        } else {
            it->second = MERGE ? it->second + v : v;
        }
    }

    void put(u16 k, u32 v) {
        R r = {k, v};
        ok64 w = full(k) ? HASHFULL : OK;
        if (w != OK) save();
        CHECK(w == Put(D(), &r));
        if (w != OK) {
            CHECK(same());
            return;
        }
        model_put(k, v);
    }
    void putgrow(u16 k, u32 v) {
        R r = {k, v};
        ok64 w = OK;
        while (w == OK && full(k)) {
            if (2 * len > CAP) w = HASHNOROOM;
            else len *= 2, relines();
        }
        CHECK(w == PutGrow(tab, &r) && len == size_t(tab[2] - tab[1]));
        if (w == OK) model_put(k, v);
    }
    void del(u16 k) {
        R r = {k, 0};
        ok64 w = ref.count(k) ? OK : HASHNONE;
        if (w != OK) save();
        CHECK(w == Del(D(), &r));
        if (w != OK) {
            CHECK(same());
            return;
        }
        lines[line(k, len)]--;
        ref.erase(k);
    }
    void get(u16 k) {
        R r = {k, 0};
        auto it = ref.find(k);
        ok64 o = Get(&r, D());
        CHECK(o == (it == ref.end() ? HASHNONE : OK));
        if (o == OK) CHECK(r.key == k && r.val == it->second);
    }
    void at(u16 k) {
        R r = {k, 0};
        R const *s = NULL;
        auto it = ref.find(k);
        ok64 o = At(&s, D(), &r);
        CHECK(o == (it == ref.end() ? HASHNONE : OK));
        if (o == OK) CHECK(s->key == k && s->val == it->second);
    }
    // HASH.md "Delete while iterating": erase the keys matching m on bits
    // [0, b); every one must go, DelAt OK iff its slot got refilled
    void erase(u8 m, u8 b) {
        u32 mask = (1u << b) - 1;
        size_t want = 0, got = 0, steps = 0;
        for (auto it = ref.begin(); it != ref.end();) {
            if ((it->first ^ m) & mask) {
                ++it;
                continue;
            }
            lines[line(it->first, len)]--;
            it = ref.erase(it);
            ++want;
        }
        R **d = D();
        for (size_t i = 0; OK == Next(&i, d);) {
            CHECK(++steps <= 2 * len);
            R *s = d[0] + i;
            if ((s->key ^ m) & mask) {
                ++i;
                continue;
            }
            ok64 o = DelAt(d, i);
            ++got;
            CHECK(o == (IsZero(s) ? HASHNONE : OK));
            if (o != OK) ++i;
        }
        CHECK(got == want);
    }
    void grow() {
        ok64 w = 2 * len <= CAP ? OK : HASHNOROOM;
        if (w != OK) save();
        CHECK(w == Grow(tab));
        if (w != OK) {
            CHECK(same());
            return;
        }
        len *= 2;
        relines();
    }
    // Move into an empty table of n slots: HASHFULL at the first entry (in tab's
    // slot order) whose line is full, the entries before it moved, tab intact
    void move(size_t n) {
        reset(aux, n);
        std::vector<u32> cnt(n / LINE);
        size_t fit = 0, i = 0;
        R **d = D();
        for (; OK == Next(&i, d); ++i, ++fit) {
            u32 &c = cnt[line(d[0][i].key, n)];
            if (c == LINE) break;
            ++c;
        }
        ok64 w = i < len ? HASHFULL : OK;
        save();
        CHECK(w == Move(aux, tab) && same());
        if (w == OK) {
            std::swap(tab, aux);
            len = n;
            return relines();
        }
        R *a[2] = {aux[1], aux[2]};
        size_t live = 0;
        for (size_t j = 0; OK == Next(&j, a); ++j) ++live;
        CHECK(live == fit);
        for (size_t j = 0; fit > 0; ++j) {
            if (IsZero(d[0] + j)) continue;
            R r = {d[0][j].key, 0};
            CHECK(OK == Get(&r, a) && r.val == d[0][j].val);
            --fit;
        }
    }
    // the one full comparison: every live slot is a ref key in its home
    // line with ref's value, and every ref key is there
    void compare() {
        R **d = D();
        CHECK(len == size_t(d[1] - d[0]));
        size_t live = 0;
        for (size_t i = 0; OK == Next(&i, d); ++i, ++live) {
            R *s = d[0] + i;
            auto it = ref.find(s->key);
            CHECK(s->key <= 0xffff && it != ref.end() && it->second == s->val);
            CHECK(line(s->key, len) == i / LINE);
        }
        CHECK(live == ref.size());
        for (auto &kv : ref) {
            R r = {kv.first, 0};
            CHECK(OK == Get(&r, d) && r.val == kv.second);
        }
    }

    // op byte: low 3 bits the op, bit 7 a 1-byte key; args follow
    void run(u8 const *p, u8 const *e, u8 shift) {
        len = LINE << shift;
        reset(tab, len);
        ref.clear();
        relines();
        while (p < e) {
            u8 op = *p++;
            u8 code = op & 7;
            size_t need = code <= 5 ? 1 + !(op & 0x80) + (code <= 2) : 1;
            if (code == 7 && !(op & 8)) need = 0;
            if (size_t(e - p) < need) break;
            u16 k = 0;
            if (code <= 5) {
                k = *p++;
                if (!(op & 0x80)) k |= u16(*p++) << 8;
            }
            switch (code) {
                case 0:
                case 1: put(k, 1u + *p++); break;
                case 2: putgrow(k, 1u + *p++); break;
                case 3: del(k); break;
                case 4: get(k); break;
                case 5: at(k); break;
                case 6: erase(*p++, (op >> 3) & 3); break;
                default:
                    if (op & 8) {
                        size_t n = LINE << (*p++ % 6);
                        move(n < CAP ? n : CAP);
                    } else {
                        grow();
                    }
            }
        }
        compare();
    }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    static Lane<kv32, 64, false> plain;
    static Lane<sum32, 16, true> merge;
    if (Size < 1) return 0;
    u8 lane = Data[0];
    if (lane & 1)
        merge.run(Data + 1, Data + Size, (lane >> 1) & 3);
    else
        plain.run(Data + 1, Data + Size, (lane >> 1) & 3);
    return 0;
}
