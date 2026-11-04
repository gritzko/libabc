// abc/fuzz/SORT.cpp — FUZZ-26B04IqDqU: SORTu64 and the Yx merge vs std::sort.
//
// The u64 lane: SORTu64 (SORT.c, Y-merge passes) equals std::sort.  The kv
// lane: u64 rows read as {key: the top 16 bits, payload: the rest}, cut into
// chunks, each stable-sorted by key; the Yx merge with a key-only z must give
// key order and keep every row (ties are fed through, none lost or doubled).

#include <algorithm>
#include <vector>

#include "INT.h"
extern "C" {  //  SORT.c is C
#include "SORT.h"
}
#include "PRO.h"
#include "B.h"
#include "TEST.h"

#define SO_MAX_INPUT 1024

static u16 so_key(u64 v) { return (u16)(v >> 48); }

fun ok64 SOkvx(u8cs s, u8cs rest) { return $u8take(s, rest, sizeof(u64)); }

fun ok64 SOkvy($u8 into, u8css eqs) {
    ok64 o = OK;
    for (u8cs *e = eqs[0]; o == OK && e != eqs[1]; ++e)
        o = u8sFeed(into, (u8c$c)e);
    return o;
}

fun b8 SOkvz($cu8c *a, $cu8c *b) {
    u64 x, y;
    memcpy(&x, **a, sizeof(u64));
    memcpy(&y, **b, sizeof(u64));
    return so_key(x) < so_key(y);
}

#define _$u8cempty SOkv_u8cempty  //  Yx.h defines it once per TU
#define _X(name) SOkv##name
#include "Yx.h"
#undef _X
#undef _$u8cempty

static ok64 so_u64(std::vector<u64> in) {
    sane(1);
    if (in.empty()) done;  //  a NULL empty slice is BADARG (FUZZ-26B04IqDqU)
    std::vector<u64> ref = in, out(in.size());
    std::sort(ref.begin(), ref.end());
    u64 *f[2] = {in.data(), in.data() + in.size()};
    u64 *t[2] = {out.data(), out.data() + out.size()};
    u64 *t0 = t[0];
    ok64 o = SORTu64(t, f);
    if (o != OK) fprintf(stderr, "SORTu64 %llx n=%zu\n", (unsigned long long)o, in.size());
    must(o == OK && t[0] == t0 + ref.size() && std::equal(ref.begin(), ref.end(), t0),
         "SORTu64");
    done;
}

static ok64 so_kv(std::vector<u64> in, size_t clen) {
    sane(1);
    if (in.empty()) done;
    auto bykey = [](u64 a, u64 b) { return so_key(a) < so_key(b); };
    struct chunk {
        u8c *lo, *hi;
    };
    std::vector<chunk> chunks;
    for (size_t i = 0; i < in.size() && chunks.size() < Y_MAX_INPUTS;
         i += clen) {
        size_t j = i + clen < in.size() ? i + clen : in.size();
        if (chunks.size() + 1 == Y_MAX_INPUTS) j = in.size();
        std::stable_sort(in.begin() + i, in.begin() + j, bykey);
        chunks.push_back({(u8c *)(in.data() + i), (u8c *)(in.data() + j)});
    }
    std::vector<u64> out(in.size() + 1);
    u8 *o[2] = {(u8 *)out.data(), (u8 *)(out.data() + out.size())};
    u8 *o0 = o[0];
    u8cs *lsm[2] = {(u8cs *)chunks.data(),
                    (u8cs *)(chunks.data() + chunks.size())};
    call(SOkvmerge, o, lsm);
    must(o[0] == o0 + in.size() * sizeof(u64), "Y merge length");
    out.resize(in.size());
    must(std::is_sorted(out.begin(), out.end(), bykey), "Y merge key order");
    std::sort(out.begin(), out.end());
    std::sort(in.begin(), in.end());
    must(out == in, "Y merge rows");
    done;
}

FUZZ(u8, SORTfuzz) {
    sane(1);
    if ($len(input) < 1) done;
    size_t clen = 1 + input[0][0] % 16;
    u8cs rest = {input[0] + 1, input[1]};
    size_t n = (size_t)$len(rest) / 8;
    std::vector<u64> v(n < SO_MAX_INPUT ? n : SO_MAX_INPUT);
    if (!v.empty()) memcpy(v.data(), rest[0], v.size() * sizeof(u64));
    call(so_u64, v);
    call(so_kv, v, clen);
    done;
}
