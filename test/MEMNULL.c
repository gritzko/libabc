// STD-26B068VwHO: empty slices may be {NULL, NULL}; glibc declares the
// mem* functions nonnull, musl does not, so redeclare them glibc's way.
#include <string.h>
void *memcpy(void *, void const *, size_t) __attribute__((nonnull(1, 2)));
void *memmove(void *, void const *, size_t) __attribute__((nonnull(1, 2)));
int memcmp(void const *, void const *, size_t) __attribute__((nonnull(1, 2)));
void *memset(void *, int, size_t) __attribute__((nonnull(1)));

#include "S.h"
#include "TEST.h"

ok64 MEMNULLtest() {
    sane(1);
    u8cs nil = {NULL, NULL};
    u8s mut = {NULL, NULL};
    want(u8csEq(nil, nil));
    want(u8csHasPrefix(nil, nil));
    want(u8csHasSuffix(nil, nil));
    want(0 == $cmp(nil, nil));
    want($eq(nil, nil));
    u8sZero(mut);
    call(u8sCopy, mut, nil);
    a_pad(u8, pad, 8);
    call(u8bFeed, pad, nil);
    u8s into = {pad[2], pad[3]};
    a_cstr(tmpl, "a$s");
    call($feedf, into, tmpl, nil);
    want(into[0] == pad[2] + 1);
    done;
}

TEST(MEMNULLtest);
