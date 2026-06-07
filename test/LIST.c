//
// Created by gritzko on 5/11/24.
//
#include "LIST.h"

#include <assert.h>
#include <stdio.h>

#include "INT.h"
#include "PRO.h"
#include "TEST.h"

typedef struct {
    ok64 value;
    list64 _list;
} entry128;

fun b8 entry128Z(entry128 const *a, entry128 const *b) {
    return a->value < b->value;
}

#define X(M, name) M##entry128##name
#include "LISTx.h"
#undef X

ok64 LISTtest1() {
    sane(1);
    aBpad(entry128, list, 1024);
    entry128 codes[3] = {
        {.value = LISTNOROOM}, {.value = LISTNODATA}, {.value = LISTBADNDX}};
    call(LISTentry128insert, list, codes + 0, 0);
    call(LISTentry128insert, list, codes + 2, 0);
    call(LISTentry128insert, list, codes + 1, 0);
    u32 i = 0;
    testeqv((long long)(LISTentry128atp(list, i)->value), (long long)(codes[0].value), "%lld");
    i = LISTentry128next(list, i);
    testeqv((long long)(LISTentry128atp(list, i)->value), (long long)(codes[1].value), "%lld");
    i = LISTentry128next(list, i);
    testeqv((long long)(LISTentry128atp(list, i)->value), (long long)(codes[2].value), "%lld");
    done;
}

// MEM-014: LISTinsert must validate the successor index taken from
// prev->_list.next before writing list[next]._list.prev = len.  A
// corrupt/out-of-range .next link (e.g. uninitialized memory or a
// caller-controlled entry) otherwise produces an out-of-bounds write
// past the buffer in release builds (bAtP only asserts in debug).
ok64 LISTtest_badnext() {
    sane(1);
    aBpad(entry128, list, 1024);
    entry128 e0 = {.value = LISTNOROOM};
    entry128 e1 = {.value = LISTNODATA};
    call(LISTentry128insert, list, &e0, 0);
    call(LISTentry128insert, list, &e1, 0);
    // corrupt the successor link of node 0 to a wildly out-of-range
    // index; a subsequent insert after node 0 would dereference it.
    LISTentry128atp(list, 0)->_list.next = 1u << 20;
    ok64 o = LISTentry128insert(list, &e1, 0);
    if (o != LISTBADNDX) fail(TESTFAILEQ);
    done;
}

ok64 LISTtest() {
    sane(1);
    call(LISTtest1);
    call(LISTtest_badnext);
    done;
}

TEST(LISTtest);
