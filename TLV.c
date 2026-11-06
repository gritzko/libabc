#include "TLV.h"

#include "OK.h"
#include "PRO.h"

fun ok64 TLVprobe(u8* t, u8* hlen, u64* blen, $cu8c data) {
    if ($empty(data)) return TLVNODATA;
    if (TLVshort(**data)) {
        if ($len(data) < 2) return TLVNODATA;
        *t = **data - TLVaA;
        *hlen = 2;
        *blen = (*data)[1];
    } else if (TLVlong(**data)) {
        if ($len(data) < 5) return TLVNODATA;
        *t = **data;
        *hlen = 5;
        *blen = (u32)$at(data, 1) | ((u32)$at(data, 2) << 8) |
                ((u32)$at(data, 3) << 16) | ((u32)$at(data, 4) << 24);
    } else if (TLVhuge(**data)) {
        if ($len(data) < 9) return TLVNODATA;
        *t = **data + TLVaA;
        *hlen = 9;
        u64 l;
        memcpy(&l, *data + 1, sizeof(l));
        *blen = l;
    } else {
        return TLVBAD;
    }
    // Subtraction-based bound: each branch above guaranteed
    // $len(data) >= *hlen, so $len(data) - *hlen cannot underflow.
    // Comparing the room that remains avoids the additive wrap where a
    // huge *blen would make (*hlen + *blen) overflow below $len(data).
    return *blen <= (u64)$len(data) - *hlen ? OK : TLVNODATA;
}

ok64 _TLVu8sDrain(u8cs from, u8p type, u8csp value) {
    sane(type != NULL && value != NULL && $ok(from));
    u8 hlen = 0;
    u64 blen = 0;
    call(TLVprobe, type, &hlen, &blen, from);
    value[0] = from[0] + hlen;
    value[1] = value[0] + blen;
    from[0] += hlen + blen;
    done;
}

ok64 _TLVu8sFeed(u8s into, u8 type, u8csc value) {
    sane(TLVlong(type) && into != NULL && value != NULL);
    u64 len = $len(value);
    test($len(into) >= (i64)(len + 5), TLVNOROOM);
    if (len < 0x100) {
        u8sFeed2(into, type | TLVaA, (u8)len);
    } else {
        u8sFeed1(into, type & ~TLVaA);
        u8sFeed32(into, (u32*)&len);
    }
    u8sCopy(into, value);
    *into += len;
    done;
}

ok64 TLVFeedKeyVal($u8 tlv, u8c type, u8csc key, $cu8c val) {
    sane($ok(tlv) && $ok(key) && ($empty(val) || $ok(val)));
    size_t keylen = $len(key);
    test(keylen < 0x100, TLVBADARG);
    u64 blen = keylen + $len(val) + 1;
    test($len(tlv) >= (i64)(blen + 4 + 1), TLVNOROOM);
    if (blen < 0x100) {
        u8sFeed2(tlv, type | TLVaA, (u8)blen);
    } else {
        u8sFeed1(tlv, type & ~TLVaA);
        u8sFeed32(tlv, (u32*)&blen);
    }
    **tlv = keylen;
    ++*tlv;
    call(u8sFeed, tlv, key);
    call(u8sFeed, tlv, val);
    done;
}
ok64 TLVDrain$(u8c$ rec, u8cs from) {
    sane(rec != NULL && $ok(from));
    u8 hlen = 0;
    u64 blen = 0;
    u8 t = 0;
    call(TLVprobe, &t, &hlen, &blen, from);
    rec[0] = from[0];
    rec[1] = from[0] + hlen + blen;
    from[0] += hlen + blen;
    done;
}

ok64 TLVDrainKeyVal(u8* type, u8cs key, $u8c val, u8cs tlv) {
    sane(type != NULL && key != NULL && val != NULL && $ok(tlv));
    u8 hlen = 0;
    u64 blen = 0;
    call(TLVprobe, type, &hlen, &blen, tlv);
    u8cs body = {tlv[0] + hlen, tlv[0] + hlen + blen};
    // idlen byte + id data + value must fit in body
    // body[0] is idlen, need at least 1 + idlen bytes
    test($len(body) > 0 && $len(body) > **body, TLVBAD);
    key[0] = body[0] + 1;
    key[1] = key[0] + **body;
    val[0] = key[1];
    val[1] = body[1];
    tlv[0] = body[1];
    done;
}
ok64 TLVu8sStart(u8sc outer, u8s inner, u8 lit) {
    sane(u8sOK(outer) && inner != NULL && TLVlong(lit));
    test($len(outer) >= 5, TLVNOROOM);
    $mv(inner, outer);
    u32 z = 0;
    u8sFeed1(inner, lit);
    u8sFeed32(inner, &z);
    done;
}

ok64 TLVu8sEnd(u8s outer, u8sc inner, u8 lit) {
    sane(u8sOK(outer) && u8sOK(inner) && inner[1] == outer[1] &&
         *inner >= *outer + 5 && lit == **outer);
    u64 tl = *inner - *outer;
    test(tl - 5 <= u32max, TLVTOOLONG);
    *(u32*)(1 + *outer) = tl - 5;
    if (tl <= 0xff + 5) {
        **outer |= TLVaA;
        memmove(*outer + 2, *outer + 5, tl - 5);
        *outer = *inner - 3;
    } else {
        *outer = *inner;
    }
    done;
}

ok64 TLVu8sStartHuge(u8sc outer, u8s inner, u8 lit) {
    sane(u8sOK(outer) && inner != NULL && TLVlong(lit));
    test($len(outer) >= 9, TLVNOROOM);
    $mv(inner, outer);
    u64 z = 0;
    u8sFeed1(inner, lit - TLVaA);
    u8sFeed64(inner, &z);
    done;
}

ok64 TLVu8sEndHuge(u8s outer, u8sc inner, u8 lit) {
    sane(u8sOK(outer) && u8sOK(inner) && inner[1] == outer[1] &&
         *inner >= *outer + 9 && lit == **outer + TLVaA);
    u64 tl = *inner - *outer;
    *(u64*)(1 + *outer) = tl - 9;
    if (tl <= 0xff + 9) {
        **outer += 2 * TLVaA;
        memmove(*outer + 2, *outer + 9, tl - 9);
        *outer = *inner - 7;
    } else {
        *outer = *inner;
    }
    done;
}
