//  SWISS-26B05A2Jxv: SWISSx.h diffed against std::unordered_map, as
//  fuzz/HASH.cpp.  A byte program of Put/Probe/PutGrow/Del/Get/At/erase-
//  while-iterating/Grow/Move over u16 keys (0 included).  Per op only the
//  return code is checked (and a failed write leaves the bytes as they
//  were); table and model are compared in full once, at the end, the
//  header's room count and every tag included.  Lanes: kv64 (16 B);
//  sum128 (32 B) with ABC_SWISS_MERGE.  Checks trap, NDEBUG or not.
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

#include "INT.h"
#include "KV.h"
#include "PRO.h"

uint8_t _pro_depth = 0;
thread_local u8 *ABC_BASS[4] = {};

#define X(M, name) M##kv64##name
#include "SWISSx.h"
#undef X

typedef struct sum128 {
    u32 key;
    u32 val;
    u64 pad[3];
} sum128;
fun u64 sum128Hash(sum128 const *v) { return mix32(v->key); }
fun b8 sum128Z(sum128 const *a, sum128 const *b) { return a->key < b->key; }
fun b8 sum128KeyEq(sum128 const *a, sum128 const *b) { return a->key == b->key; }
fun void sum128Merge(sum128 *acc, sum128 const *nxt) { acc->val += nxt->val; }
#define X(M, name) M##sum128##name
#include "Bx.h"
#define ABC_SWISS_MERGE
#include "SWISSx.h"
#undef X

#define CHECK(c)                       \
    do {                               \
        if (!(c)) __builtin_trap();    \
    } while (0)

// Overloads route the Lane template to each instantiation.
#define OPS(R)                                                                \
    fun u64 Hash(R const *r) { return R##Hash(r); }                           \
    fun ok64 Init(R **b, size_t n) { return SWISS##R##Init(b, n); }           \
    fun ok64 Put(R **b, R const *r) { return SWISS##R##Put(b, r); }           \
    fun ok64 PutGrow(R **b, R const *r) { return SWISS##R##PutGrow(b, r); }   \
    fun ok64 Probe(R **s, R **b, R const *r) { return SWISS##R##Probe(s, b, r); } \
    fun ok64 Unprobe(R **b, R *s) { return SWISS##R##Unprobe(b, s); }         \
    fun ok64 Get(R *r, R **b) { return SWISS##R##Get(r, b); }                 \
    fun ok64 At(R const **s, R **b, R const *r) { return SWISS##R##At(s, b, r); } \
    fun ok64 Del(R **b, R const *r) { return SWISS##R##Del(b, r); }           \
    fun ok64 DelAt(R **b, size_t i) { return SWISS##R##DelAt(b, i); }         \
    fun ok64 Next(size_t *i, R **b) { return SWISS##R##Next(i, b); }          \
    fun ok64 Grow(R **b) { return SWISS##R##Grow(b); }                        \
    fun ok64 Move(R **into, R **from) { return SWISS##R##Move(into, from); }  \
    fun size_t Max(R **b) { return SWISS##R##_max(b); }                       \
    fun ok64 Map(R **b, size_t n) { return R##bMap(b, n); }
OPS(kv64)
OPS(sum128)

template <class R, bool MERGE>
struct Lane {
    static constexpr size_t CAP = 2048;  // records mapped per buffer
    R *tab[4] = {}, *aux[4] = {};
    size_t len = 0;
    std::unordered_map<u16, u32> ref;
    std::vector<u8> snap;

    Lane() { CHECK(OK == Map(tab, CAP) && OK == Map(aux, CAP)); }
    static R mk(u32 k, u32 v) {
        R r{};
        r.key = k, r.val = v;
        return r;
    }
    static u8 *ctl(R **b) { return (u8 *)b[0] + SWISS_HEAD; }
    static u64 left(R **b) { return *(u64 *)b[0]; }
    static size_t live(R **b) {
        size_t n = 0;
        for (size_t i = 0; OK == Next(&i, b); ++i) ++n;
        return n;
    }
    void save() { snap.assign((u8 *)tab[0], (u8 *)tab[2]); }
    bool same() {
        return snap.size() == size_t((u8 *)tab[2] - (u8 *)tab[0]) &&
               0 == memcmp(snap.data(), (void *)tab[0], snap.size());
    }
    void model_put(u16 k, u32 v) {
        auto it = ref.find(k);
        if (it == ref.end()) ref[k] = v;
        else it->second = MERGE ? it->second + v : v;
    }
    // a claim may FULL only for an absent key with no room left: snapshot then
    bool maybe_full(u16 k) { return !ref.count(k) && left(tab) == 0; }

    void put(u16 k, u32 v) {
        R r = mk(k, v);
        if (maybe_full(k)) save();
        ok64 o = Put(tab, &r);
        CHECK(o == OK || (o == FULL && !ref.count(k) && left(tab) == 0));
        if (o == FULL) {
            CHECK(same());
            return;
        }
        model_put(k, v);
    }
    void probe(u16 k, u32 v) {
        R r = mk(k, v);
        R *s = NULL;
        if (maybe_full(k)) save();
        ok64 o = Probe(&s, tab, &r);
        if (ref.count(k)) {
            CHECK(o == OK && s->key == k && s->val == ref[k]);
            return;
        }
        CHECK(o == NONE || (o == FULL && left(tab) == 0));
        if (o == FULL) {
            CHECK(same());
            return;
        }
        CHECK(s->key == 0 && s->val == 0);
        if (v & 1) {
            *s = r;
            ref[k] = v;
        } else {
            CHECK(OK == Unprobe(tab, s));
        }
    }
    void putgrow(u16 k, u32 v) {
        R r = mk(k, v);
        ok64 o = PutGrow(tab, &r);
        size_t n = tab[2] - tab[1];
        CHECK(o == OK || (o == NOROOM && !ref.count(k) && 2 * n > Max(tab)));
        CHECK(n >= len && 0 == (n & (n - 1)));
        len = n;
        if (o == OK) model_put(k, v);
    }
    void del(u16 k) {
        R r = mk(k, 0);
        CHECK(Del(tab, &r) == (ref.erase(k) ? OK : NONE));
    }
    void get(u16 k) {
        R r = mk(k, 0);
        auto it = ref.find(k);
        ok64 o = Get(&r, tab);
        CHECK(o == (it == ref.end() ? NONE : OK));
        if (o == OK) CHECK(r.key == k && r.val == it->second);
    }
    void at(u16 k) {
        R r = mk(k, 0);
        R const *s = NULL;
        auto it = ref.find(k);
        ok64 o = At(&s, tab, &r);
        CHECK(o == (it == ref.end() ? NONE : OK));
        if (o == OK) CHECK(s->key == k && s->val == it->second);
    }
    // SWISS.md: nothing moves on delete, every matching key goes in one pass
    void erase(u8 m, u8 b) {
        u32 mask = (1u << b) - 1;
        size_t want = 0, got = 0;
        for (auto it = ref.begin(); it != ref.end();) {
            if ((it->first ^ m) & mask) ++it;
            else it = ref.erase(it), ++want;
        }
        for (size_t i = 0; OK == Next(&i, tab); ++i) {
            if ((tab[1][i].key ^ m) & mask) continue;
            CHECK(NONE == DelAt(tab, i));
            ++got;
        }
        CHECK(got == want);
    }
    void grow() {
        ok64 w = 2 * len <= Max(tab) ? OK : NOROOM;
        if (w != OK) save();
        CHECK(w == Grow(tab));
        if (w != OK) {
            CHECK(same());
            return;
        }
        len *= 2;
    }
    // Move into an empty table of n slots: FULL once n - n/8 are in
    void move(size_t n) {
        CHECK(OK == Init(aux, n));
        size_t fit = n - n / 8;
        ok64 w = ref.size() > fit ? FULL : OK;
        save();
        CHECK(w == Move(aux, tab) && same());
        if (w == OK) {
            std::swap(tab, aux);
            len = n;
            return;
        }
        CHECK(live(aux) == fit);
        for (size_t j = 0; OK == Next(&j, aux); ++j) {
            R r = mk(aux[1][j].key, 0);
            CHECK(OK == Get(&r, tab) && r.val == aux[1][j].val);
        }
    }
    // the one full comparison: live slots are ref's, tagged by their hash;
    // the room left is 7/8 of the slots less the live and the tombs
    void compare() {
        CHECK(len == size_t(tab[2] - tab[1]));
        size_t n = 0, tombs = 0;
        for (size_t i = 0; i < len; ++i) {
            u8 c = ctl(tab)[i];
            if (c == SWISS_TOMB) ++tombs;
            if (!(c & SWISS_LIVE)) {
                CHECK(c == SWISS_TOMB || c == SWISS_EMPTY);
                continue;
            }
            R *s = tab[1] + i;
            auto it = ref.find(s->key);
            CHECK(s->key <= 0xffff && it != ref.end() && it->second == s->val);
            CHECK(c == (SWISS_LIVE | (Hash(s) & 0x7f)));
            ++n;
        }
        CHECK(n == ref.size() && live(tab) == n);
        CHECK(left(tab) == len - len / 8 - n - tombs);
        for (auto &kv : ref) {
            R r = mk(kv.first, 0);
            CHECK(OK == Get(&r, tab) && r.val == kv.second);
        }
    }

    // op byte: low 3 bits the op, bit 7 a 1-byte key; args follow
    void run(u8 const *p, u8 const *e, u8 shift) {
        len = SWISS_GROUP << shift;
        CHECK(OK == Init(tab, len));
        ref.clear();
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
                case 0: put(k, 1u + *p++); break;
                case 1: probe(k, 1u + *p++); break;
                case 2: putgrow(k, 1u + *p++); break;
                case 3: del(k); break;
                case 4: get(k); break;
                case 5: at(k); break;
                case 6: erase(*p++, (op >> 3) & 3); break;
                default:
                    if (op & 8) move(SWISS_GROUP << (*p++ % 7));
                    else grow();
            }
        }
        compare();
    }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    static Lane<kv64, false> plain;
    static Lane<sum128, true> merge;
    if (Size < 1) return 0;
    u8 lane = Data[0];
    if (lane & 1)
        merge.run(Data + 1, Data + Size, (lane >> 1) & 3);
    else
        plain.run(Data + 1, Data + Size, (lane >> 1) & 3);
    return 0;
}
