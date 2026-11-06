#ifndef ABC_SWISS_H
#define ABC_SWISS_H
//  SWISS-26B05A2Jxv: the non-template bits of SWISSx.h (SWISS.md): the
//  control byte codes and the 16-byte group scans, SSE2, NEON or SWAR,
//  picked at compile time (ABC_SWISS_SWAR forces SWAR).  A scan returns
//  a 16-bit mask, bit i for control byte i of the group.
#include "01.h"
#include "OK.h"

con ok64 SWISBADARG = 0x72049c2ca34a6d0;
con ok64 SWISFULL = 0x72049c3de555;
con ok64 SWISNONE = 0x72049c5d85ce;
con ok64 SWISNOROOM = 0x72049c5d86d8616;

#define SWISS_EMPTY 0x00  // a zeroed control array is an empty table
#define SWISS_TOMB 0x01
#define SWISS_MOVE 0x02  // a resident not yet re-placed by a rehash
#define SWISS_LIVE 0x80  // | 7 tag bits of the hash
#define SWISS_GROUP 16
#define SWISS_HEAD 16  // bytes before the control array: u64 room left, pad

#if defined(__SSE2__) && !defined(ABC_SWISS_SWAR)
#include <emmintrin.h>
fun __m128i SWISS_ld(u8 const *g) { return _mm_load_si128((__m128i const *)g); }
fun u32 SWISSMatch(u8 const *g, u8 tag) {
    __m128i t = _mm_set1_epi8((char)tag);
    return (u32)_mm_movemask_epi8(_mm_cmpeq_epi8(SWISS_ld(g), t));
}
fun u32 SWISSLive(u8 const *g) { return (u32)_mm_movemask_epi8(SWISS_ld(g)); }
fun u32 SWISSEmpty(u8 const *g) {
    return (u32)_mm_movemask_epi8(_mm_cmpeq_epi8(SWISS_ld(g), _mm_setzero_si128()));
}
#elif defined(__aarch64__) && !defined(ABC_SWISS_SWAR)
#include <arm_neon.h>

fun u32 SWISS_pack(uint8x16_t m) {
    static u8 const bit[16] = {1, 2, 4, 8, 16, 32, 64, 128,
                               1, 2, 4, 8, 16, 32, 64, 128};
    uint8x16_t b = vandq_u8(m, vld1q_u8(bit));
    return vaddv_u8(vget_low_u8(b)) | (u32)vaddv_u8(vget_high_u8(b)) << 8;
}
fun u32 SWISSMatch(u8 const *g, u8 tag) {
    return SWISS_pack(vceqq_u8(vld1q_u8(g), vdupq_n_u8(tag)));
}
fun u32 SWISSLive(u8 const *g) {
    return SWISS_pack(vreinterpretq_u8_s8(vshrq_n_s8(vld1q_s8((i8 const *)g), 7)));
}
fun u32 SWISSEmpty(u8 const *g) { return SWISS_pack(vceqzq_u8(vld1q_u8(g))); }
#else
#define SWISS_LO 0x7f7f7f7f7f7f7f7fULL
//  the 0x80 bits of a word's bytes, packed into its 8 low bits
fun u32 SWISS_pack(u64 hi) {
    return (u32)(((hi >> 7) * 0x0102040810204080ULL) >> 56);
}
fun u64 SWISS_zero(u64 w) { return ~((((w & SWISS_LO) + SWISS_LO) | w) | SWISS_LO); }
fun u32 SWISS_two(u8 const *g, u64 x, u8 how) {
    u64 w[2];
    memcpy(w, g, 16);
    u64 a = how ? w[0] & ~SWISS_LO : SWISS_zero(w[0] ^ x);
    u64 b = how ? w[1] & ~SWISS_LO : SWISS_zero(w[1] ^ x);
    return SWISS_pack(a) | SWISS_pack(b) << 8;
}
fun u32 SWISSMatch(u8 const *g, u8 tag) {
    return SWISS_two(g, tag * 0x0101010101010101ULL, 0);
}
fun u32 SWISSLive(u8 const *g) { return SWISS_two(g, 0, 1); }
fun u32 SWISSEmpty(u8 const *g) { return SWISS_two(g, 0, 0); }
#undef SWISS_LO
#endif

#endif
