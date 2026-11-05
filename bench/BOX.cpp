//  abc/bench/BOX.cpp — BENCH-26B0559PCK: BOX (BOXb.c) vs std::set, a sorted
//  vector and absl::btree_set over 4..64-byte records at in-cache sizes only:
//  N <= 2^20 rows and N*sizeof <= 4 MB (a quarter of the 7840U's 16 MB L3).
//  box is BOX as the puts leave it, box1 the same after Sort(all).
#include "BOXb.hpp"

template <class T>
using Box = Lsm<T, BB, false>;
template <class T>
using Box1 = Lsm<T, BB, true>;

template <class T>
static void reg_all(char const *rec) {
    for (size_t lg : {10, 14, 16, 18, 20}) {
        size_t n = (size_t)1 << lg;
        if (n * sizeof(T) > (4 << 20)) break;
        std::vector<Sub> s = {sub<Box, T>("box"), sub<Box1, T>("box1")};
        for (Sub const &b : baselines<T>()) s.push_back(b);
        reg_cfg(rec, n, s, SIZE_MAX);
    }
}

int main(int argc, char **argv) {
    reg_all<e4>("e4");
    reg_all<e8>("e8");
    reg_all<e16>("e16");
    reg_all<e32>("e32");
    reg_all<e64>("e64");
    return bench_main(argc, argv);
}
