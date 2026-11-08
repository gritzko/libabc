#include "TLV.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "FILE.h"
#include "INT.h"
#include "PRO.h"
#include "TEST.h"

ok64 TLVtest1() {
    sane(1);
    a_pad(u8, pad, 64);
    u8cs str1 = $u8str("Hello");
    u8cs str2 = $u8str(" ");
    u8cs str3 = $u8str("world!");
    u8 **into = u8bIdle(pad);
    call(TLVu8sFeed, into, 'S', str1);
    call(TLVu8sFeed, into, 'S', str2);
    call(TLVu8sFeed, into, 'S', str3);
    u8c **from = u8bDataC(pad);
    u8 t1, t2, t3;
    u8cs str1b, str2b, str3b;
    call(TLVu8sDrain, from, &t1, str1b);
    testeqv((long long)(0), (long long)($cmp(str1, str1b)), "%lld");
    call(TLVu8sDrain, from, &t2, str2b);
    testeqv((long long)(0), (long long)($cmp(str2, str2b)), "%lld");
    call(TLVu8sDrain, from, &t3, str3b);
    testeqv((long long)(0), (long long)($cmp(str3, str3b)), "%lld");
    done;
}

ok64 TLVtest2() {
    sane(1);
    a_pad(u8, pad, 256);
    u8 **init = u8bIdle(pad);
    aBpad(u8, tlv, 5000);
    for (u32 v = 0; v < 256; v++) {
        **init = v;
        ++*init;
    }
    u8c **block = u8bDataC(pad);
    testeqv((long long)($len(block)), (long long)(256), "%lld");
    u8 **into = u8bIdle(tlv);
    for (int j = 0; j < 2; j++) {
        call(TLVu8sFeed, into, 'B', block);
    }
    u8c **from = u8bDataC(tlv);
    $print(from);
    for (int i = 0; i < 2; i++) {
        u8cs take;
        u8 t;
        call(TLVu8sDrain, from, &t, take);
        assert(0 == $cmp(block, take));
    }
    done;
}

fun b8 u32pZ(u32p const *a, u32p const *b) { return *a < *b; }

#define X(M, name) M##u32p##name
#include "Bx.h"
#undef X

ok64 TLVtest3() {
    sane(1);
    a_pad(u8, pad, 1024);

    u8cs aaa = $u8str("aaa");
    u8cs bbb = $u8str("bbbb");
    u8cs ccc = $u8str("ccccc");

    call(TLVu8bInto, pad, 'A');
    call(u8bFeed, pad, aaa);
    call(TLVu8bInto, pad, 'B');
    call(TLVu8bInto, pad, 'C');
    call(u8bFeed, pad, ccc);
    call(TLVu8bOuto, pad, 'C');
    call(u8bFeed, pad, bbb);
    call(TLVu8bOuto, pad, 'B');
    call(TLVu8bOuto, pad, 'A');

    $println(pad_datac);

    u8cs ina = {}, inb = {}, inc = {};
    u8 at, bt, ct;
    call(TLVu8sDrain, pad_datac, &at, ina);
    a_head(u8c, head, ina, $len(aaa));
    a_rest(u8c, rest, ina, $len(aaa));
    call(TLVu8sDrain, rest, &bt, inb);
    call(TLVu8sDrain, inb, &ct, inc);

    testeqv((long long)(at), (long long)('A'), "%lld");
    testeqv((long long)(bt), (long long)('B'), "%lld");
    testeqv((long long)(ct), (long long)('C'), "%lld");

    testeqv((long long)(0), (long long)($cmp(ccc, inc)), "%lld");
    testeqv((long long)(0), (long long)($cmp(bbb, inb)), "%lld");
    testeqv((long long)(0), (long long)($cmp(aaa, head)), "%lld");

    done;
}

ok64 TLVtest4() {
    sane(1);
    aBpad2(u8, pad, PAGESIZE);
    u8 correct[] = {'a', 4, 't', 'e', 's', 't'};
    a$(u8c, cor, correct);
    u8cs text = {cor[0] + 2, cor[1]};
    u8s inner = {};
    call(TLVu8sStart, padidle, inner, 'A');
    call(u8sFeed, inner, text);
    call(TLVu8sEnd, padidle, inner, 'A');
    $testeq(cor, paddata);
    done;
}

// CODE-26B065FmPf: Start/End and the Huge pair frame every payload
// length right (short, long, huge), nest, and fail with `inner` untouched.
ok64 TLVtestStartEnd() {
    sane(1);
    Bu8 buf = {};
    call(u8bAllocate, buf, 1 << 18);
    u64 lens[] = {0, 1, 255, 256, 0x10000 + 9};
    for (int huge = 0; huge < 2; ++huge) {
        for (size_t i = 0; i < sizeof(lens) / sizeof(*lens); ++i) {
            u8bReset(buf);
            u8s inner = {};
            if (huge) {
                call(TLVu8sStartHuge, u8bIdle(buf), inner, 'X');
            } else {
                call(TLVu8sStart, u8bIdle(buf), inner, 'X');
            }
            memset(inner[0], 'x', lens[i]);
            inner[0] += lens[i];
            if (huge) {
                call(TLVu8sEndHuge, u8bIdle(buf), inner, 'X');
            } else {
                call(TLVu8sEnd, u8bIdle(buf), inner, 'X');
            }
            u64 hlen = lens[i] <= 0xff ? 2 : huge ? 9 : 5;
            testeqv((u64)(hlen + lens[i]), (u64)u8bDataLen(buf), FMTu64);
            u8 t = 0;
            u8cs val = {};
            // the inline drain misframes huge records, CODE-26A23C_Sau
            call(_TLVu8sDrain, u8bDataC(buf), &t, val);
            testeqv((u64)'X', (u64)t, FMTu64);
            testeqv(lens[i], (u64)$len(val), FMTu64);
        }
    }
    u8bReset(buf);
    u8s outer = {}, inner = {};
    call(TLVu8sStart, u8bIdle(buf), outer, 'O');
    call(TLVu8sStart, outer, inner, 'I');
    call(TLVu8sEnd, outer, inner, 'I');
    call(TLVu8sEnd, u8bIdle(buf), outer, 'O');
    u8 nested[] = {'o', 2, 'i', 0};
    a$(u8c, nest, nested);
    $testeq(nest, u8bDataC(buf));
    u8 four[4] = {};
    u8s tiny = {four, four + 4};
    u8s keep = {four, four};
    want(TLVu8sStart(tiny, keep, 'A') == TLVNOROOM);
    want(keep[1] == four);
    call(u8bFree, buf);
    done;
}

// MEM-004: a huge-type TLV record whose declared body length is near
// u64max makes the additive guard `hlen + blen <= len` wrap below `len`,
// so the bounds check passes and a wild, out-of-bounds value slice is
// returned. The probe MUST reject such a record (TLVNODATA), and a drain
// must not advance the stream past the (tiny) buffer.
ok64 TLVtest5() {
    sane(1);
    a_pad(u8, pad, 64);
    // huge-type byte ('!'..':'); '!' maps to type 'A' inside the probe
    u8 huge = '!';
    // 9 + blen wraps to a small value below $len(data) for the buggy guard
    u64 blen = u64max - 4;
    u8 **into = u8bIdle(pad);
    call(u8sFeed8, into, &huge);
    call(u8sFeed64, into, &blen);

    // A drain (which internally probes) must reject: the declared body
    // cannot fit in the 9-byte buffer. The buggy additive guard wraps and
    // returns OK, then forms a wild slice and desyncs the stream.
    u8c **from = u8bDataC(pad);
    size_t before = $len(from);
    u8cs rec = {};
    try(TLVDrain$, rec, from);
    testeqv((long long)(TLVNODATA), (long long)(__), "%lld");
    // stream must not have advanced past the tiny buffer
    testeqv((long long)(before), (long long)($len(from)), "%lld");
    __ = OK;  // the expected-failure above left __ set; clear before done
    done;
}

ok64 TLVtest() {
    sane(1);
    call(TLVtest1);
    call(TLVtest2);
    call(TLVtest3);
    call(TLVtest4);
    call(TLVtest5);
    call(TLVtestStartEnd);
    done;
}

TEST(TLVtest);
