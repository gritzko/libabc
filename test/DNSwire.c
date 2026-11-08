// STD-26B068WFR4 repro: RFC 1035 §2.3.2 puts 16/32-bit fields in
// network order; the codec swapped unconditionally, so a BE host sent LE.

#include "DNS.h"

#include "PRO.h"
#include "TEST.h"

ok64 DNSwireBytes() {
    sane(1);
    a_pad(u8, buf, 16);
    call(DNSu8sFeed16, buf_idle, 80);
    call(DNSu8sFeed32, buf_idle, 0x01020304);
    u8 const want6[] = {0x00, 0x50, 0x01, 0x02, 0x03, 0x04};
    a$(u8c, exp, want6);
    want(u8csEq(buf_datac, exp));

    u8 const raw[] = {0x12, 0x34, 0xa1, 0xb2, 0xc3, 0xd4};
    u8cs from = {raw, raw + sizeof(raw)};
    u16 v16 = 0;
    u32 v32 = 0;
    call(DNSu8sDrain16, from, &v16);
    call(DNSu8sDrain32, from, &v32);
    want(v16 == 0x1234);
    want(v32 == 0xa1b2c3d4);
    want($empty(from));
    done;
}

TEST(DNSwireBytes);
