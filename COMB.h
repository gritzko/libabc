#ifndef ABC_COMB_H
#define ABC_COMB_H

#include <string.h>

#include "01.h"
#include "BUF.h"
#include "OK.h"

con ok64 COMBBAD = 0xc6162cb28d;

// ABC-015: header is four u64 on every arch; sizeof(size_t)*4 broke 32-bit
#define COMBsize (sizeof(u64) * 4)

// ABC-015: was a non-static DEFINITION (duplicate symbol per includer)
static const char COMBmagic[8] = "ABCCOMB";

#define COMBinit(buf)               \
    do {                            \
        u64** b = (u64**)buf;       \
        memcpy(b[0], COMBmagic, 8); \
        b[1] = b[0] + 4;            \
        b[2] = b[1];                \
    } while (0)

#define COMBsave(buf)            \
    do {                         \
        u64* c = (u64*)buf[0];   \
        memcpy(c, COMBmagic, 8); \
        c[1] = buf[1] - buf[0];  \
        c[2] = buf[2] - buf[0];  \
        c[3] = buf[3] - buf[0];  \
    } while (0)

// ABC-015: the header is cross-process input (COMB.md), so check the magic
// and range-check the stored offsets before moving the buffer's borders.
fun ok64 COMBload(u8bp buf) {
    if (BNULL(buf)) return COMBBAD;
    u64 const* c = (u64 const*)buf[0];
    size_t cap = (size_t)((u8c*)buf[3] - (u8c*)buf[0]);
    if (cap < COMBsize || 0 != memcmp(c, COMBmagic, 8)) return COMBBAD;
    if (c[1] < COMBsize || c[1] > c[2] || c[2] > cap) return COMBBAD;
    u8** b = (u8**)buf;
    b[1] = b[0] + c[1];
    b[2] = b[0] + c[2];
    return OK;
}

#endif  // ABC_COMB_H
