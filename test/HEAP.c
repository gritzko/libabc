#include "INT.h"
#include "PRO.h"
#include "TEST.h"

// Instantiate the HEAP template for u32
#define X(M, name) M##u32##name
#include "HEAPx.h"
#undef X

ok64 HEAPtest1() {
    sane(1);
    // Make a buffer on the stack
    aBpad(u32, pad, 32);
    // Pushes one entry into the heap buffer
    call(HEAPu32Push1, pad, 3);
    call(HEAPu32Push1, pad, 2);
    call(HEAPu32Push1, pad, 1);
    u32 one, two, three;
    // Retrieves the least entry.
    // May also use **pad to read one.
    call(HEAPu32Pop, &one, pad);
    call(HEAPu32Pop, &two, pad);
    call(HEAPu32Pop, &three, pad);
    testeqv((long long)(one), (long long)(1), "%lld");
    testeqv((long long)(two), (long long)(2), "%lld");
    testeqv((long long)(three), (long long)(3), "%lld");
    done;
}
// ABC-015: sTopsZ fabricated a 1-element slice over an empty heap (OOB
// for consumers); it must MISS instead, like its twin MSETTopZ.
ok64 HEAPtest2() {
    sane(1);
    u32 none[1];
    u32sc empty = {none, none};
    u32s eqs = {NULL, NULL};
    testeqv((long long)(u32sTopsZ(empty, eqs, u32Z)), (long long)(MISS),
            "%lld");
    // normal case: run of equal minimums lands at the front
    u32 arr[] = {1, 1, 2, 1, 3, 4};
    u32sc heap = {arr, arr + 6};
    call(u32sHeapZ, heap, u32Z);
    call(u32sTopsZ, heap, eqs, u32Z);
    testeqv((long long)($len(eqs)), (long long)((size_t)3), "%lld");
    for (int i = 0; i < 3; i++) testeqv((long long)(arr[i]), (long long)(1), "%lld");
    done;
}

// ABC-015: sHeap dropped its comparator argument, silently heapifying
// with default Z; now it is the argless default-Z twin of sHeapZ.
fun b8 u32Zdesc(u32c *a, u32c *b) { return *a > *b; }

ok64 HEAPtest3() {
    sane(1);
    u32 arr[] = {3, 1, 4, 1, 5, 9, 2, 6};
    u32sc heap = {arr, arr + 8};
    call(u32sHeapZ, heap, u32Zdesc);
    testeqv((long long)(arr[0]), (long long)(9), "%lld");
    call(u32sHeap, heap);
    testeqv((long long)(arr[0]), (long long)(1), "%lld");
    done;
}

ok64 HEAPtest() {
    sane(1);
    call(HEAPtest1);
    call(HEAPtest2);
    call(HEAPtest3);
    done;
}

TEST(HEAPtest)
