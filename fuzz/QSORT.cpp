// abc/fuzz/QSORT.cpp — FUZZ-26B04IqDqU: QSORTx vs std::sort.
//
// The input is read three ways: u16 and u64 scalars (sSort, sDedup and
// sBinSearch against std::sort, std::unique, std::binary_search) and
// {u16 key, u16 payload} rows sorted by key only: the same keys as
// std::stable_sort, each key's payloads the same multiset, untouched; the
// stable InSort plus sDedup keeps each key's newest payload.

#include <algorithm>
#include <vector>

#include "INT.h"
#include "PRO.h"
#include "TEST.h"

typedef struct qk {
    u16 key;
    u16 pay;
} qk;
fun b8 qkZ(qk const *a, qk const *b) { return a->key < b->key; }

#define X(M, name) M##qk##name
#include "Bx.h"
#include "QSORTx.h"
#undef X

#define QS_MAX_INPUT 8192

template <typename T>
static std::vector<T> qs_read(u8cs input) {
    std::vector<T> v((size_t)$len(input) / sizeof(T));
    if (!v.empty()) memcpy(v.data(), input[0], v.size() * sizeof(T));
    return v;
}

static ok64 qs_u16(u8cs input) {
    sane(1);
    std::vector<u16> a = qs_read<u16>(input), ref = a;
    std::sort(ref.begin(), ref.end());
    u16s s = {a.data(), a.data() + a.size()};
    u16sSort(s);
    must(a == ref, "u16 sort");
    for (u32 k = 0; k < 64 && !a.empty(); k++) {
        u16 needle = (u16)(k * 1031);
        u16 *p = u16sBinSearch(&needle, s);
        b8 in = std::binary_search(ref.begin(), ref.end(), needle);
        must(in ? p && *p == needle : p == NULL, "u16 BinSearch");
    }
    ref.erase(std::unique(ref.begin(), ref.end()), ref.end());
    u16sDedup(s);
    must((size_t)(s[1] - s[0]) == ref.size() &&
             std::equal(ref.begin(), ref.end(), s[0]),
         "u16 dedup");
    done;
}

static ok64 qs_u64(u8cs input) {
    sane(1);
    std::vector<u64> a = qs_read<u64>(input), ref = a;
    std::sort(ref.begin(), ref.end());
    u64s s = {a.data(), a.data() + a.size()};
    u64sSort(s);
    must(a == ref, "u64 sort");
    ref.erase(std::unique(ref.begin(), ref.end()), ref.end());
    u64sDedup(s);
    must((size_t)(s[1] - s[0]) == ref.size() &&
             std::equal(ref.begin(), ref.end(), s[0]),
         "u64 dedup");
    done;
}

static b8 qs_bykey(qk const &a, qk const &b) { return a.key < b.key; }

static ok64 qs_kv(u8cs input) {
    sane(1);
    std::vector<qk> a = qs_read<qk>(input), ref = a;
    std::stable_sort(ref.begin(), ref.end(), qs_bykey);
    qks s = {a.data(), a.data() + a.size()};
    qksSort(s);
    for (size_t i = 0; i < a.size();) {
        size_t j = i;
        while (j < a.size() && ref[j].key == ref[i].key) j++;
        std::vector<u16> got, want;
        for (size_t k = i; k < j; k++) {
            must(a[k].key == ref[i].key, "kv key order");
            got.push_back(a[k].pay);
            want.push_back(ref[k].pay);
        }
        std::sort(got.begin(), got.end());
        std::sort(want.begin(), want.end());
        must(got == want, "kv payloads");
        i = j;
    }
    //  stable: InSort is the reference order (a prefix: it is quadratic);
    //  Dedup keeps the newest
    std::vector<qk> ins(a.size() > 256 ? 256 : a.size());
    if (!ins.empty()) memcpy(ins.data(), input[0], ins.size() * sizeof(qk));
    ref = ins;
    std::stable_sort(ref.begin(), ref.end(), qs_bykey);
    //  InSort(NULL, NULL) does NULL + 1 (QSORTx.h:25, FUZZ-26B04IqDqU)
    if (!ins.empty()) QSORTqkInSort(ins.data(), ins.data() + ins.size());
    for (size_t i = 0; i < ins.size(); i++)
        must(ins[i].key == ref[i].key && ins[i].pay == ref[i].pay,
             "InSort stable");
    qks d = {ins.data(), ins.data() + ins.size()};
    qksDedup(d);
    size_t w = 0;
    for (size_t i = 0; i < ref.size(); i++) {
        if (i + 1 < ref.size() && ref[i + 1].key == ref[i].key) continue;
        must(d[0] + w < d[1] && d[0][w].key == ref[i].key &&
                 d[0][w].pay == ref[i].pay,
             "kv dedup newest");
        w++;
    }
    must(d[0] + w == d[1], "kv dedup length");
    done;
}

FUZZ(u8, QSORTfuzz) {
    sane(1);
    if ($len(input) > QS_MAX_INPUT) input[1] = input[0] + QS_MAX_INPUT;
    call(qs_u16, input);
    call(qs_u64, input);
    call(qs_kv, input);
    done;
}
