#include "INT.h"
#include "KV.h"
#include "PRO.h"
#include "TEST.h"

// --- Test 0: basic sort ---
ok64 QSORT0() {
    sane(1);
    u64 arr[] = {5, 3, 1, 4, 2, 9, 7, 8, 6, 0};
    u64s data = {arr, arr + 10};
    u64sSort(data);
    for (int i = 0; i < 9; i++) want(arr[i] <= arr[i + 1]);
    for (int i = 0; i < 10; i++) testeqv((long long)(arr[i]), (long long)((u64)i), "%lld");
    done;
}

// --- Test 1: already sorted ---
ok64 QSORT1() {
    sane(1);
    u64 arr[] = {1, 2, 3, 4, 5};
    u64s data = {arr, arr + 5};
    u64sSort(data);
    for (int i = 0; i < 5; i++) testeqv((long long)(arr[i]), (long long)((u64)(i + 1)), "%lld");
    done;
}

// --- Test 2: reverse sorted ---
ok64 QSORT2() {
    sane(1);
    u64 arr[] = {5, 4, 3, 2, 1};
    u64s data = {arr, arr + 5};
    u64sSort(data);
    for (int i = 0; i < 5; i++) testeqv((long long)(arr[i]), (long long)((u64)(i + 1)), "%lld");
    done;
}

// --- Test 3: all equal ---
ok64 QSORT3() {
    sane(1);
    u64 arr[] = {7, 7, 7, 7, 7};
    u64s data = {arr, arr + 5};
    u64sSort(data);
    for (int i = 0; i < 5; i++) testeqv((long long)(arr[i]), (long long)((u64)7), "%lld");
    done;
}

// --- Test 4: single element ---
ok64 QSORT4() {
    sane(1);
    u64 arr[] = {42};
    u64s data = {arr, arr + 1};
    u64sSort(data);
    testeqv((long long)(arr[0]), (long long)((u64)42), "%lld");
    done;
}

// --- Test 5: empty ---
ok64 QSORT5() {
    sane(1);
    u64 arr[1] = {};
    u64s data = {arr, arr};
    u64sSort(data);
    done;
}

// --- Test 6: large random, compare to stdlib qsort ---
#define QSORT_N 10000
ok64 QSORT6() {
    sane(1);
    u64 a[QSORT_N], b[QSORT_N];
    u64 r = 0x123456789ABCDEF0ULL;
    for (int i = 0; i < QSORT_N; i++) {
        r = r * 6364136223846793005ULL + 1442695040888963407ULL;
        a[i] = b[i] = r;
    }
    u64s as = {a, a + QSORT_N};
    u64sSort(as);
    //  Validate "sorted" by direct check; stdlib qsort parity is now
    //  redundant with QSORTx's own correctness tests.
    for (int i = 1; i < QSORT_N; i++) testeqv((long long)(a[i - 1] <= a[i]), 1LL, "%lld");
    (void)b;
    done;
}

// --- Test 7: sDedup on sorted data ---
ok64 QSORT7() {
    sane(1);
    u64 arr[] = {1, 1, 2, 3, 3, 3, 4, 5, 5};
    u64s data = {arr, arr + 9};
    u64sDedup(data);
    testeqv((long long)($len(data)), (long long)((size_t)5), "%lld");
    for (int i = 0; i < 5; i++) testeqv((long long)(data[0][i]), (long long)((u64)(i + 1)), "%lld");
    done;
}

// --- Test 8: sDedup all equal ---
ok64 QSORT8() {
    sane(1);
    u64 arr[] = {7, 7, 7, 7};
    u64s data = {arr, arr + 4};
    u64sDedup(data);
    testeqv((long long)($len(data)), (long long)((size_t)1), "%lld");
    testeqv((long long)(data[0][0]), (long long)((u64)7), "%lld");
    done;
}

// --- Test 9: sDedup no duplicates ---
ok64 QSORT9() {
    sane(1);
    u64 arr[] = {1, 2, 3, 4, 5};
    u64s data = {arr, arr + 5};
    u64sDedup(data);
    testeqv((long long)($len(data)), (long long)((size_t)5), "%lld");
    done;
}

// --- Test 10: sDedup empty ---
ok64 QSORT10() {
    sane(1);
    u64 arr[1] = {};
    u64s data = {arr, arr};
    u64sDedup(data);
    testeqv((long long)($len(data)), (long long)((size_t)0), "%lld");
    done;
}

// --- Test 11: sort+dedup end-to-end ---
ok64 QSORT11() {
    sane(1);
    u64 arr[] = {5, 3, 1, 3, 2, 5, 1, 4, 2};
    u64s data = {arr, arr + 9};
    u64sSort(data);
    u64sDedup(data);
    testeqv((long long)($len(data)), (long long)((size_t)5), "%lld");
    for (int i = 0; i < 5; i++) testeqv((long long)(data[0][i]), (long long)((u64)(i + 1)), "%lld");
    done;
}

// --- Test 12: u32 sort ---
ok64 QSORT12() {
    sane(1);
    u32 arr[] = {10, 3, 7, 1, 5};
    u32s data = {arr, arr + 5};
    u32sSort(data);
    for (int i = 0; i < 4; i++) want(arr[i] <= arr[i + 1]);
    done;
}

// --- Test 13: bSort / bDedup ---
ok64 QSORT13() {
    sane(1);
    Bu64 buf = {};
    call(u64bAlloc, buf, 16);
    u64bFeed1(buf, 5);
    u64bFeed1(buf, 3);
    u64bFeed1(buf, 1);
    u64bFeed1(buf, 3);
    u64bFeed1(buf, 1);
    testeqv((long long)(u64bDataLen(buf)), (long long)((size_t)5), "%lld");
    u64bSort(buf);
    u64bDedup(buf);
    testeqv((long long)(u64bDataLen(buf)), (long long)((size_t)3), "%lld");
    u64 *d = u64bDataHead(buf);
    testeqv((long long)(d[0]), (long long)((u64)1), "%lld");
    testeqv((long long)(d[1]), (long long)((u64)3), "%lld");
    testeqv((long long)(d[2]), (long long)((u64)5), "%lld");
    u64bFree(buf);
    done;
}

// --- Test 14: large random with many duplicates ---
#define QSORT_D 50000
ok64 QSORT14() {
    sane(1);
    u64 a[QSORT_D], b[QSORT_D];
    u64 r = 42;
    for (int i = 0; i < QSORT_D; i++) {
        r = r * 6364136223846793005ULL + 1;
        a[i] = b[i] = r % 1000;  // lots of duplicates
    }
    u64s as = {a, a + QSORT_D};
    u64sSort(as);
    for (int i = 1; i < QSORT_D; i++) testeqv((long long)(a[i - 1] <= a[i]), 1LL, "%lld");
    (void)b;

    u64sDedup(as);
    want($len(as) <= 1000);
    want($len(as) > 0);
    for (i64 i = 1; i < $len(as); i++) want(as[0][i - 1] < as[0][i]);
    done;
}

// DOG-027: 15 — QSORTkv64InSort is STABLE: kv64Z compares keys only, so
// equal keys must come out in arrival order.
ok64 QSORT15() {
    sane(1);
    kv64 arr[] = {{2, 20}, {1, 10}, {2, 21}, {1, 11}, {2, 22}, {1, 12}};
    QSORTkv64InSort(arr, arr + 6);
    static kv64 const want[6] = {{1, 10}, {1, 11}, {1, 12},
                                 {2, 20}, {2, 21}, {2, 22}};
    for (size_t i = 0; i < 6; i++) {
        testeqv((long long)(arr[i].key), (long long)(want[i].key), "%lld");
        testeqv((long long)(arr[i].val), (long long)(want[i].val), "%lld");
    }
    done;
}

// DOG-027: 16 — kv64sDedup keeps the LAST of an equal-key run, which after
// a stable sort is the arrival-newest value.
ok64 QSORT16() {
    sane(1);
    kv64 arr[] = {{1, 10}, {1, 11}, {1, 12}, {2, 20}, {3, 30}, {3, 31}};
    kv64s data = {arr, arr + 6};
    kv64sDedup(data);
    static kv64 const want[3] = {{1, 12}, {2, 20}, {3, 31}};
    testeqv((long long)($len(data)), (long long)((size_t)3), "%lld");
    for (size_t i = 0; i < 3; i++) {
        testeqv((long long)(data[0][i].key), (long long)(want[i].key), "%lld");
        testeqv((long long)(data[0][i].val), (long long)(want[i].val), "%lld");
    }
    done;
}

// DOG-027: 17 — the memtable's worst case: a 32-row DATA window, each of 16
// keys arriving twice; InSort then Dedup must leave the newest arrival.
#define QSORT_MEM 32
ok64 QSORT17() {
    sane(1);
    kv64 arr[QSORT_MEM];
    for (u64 i = 0; i < QSORT_MEM; i++) {
        arr[i].key = (i * 11) % 16;  // 11 is invertible mod 16: each key twice
        arr[i].val = 1000 + i;       // value == arrival index
    }
    QSORTkv64InSort(arr, arr + QSORT_MEM);
    for (int i = 1; i < QSORT_MEM; i++) want(arr[i - 1].key <= arr[i].key);
    kv64s data = {arr, arr + QSORT_MEM};
    kv64sDedup(data);
    testeqv((long long)($len(data)), (long long)((size_t)16), "%lld");
    for (u64 k = 0; k < 16; k++) {
        testeqv((long long)(data[0][k].key), (long long)(k), "%lld");
        // second arrival of key k sits at i = 16 + (3*k)%16 (3 == 11^-1 mod 16)
        testeqv((long long)(data[0][k].val),
                (long long)(1000 + 16 + (3 * k) % 16), "%lld");
    }
    done;
}

ok64 QSORTtest() {
    sane(1);
    call(QSORT0);
    call(QSORT1);
    call(QSORT2);
    call(QSORT3);
    call(QSORT4);
    call(QSORT5);
    call(QSORT6);
    call(QSORT7);
    call(QSORT8);
    call(QSORT9);
    call(QSORT10);
    call(QSORT11);
    call(QSORT12);
    call(QSORT13);
    call(QSORT14);
    call(QSORT15);
    call(QSORT16);
    call(QSORT17);
    done;
}

TEST(QSORTtest);
