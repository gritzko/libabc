#include "Bx.h"

#define T X(, )

fun T* X(LIST, atp)(X(, b) list, u32 ndx) { return X(, bAtP)(list, ndx); }

fun u32 X(LIST, next)(X(, b) list, u32 ndx) {
    return X(, bAtP)(list, ndx)->_list.next;
}

fun ok64 X(LIST, insert)(X(, b) list, T const* entry, u32 prev) {
    T** data = X(, bData)(list);
    size_t len = $len(data);
    if (len < prev) return LISTBADNDX;
    ok64 o = X(, bFeedP)(list, entry);
    if (o != OK) return o;
    u32 next = X(, bAtP)(list, prev)->_list.next;
    // MEM-014: the successor index is taken from a list link that may be
    // corrupt or caller-controlled; validate it against the list length
    // before dereferencing list[next], since bAtP only asserts (compiled
    // out in release) and would otherwise write out of bounds at :next.
    if (next > len) return LISTBADNDX;
    X(, bAtP)(list, len)->_list.next = next;
    X(, bAtP)(list, len)->_list.prev = prev;
    X(, bAtP)(list, prev)->_list.next = len;
    X(, bAtP)(list, next)->_list.prev = len;
    return OK;
}
