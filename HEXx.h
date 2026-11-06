#include "HEX.h"

#define T X(, )

fun ok64 X(HEX, Put)(u8s hex, T const* x) {
    u8cs bin = {(u8*)x, (u8*)(x + 1)};
    return HEXu8sFeedSome(hex, bin);
}

#undef T  // CODE-26B065T8Mr: leaked into every SHA.h includer
