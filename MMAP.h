#ifndef ABC_MMAP_H
#define ABC_MMAP_H
#include "01.h"
#include "B.h"

con ok64 MMAPBADARG = 0x5962992ca34a6d0;
// MMAPFAIL defined in B.h

ok64 MMAPopen(voidb buf, size_t size);

fun ok64 MMAPclose(voidb buf) {
    if (buf == NULL || *buf == NULL) return MMAPBADARG;
    munmap(buf[0], Bsize(buf));
    memset((void **)buf, 0, sizeof(voidb));
    return OK;
}

ok64 MMAPresize(voidb buf, size_t new_size);

fun ok64 MMAPresize2(voidb buf) { return MMAPresize(buf, Bsize(buf) * 2); }

fun ok64 MMAPresize4(voidb buf) {
    size_t new_size = Bsize(buf);
    // MEM-021: a wrapping +25% or an unroundable size must be refused,
    // never remapped smaller than the buffer already is
    if (new_size > u64max - (new_size >> 2)) return MMAPBADARG;
    new_size += new_size >> 2;
    new_size = round_power_of_2(new_size);
    if (new_size == 0) return MMAPBADARG;
    return MMAPresize(buf, new_size);
}

fun ok64 MMAPmayresize(voidb buf, size_t idle_size) {
    size_t has_idle = $size(Bidle(buf));
    if (has_idle >= idle_size) return OK;
    // MEM-021: the grow-to size must not wrap before rounding, or the
    // caller's huge idle request turns into a shrink reported as OK
    size_t grow = idle_size - has_idle;
    if ((size_t)Bsize(buf) > u64max - grow) return MMAPBADARG;
    size_t new_size = round_power_of_2(Bsize(buf) + grow);
    if (new_size == 0) return MMAPBADARG;
    return MMAPresize(buf, new_size);
}

#define Bmmap(buf, len) MMAPopen((voidbp)buf, len * sizeof(**buf))
#define Bunmap(buf) MMAPclose((voidbp)buf)
#define Bremap(buf, new_len) MMAPresize((voidbp)buf, new_len * sizeof(**buf))
#define Bremap2(buf) MMAPresize2((voidbp)buf)
#define Bremap4(buf) MMAPresize4((voidbp)buf)
#define Bmayremap(buf, idle_len) \
    MMAPmayresize((voidbp)buf, idle_len * sizeof(**buf))

#endif
