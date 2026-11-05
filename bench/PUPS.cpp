//  abc/bench/PUPS.cpp — BENCH-26B0559PCK: PUPS (PUPb.c) vs std::set, a sorted
//  vector and absl::btree_set past the cache: 2^20..2^28 rows by 4x, up to
//  1 GB of rows; runs are files in $PUPBENCH_DIR (default /dev/shm, tmpfs).
//  pups commits after the build; a baseline estimated over 3 GB is left out.
#include "BOXb.hpp"

template <class T>
using Pup = Lsm<T, PP, true>;

template <class T>
static void reg_all(char const *rec) {
    for (size_t lg = 20; lg <= 28; lg += 2) {
        size_t n = (size_t)1 << lg;
        if (n * sizeof(T) > (1 << 30)) break;
        std::vector<Sub> s = {sub<Pup, T>("pups")};
        for (Sub const &b : baselines<T>()) s.push_back(b);
        reg_cfg(rec, n, s, 3ULL << 30);
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
