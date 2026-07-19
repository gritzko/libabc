#include "SORT.h"

#include "Y.h"

// ABC-021: passes ping-pong exactly n elements between the two
// buffers (the old code took each buffer's full extent for the next
// pass); chunks still clamp at creation (MEM-003)
ok64 SORTu64($u64 into, $u64 from) {
    if ($len(into) < $len(from)) return SORTNOROOM;
    i64 n = $len(from);
    aBpad2(u8cs, chunks, Y_MAX_INPUTS);
    ok64 o = OK;
    size_t clen = 1;
    b8 dir = NO;
    while (o == OK && (i64)clen < n) {
        dir = !dir;
        u64* src = dir ? from[0] : into[0];
        u64* dst = dir ? into[0] : from[0];
        $u8 out8;
        out8[0] = (u8*)dst;
        out8[1] = (u8*)(dst + n);
        u64* p = src;
        u64* end = src + n;
        while (p < end && o == OK) {
            while (!$empty(chunksidle) && p < end) {
                u8cs chunk;
                chunk[0] = (u8c*)p;
                p += clen;
                chunk[1] = p > end ? (u8c*)end : (u8c*)p;
                HEAPu8csPush1Z(chunksbuf, chunk, SORTu64z);
            }
            while (o == OK && !$empty(chunksdata)) {
                o = SORTu64next(out8, chunksdata);
            }
        }
        clen *= Y_MAX_INPUTS;
    }
    if (o != OK) return o;
    if (dir == NO) {
        o = u64sFeed(into, (u64c$c)from);
    } else {
        *into += n;
    }
    return o;
}
