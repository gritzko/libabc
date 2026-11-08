//  PUPS-26A24FhuO9: an empty run.  A Create of no rows books a run with no
//  rows that Open maps back, every reader sees empty, and a ladder
//  merges it away; a 0-byte file (a crashed non-durable create) still fails
//  the open with PUPEMPTY.
#include "PUPS.h"

#include <stdio.h>
#include <string.h>

#include "FILE.h"
#include "PRO.h"
#include "TEST.h"

#include "TESTBE.h"

#define X(M, name) M##kv64##name
#include "PUPx.h"
#undef X

static ok64 PUPNilRun(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    a_cstr(ext, ".nil.idx");
    kv64cs none = {};
    a_pup(kv64, h);
    call(PUPkv64Create, h, h_keys, $path(dir), ext, none, 0, YES, NULL);
    call(PUPkv64Create, h, h_keys, $path(dir), ext, none, 5, NO, NULL);
    want(Bpastlen(h) == 2);
    kv64cs rows = {}, toc = {};
    PUPkv64Split(rows, toc, h[0][0]);
    want(rows[0] != NULL && $empty(rows) && $empty(toc));
    PUPkv64Close(h, h_keys);

    //  reopened: two empty runs, by every reader
    call(PUPkv64Open, h, h_keys, $path(dir), ext, u64max);
    want(Bpastlen(h) == 2);
    a_pad(kv64cs, all, 4);
    call(PUPkv64Runs, all, h);
    want($len(all_data) == 2);
    $for(kv64cs, r, all_data) want($empty(*r));
    PUPkv64Split(rows, toc, h[0][1]);
    want($empty(toc));

    //  rows on top: the empty runs merge in as nothing
    kv64 two[2] = {{1, 10}, {2, 20}};
    kv64cs rs = {two, two + 2};
    call(PUPkv64Create, h, h_keys, $path(dir), ext, rs, 0, YES, NULL);
    call(PUPkv64Ladder, h, h_keys, $path(dir), ext, NULL, NULL);
    kv64 got[4];
    kv64s out = {got, got + 4};
    call(PUPkv64Read, out, h);
    want(out[0] - got == 2 && got[0].key == 1 && got[1].val == 20);
    PUPkv64Close(h, h_keys);

    //  a 0-byte run still fails the open
    char path[512];
    snprintf(path, sizeof path, "%s/0000000009.nil.idx", tmp);
    FILE *f = fopen(path, "w");
    want(f != NULL);
    fclose(f);
    want(PUPkv64Open(h, h_keys, $path(dir), ext, u64max) == PUPEMPTY);
    PUPkv64Close(h, h_keys);
    TESTBErmrf(tmp);
    done;
}

//  PUPS-26B08Iot53: a mutated nil run, 7 plain bytes under a 16-byte row,
//  is not a whole run: the open fails clean, the file stays, nothing clipped.
static ok64 PUPNilBadRun(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    a_cstr(ext, ".nil.idx");
    char path[512];
    snprintf(path, sizeof path, "%s/0000000007.nil.idx", tmp);
    FILE *f = fopen(path, "w");
    want(f != NULL);
    want(fwrite("A\0\0\0\0TC", 1, 7, f) == 7);
    fclose(f);
    a_pup(kv64, h);
    want(PUPkv64Open(h, h_keys, $path(dir), ext, u64max) == PUPBADRUN);
    want(Bpastlen(h) == 0 && u64bDataLen(h_keys) == 0);
    PUPkv64Close(h, h_keys);
    f = fopen(path, "r");
    want(f != NULL);
    fclose(f);
    TESTBErmrf(tmp);
    done;
}

ok64 PUPNILtest() {
    sane(1);
    call(PUPNilRun);
    call(PUPNilBadRun);
    done;
}

TEST(PUPNILtest);
