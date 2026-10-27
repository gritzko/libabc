//  PUPS-26A24FhuO9: an empty run.  PUPCreate*(no bytes) books a run with no
//  rows that PUPOpenAll maps back, every reader sees empty, and a ladder
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
#include "HITx.h"
#include "PUPx.h"
#undef X

static ok64 PUPNilRun(void) {
    sane(1);
    call(FILEInit);
    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
    a_cstr(ext, ".nil.idx");
    u8cs none = {};
    Bpup256 pups = {};
    call(pup256bAllocate, pups, 8);
    call(PUPCreateAs, pups, $path(dir), ext, none, YES);
    call(PUPCreateAt, pups, $path(dir), ext, none, 5);  //  the fast path too
    want(PUPCount(pups) == 2);
    for (u32 row = 0; row <= 16; row += 16) {
        u8cs d = {};
        PUPDataRow(d, pups, 0, row);
        want(d[0] != NULL && u8csEmpty(d));
    }
    call(PUPClose, pups);

    //  reopened: two empty runs, by every reader
    call(pup256bAllocate, pups, 8);
    call(PUPOpenAll, pups, $path(dir), ext);
    want(PUPCount(pups) == 2);
    Bu8cs all = {};
    call(u8csbAllocate, all, 4);
    call(PUPAllDataRow, all, pups, 16);
    want(u8csbDataLen(all) == 2);
    for (size_t i = 0; i < 2; i++) want(u8csEmpty(u8csbDataHead(all)[i]));
    call(PUPAllData, all, pups);
    for (size_t i = 0; i < u8csbDataLen(all); i++)
        want(u8csEmpty(u8csbDataHead(all)[i]));
    u8cs toc = {};
    PUPToc(toc, pups, 1, 16);
    want(u8csEmpty(toc));

    //  rows on top: the empty runs merge in as nothing
    kv64 rows[2] = {{1, 10}, {2, 20}};
    u8cs rec = {(u8c *)rows, (u8c *)(rows + 2)};
    call(PUPCreateAs, pups, $path(dir), ext, rec, YES);
    call(PUPLadder, pups, $path(dir), ext, PUPkv64Merge, sizeof(kv64));
    kv64 got[4];
    kv64s out = {got, got + 4};
    call(PUPkv64Read, out, pups, NULL);
    want(out[0] - got == 2 && got[0].key == 1 && got[1].val == 20);
    call(PUPClose, pups);
    u8csbFree(all);

    //  a 0-byte run still fails the open
    char path[512];
    snprintf(path, sizeof path, "%s/0000000009.nil.idx", tmp);
    FILE *f = fopen(path, "w");
    want(f != NULL);
    fclose(f);
    call(pup256bAllocate, pups, 8);
    want(PUPOpenAll(pups, $path(dir), ext) == PUPEMPTY);
    call(PUPClose, pups);
    TESTBErmrf(tmp);
    done;
}

ok64 PUPNILtest() {
    sane(1);
    call(PUPNilRun);
    done;
}

TEST(PUPNILtest);
