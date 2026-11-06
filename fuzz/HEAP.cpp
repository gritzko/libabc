// abc/fuzz/HEAP.cpp — FUZZ-26B04IqDqU: HEAPx over {u16 key, u16 val} vs
// std::multiset.  A byte program of Push, Push1, Pop, EjectAtZ, sTops and a
// scramble + sHeap on a small buffer (Push fills it: SNOROOM, unchanged), run
// on a min-heap (the default Z) and a max-heap (PushZ/PopZ by a reverse z).
// Pops must give the extreme key with a (key, val) the model holds; the key
// view never reads val.  At the end both drain, and the sum lane folds equal
// keys by adding vals, against a std::map of sums.

#include <map>
#include <set>
#include <vector>

#include "INT.h"
#include "PRO.h"
#include "TEST.h"

typedef struct hp {
    u16 key;
    u16 val;
} hp;
fun b8 hpZ(hp const *a, hp const *b) { return a->key < b->key; }
fun b8 hpRZ(hp const *a, hp const *b) { return a->key > b->key; }

#define X(M, name) M##hp##name
#include "Bx.h"
#include "HEAPx.h"
#undef X

#define HP_MAX_INPUT 4096
#define HP_MAX_LEN 256

typedef std::multiset<std::pair<u16, u16>> hpmodel;

static u8 hp_byte(u8cs in) { return $empty(in) ? 0 : *in[0]++; }

//  One heap lane: the buffer, its model, the order (rev: a max-heap).
struct hplane {
    std::vector<hp> mem;
    hp *b[4];
    hpmodel m;
    b8 rev;
    hp *const *data() { return b + 1; }
    size_t len() { return (size_t)(b[2] - b[1]); }
    u16 top() { return rev ? m.rbegin()->first : m.begin()->first; }
};

static void hp_init(hplane &l, size_t cap, b8 rev) {
    l.mem.assign(cap + 1, hp{});
    l.b[0] = l.b[1] = l.b[2] = l.mem.data();
    l.b[3] = l.mem.data() + cap;
    l.m.clear();
    l.rev = rev;
}

static ok64 hp_push(hplane &l, hp r, b8 one) {
    sane(1);
    std::vector<hp> snap(l.b[1], l.b[2]);
    b8 room = l.b[2] < l.b[3];
    ok64 o = l.rev ? (one ? HEAPhpPush1Z(l.b, r, hpRZ)
                          : HEAPhpPushZ(l.b, &r, hpRZ))
                   : (one ? HEAPhpPush1(l.b, r) : HEAPhpPush(l.b, &r));
    if (room) {
        must(o == OK, "Push");
        l.m.insert({r.key, r.val});
    } else {
        must(o == SNOROOM && l.len() == snap.size() &&
                 std::equal(snap.begin(), snap.end(), l.b[1],
                            [](hp const &a, hp const &b) {
                                return a.key == b.key && a.val == b.val;
                            }),
             "full Push changed the heap");
    }
    done;
}

static ok64 hp_pop(hplane &l, hp *out) {
    sane(1);
    b8 empty = l.m.empty();
    hp v = {};
    ok64 o = l.rev ? HEAPhpPopZ(&v, l.b, hpRZ) : HEAPhpPop(&v, l.b);
    *out = v;
    if (empty) {
        must(o == HEAPNONE && l.len() == 0, "Pop empty");
        done;
    }
    must(o == OK && v.key == l.top(), "Pop key");
    auto it = l.m.find({v.key, v.val});
    must(it != l.m.end(), "Pop val");
    l.m.erase(it);
    must(l.len() == l.m.size(), "Pop len");
    done;
}

FUZZ(u8, HEAPfuzz) {
    sane(1);
    if ($len(input) > HP_MAX_INPUT) input[1] = input[0] + HP_MAX_INPUT;
    a_dup(u8c, in, input);
    size_t cap = 1 + hp_byte(in) % HP_MAX_LEN;
    hplane lanes[2];
    hp_init(lanes[0], cap, NO);
    hp_init(lanes[1], cap, YES);
    while (!$empty(in)) {
        u8 op = hp_byte(in);
        u8 a = hp_byte(in), b = hp_byte(in);
        for (hplane &l : lanes) {
            hp *d[2] = {l.b[1], l.b[2]};
            switch (op & 7) {
                case 0 ... 2:
                    call(hp_push, l, hp{(u16)(a % 64), b}, (op & 7) == 2);
                    break;
                case 3: {
                    hp v;
                    call(hp_pop, l, &v);
                } break;
                case 4: {
                    if (l.m.empty()) break;
                    size_t at = a % l.len();
                    hp v = d[0][at];
                    auto it = l.m.find({v.key, v.val});
                    must(it != l.m.end(), "EjectAt row");
                    must(HEAPhpEjectAtZ(l.b, at, l.rev ? hpRZ : hpZ) == OK,
                         "EjectAt");
                    l.m.erase(it);
                } break;
                case 5: {
                    hp *eqs[2] = {};
                    ok64 o = hpsTopsZ(d, eqs, l.rev ? hpRZ : hpZ);
                    if (l.m.empty()) {
                        must(o == HEAPNONE, "Tops empty");
                        break;
                    }
                    u16 t = l.top();
                    must(o == OK && eqs[0] == d[0] &&
                             (size_t)(eqs[1] - eqs[0]) ==
                                 (size_t)std::distance(
                                     l.m.lower_bound({t, 0}),
                                     l.m.upper_bound({t, 0xffff})),
                         "Tops count");
                    for (hp *p = eqs[0]; p < eqs[1]; p++)
                        must(p->key == t, "Tops key");
                } break;
                default: {  //  scramble two rows, then sHeap restores order
                    size_t n = l.len();
                    if (n < 2) break;
                    std::swap(d[0][a % n], d[0][b % n]);
                    must(hpsHeapZ(d, l.rev ? hpRZ : hpZ) == OK, "sHeap");
                } break;
            }
            must(l.len() == l.m.size(), "length");
        }
    }
    //  drain both; the sum lane folds equal keys as ABC_X_MERGE would
    for (hplane &l : lanes) {
        std::map<u16, u32> want;
        for (auto const &kv : l.m) want[kv.first] += kv.second;
        std::map<u16, u32> got;
        hp v;
        while (!l.m.empty()) {
            call(hp_pop, l, &v);
            got[v.key] += v.val;
        }
        must(got == want, "sum lane");
    }
    done;
}
