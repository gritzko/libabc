#include "COMB.h"

#include <unistd.h>

#include "S.h"
#include "BUF.h"
#include "PRO.h"
#include "TEST.h"

ok64 COMBtest1() {
    sane(1);
    aBcpad(u8, pad, PAGESIZE);
    COMBinit(padbuf);
    $feed1(padidle, 'A');
    COMBsave(padbuf);
    Breset(padbuf);
    call(COMBload, padbuf);
    testeqv((long long)($len(paddata)), (long long)(1), "%lld");
    a$str(str1, "A");
    want($eq(paddata, str1));
    done;
}

// ABC-015: the COMB header is untrusted cross-process input; a corrupt
// magic or out-of-range offsets must COMBBAD, not move borders blindly.
ok64 COMBtest3() {
    sane(1);
    aBcpad(u8, pad, PAGESIZE);
    COMBinit(padbuf);
    $feed1(padidle, 'A');
    COMBsave(padbuf);
    u64* c = (u64*)padbuf[0];
    // data offset beyond the buffer
    u64 keep = c[2];
    c[2] = PAGESIZE + 4096;
    testeqv((long long)(COMBload(padbuf)), (long long)(COMBBAD), "%lld");
    want(Bok(padbuf));
    c[2] = keep;
    // past < header
    keep = c[1];
    c[1] = 8;
    testeqv((long long)(COMBload(padbuf)), (long long)(COMBBAD), "%lld");
    c[1] = keep;
    // inverted past/data
    keep = c[1];
    c[1] = c[2] + 8;
    testeqv((long long)(COMBload(padbuf)), (long long)(COMBBAD), "%lld");
    c[1] = keep;
    // corrupt magic
    ((u8*)c)[0] ^= 0xff;
    testeqv((long long)(COMBload(padbuf)), (long long)(COMBBAD), "%lld");
    ((u8*)c)[0] ^= 0xff;
    // intact header loads fine
    call(COMBload, padbuf);
    testeqv((long long)($len(paddata)), (long long)(1), "%lld");
    done;
}

// #define X(M, name) M##u8##name
// #include "COMBx.h"
// #undef X

ok64 COMBtest2() {
    sane(1);
    aBcpad(u8, pad, PAGESIZE);
    // call(COMBu8init, padbuf);
    done;
}

ok64 COMBtest() {
    sane(1);
    call(COMBtest1);
    // call(COMBtest2);
    call(COMBtest3);
    done;
}

TEST(COMBtest);
