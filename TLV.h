#ifndef LIBRDX_TLV_H
#define LIBRDX_TLV_H
#include "01.h"
#include "INT.h"
#include "OK.h"
#include "S.h"

con ok64 TLVRECBAD = 0x1d55f6ce30b28d;
con ok64 TLVBADREC = 0x1d55f2ca35b38c;
con ok64 TLVNODATA = 0x1d55f5d834a74a;
con ok64 TLVBADTYPE = 0x7557cb28d76264e;
con ok64 TLVNOROOM = 0x1d55f5d86d8616;
con ok64 TLVOVERFLO = 0x7557d87ce6cf558;
con ok64 TLVBADCALL = 0x7557cb28d30a555;
con ok64 TLVBADARG = 0x1d55f2ca34a6d0;
con ok64 TLVBADKV = 0x7557cb28d51f;
con ok64 TLVTOOLONG = 0x7557dd6185585d0;

#define TLVaA 0x20
#define TLV_MAX_LEN ((1 << 30) - 1)

ok64 _TLVu8sFeed(u8s into, u8 type, u8csc value);

fun ok64 TLVu8sFeed(u8s into, u8 type, u8csc value) {
    if ($len(value) > 0xff || $len(into) < 2 + $len(value))
        return _TLVu8sFeed(into, type, value);
    u8sFeed2(into, type | TLVaA, (u8)$len(value));
    u8sFeed(into, value);
    return OK;
}

ok64 _TLVu8sDrain(u8cs from, u8p type, u8csp value);

fun ok64 TLVu8sDrain(u8cs from, u8p type, u8csp value) {
    if ($len(from) < 2 || !(**from & TLVaA))
        return _TLVu8sDrain(from, type, value);
    *type = **from & ~TLVaA;
    u8 len = *(1 + *from);
    if ($len(from) < 2 + len) return TLVNODATA;
    *from += 2;
    value[0] = *from;
    *from += len;
    value[1] = *from;
    return OK;
}

// ----------------------

fun int TLVhuge(u8 t) { return t >= '!' && t <= ':'; }
fun int TLVlong(u8 t) { return t >= 'A' && t <= 'Z'; }
fun int TLVshort(u8 t) { return t >= 'a' && t <= 'z'; }

fun u32 TLVlen(size_t len) { return len <= 0xff ? 2 + len : 5 + len; }

ok64 TLVDrain$(u8c$ rec, u8cs from);

ok64 TLVFeedKeyVal($u8 tlv, u8c type, u8csc key, $cu8c val);

ok64 TLVDrainKeyVal(u8* type, u8cs key, u8cs val, u8cs tlv);

// A record of yet-unknown length: Start reserves a long (Huge: a 9-byte)
// header at `idle` and sets `inner` past it; End frames it, moves `idle`.
ok64 TLVu8sStart(u8sc idle, u8s inner, u8 lit);

ok64 TLVu8sEnd(u8s idle, u8sc inner, u8 lit);

ok64 TLVu8sStartHuge(u8sc idle, u8s inner, u8 lit);

ok64 TLVu8sEndHuge(u8s idle, u8sc inner, u8 lit);

// Buffer form of Start/End kept for impII STATE.c (CODE-26B065FmPf):
// the header stashes the DATA length, DATA restarts at the payload.
fun ok64 TLVu8bInto(u8bp into, u8 type) {
    size_t dl = u8bDataLen(into);
    if (unlikely(dl > u32max)) return TLVTOOLONG;
    u8s inner = {};
    ok64 o = TLVu8sStart(u8bIdle(into), inner, type);
    if (o != OK) return o;
    u32 dl32 = dl;
    memcpy(into[2] + 1, &dl32, 4);
    ((u8**)into)[1] = ((u8**)into)[2] = inner[0];
    return OK;
}

fun ok64 TLVu8bOuto(u8bp into, u8 type) {
    if (u8bPastLen(into) < 5) return TLVNODATA;
    u8* hdr = into[1] - 5;
    u32 dl = 0;
    memcpy(&dl, hdr + 1, 4);
    if (hdr - into[0] < (i64)dl) return TLVBADREC;
    u8s outer = {hdr, into[3]};
    u8s inner = {into[2], into[3]};
    ok64 o = TLVu8sEnd(outer, inner, type);
    if (o != OK) return o;
    ((u8**)into)[1] = hdr - dl;
    ((u8**)into)[2] = outer[0];
    return OK;
}

#endif
