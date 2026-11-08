//
// Created by gritzko on 5/10/24.
//

#ifndef LIBRDX_$_H
#define LIBRDX_$_H
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "01.h"

#define $head(s) ((s)[0])
#define $term(s) ((s)[1])
#define $last(s) ((s)[1] - 1)
#define $len(s) ($term(s) - $head(s))
#define $size(s) (((uint8_t *)$term(s)) - ((uint8_t *)$head(s)))
#define $at(s, n) (*($head(s) + (n)))
#define $atp(s, n) (($head(s) + (n)))

#define $empty(s) (s == NULL || $head(s) >= $term(s))
#define $ok(s) (s != NULL && *s != NULL && s[1] >= s[0])
//  $ok or the all-NULL empty slice
#define $okz(s) (s != NULL && (*s != NULL ? s[1] >= s[0] : s[1] == NULL))
#define a$(T, n, a) T *n[2] = {(a), (a) + (sizeof(a) / sizeof(T))};

// ABC-016: single-evaluation of o (may have side effects)
#define $off(s, o)                        \
    ({                                    \
        typeof((s)[0]) _oh = (s)[0] + (o); \
        _oh < (s)[1] ? _oh : (s)[1];      \
    })

#define a_dup(T, n, s) T *n[2] = {(s)[0], (s)[1]}
#define $dup(s) {(s)[0], (s)[1]}
#define zero$(s) s[0] = s[1] = NULL;

#define a_past(T, n, old, neu)                    \
    assert(old[1] == neu[1] && old[0] <= neu[0]); \
    T *n[2] = {old[0], neu[0]};

// a_rest: slice from offset to end [orig+off, end)
#define a_rest(T, n, orig, off)                        \
    T##s n = {(T *)(orig[0]) + (off), (T *)(orig[1])}; \
    assert($size(n) <= $size(orig));

// a_tail: last len elements [end-len, end)
#define a_tail(T, n, orig, len)                        \
    T##s n = {(T *)(orig[1]) - (len), (T *)(orig[1])}; \
    assert($size(n) <= $size(orig));

// a_head: first len elements [start, start+len)
#define a_head(T, n, orig, len)                        \
    T##s n = {(T *)(orig[0]), (T *)(orig[0]) + (len)}; \
    assert($size(n) <= $size(orig));

#define a$tail(T, n, s, off) \
    $##T n = {(i64)(off) > $len(s) ? s[1] : s[0] + (off), s[1]};
#define a$head(T, n, s, l) \
    T *n[2] = {s[0], ((i64)(l) > $len(s) ? s[1] : (s[0] + l))};

// a_part: len elements at offset off [start+off, start+off+len)
#define a_part(T, n, s, off, len)                   \
    T *n[2] = {*(s) + (off), *(s) + (off) + (len)}; \
    assert(n[0] >= (s)[0] && n[1] <= (s)[1]);
#define $set(a, b)   \
    {                \
        a[0] = b[0]; \
        a[1] = b[1]; \
    }

#define $mv(s1, s2)                           \
    {                                         \
        (s1)[0] = (s2)[0], (s1)[1] = (s2)[1]; \
    }

// FUZZ-26B04Iucge: typed slices are aligned by contract, see $fits.
#define $for(T, n, s) for (T *n = s[0]; n < s[1]; ++n)
#define $rof(T, n, s) for (T *n = s[1]; n > s[0] && (--n, 1);)
// Can bytes s be seen as a slice of T: an aligned base, whole records.
#define $fits(T, s)                                    \
    ((uintptr_t)(s)[0] % alignof(T) == 0 &&            \
     ((char const *)(s)[1] - (char const *)(s)[0]) % sizeof(T) == 0)
#define $eat(s) for (; s[0] < s[1]; s[0]++)

#define eats(T, i, s) for (T *i = s[0]; i < s[1]; i++)

typedef int (*fncmp)(const void *, const void *);
#define $sort(s, cmp) qsort((void *)s[0], $len(s), sizeof(**s), (fncmp)cmp)
#define $minsize(a, b) ($size(a) > $size(b) ? $size(b) : $size(a))

fun int $memcmp($cc a, $cc b) {
    size_t sza = $size(a), szb = $size(b);
    size_t sz = sza < szb ? sza : szb;
    int ret = sz ? memcmp(*a, *b, sz) : 0;  // STD-26B068VwHO: may be NULL
    if (ret == 0 && sza != szb) {
        ret = sza < szb ? -1 : 1;
    }
    return ret;
}

typedef int (*$cmpfn)($cc a, $cc b);

#define $cmp(a, b) $memcmp((void const *const *)(a), (void const *const *)b)

#define $eq(a, b) \
    ($size(a) == $size(b) && (!$size(a) || 0 == memcmp(*a, *b, $size(a))))

// Expand a slice as the (length, pointer) pair printf("%.*s", ...) wants.
// Use as: printf("got '" $FMT_S "'\n", $ARG(my_slice));
#define U8SFMT    "%.*s"
#define u8sFmt(s)   (int)$len(s), (char const *)(s)[0]

#define a$str(n, s)          \
    char const *__##n = (s); \
    u8 const *n[2] = {(u8 *)__##n, (u8 *)__##n + strlen(__##n)};

#define a_cstr(n, c) u8 const *n[2] = {(u8c *)(c), (u8c *)((c) + strlen(c))}

// u8cs initializer for a string literal — compile-time, no strlen.
// Usage: u8cs args[] = {u8slit("git"), u8slit("-C"), …};
#define u8slit(s) {(u8c *)(s), (u8c *)(s) + sizeof(s) - 1}

// u8cs initializer for a runtime NUL-terminated char* (uses strlen).
// Usage in an array: u8cs args[] = { ..., u8scstr(nstr[k]), ... };
#define u8scstr(c) {(u8c *)(c), (u8c *)(c) + strlen(c)}

#define a_u8cs(n, ...)          \
    u8c _##n[] = {__VA_ARGS__}; \
    u8c *n[2] = {_##n, _##n + sizeof(_##n)};

fun ok64 $feedf(u8 **into, u8 const *const *tmpl, ...) {
    va_list ap;
    u8 const **sarg = NULL;
    a_dup(u8 const, p, tmpl);
    va_start(ap, tmpl);
    while (!$empty(p) && !$empty(into)) {
        if (**p != '$') {
            **into = **p;
            ++*into;
            ++*p;
            continue;
        }
        ++*p;
        if ($empty(p)) return SBADARG;
        switch (**p) {
            case 's': {
                sarg = va_arg(ap, u8 const **);
                // ABC-005: a short feed truncates the output; report
                // SNOROOM instead of advancing past the '$s'
                size_t sz = $minsize(into, sarg);
                if (sz) memcpy(*into, *sarg, sz);
                *into += sz;
                if (sz < (size_t)$size(sarg)) {
                    va_end(ap);
                    return SNOROOM;
                }
                ++*p;
                break;
            }
            case 'u':
            case 'f': {
                size_t room = $size(into);
                int n = (**p == 'u') ? snprintf((char *)*into, room, FMTu64,
                                                va_arg(ap, u64))
                                     : snprintf((char *)*into, room, "%lf",
                                                va_arg(ap, double));
                if (n < 0) {
                    va_end(ap);
                    return SBADARG;
                }
                // ABC-005: snprintf truncation is SNOROOM; its NUL is
                // not payload, only room-1 bytes actually landed
                if ((size_t)n >= room) {
                    *into += room ? room - 1 : 0;
                    va_end(ap);
                    return SNOROOM;
                }
                *into += n;
                ++*p;
                break;
            }
            case '$':
                if ($len(into) < 2) return SNOROOM;
                **into = '$';
                ++*into;
                ++*p;
                break;
            default:
                if ($len(into) < 1) return SNOROOM;
                **into = '$';
                ++*into;
        }
    }
    va_end(ap);
    return $empty(p) ? OK : SNOROOM;
}

// ABC-005: must() takes (cond, msg); the one-arg form broke compilation
#define $tailshift(s, off, rm)                                \
    {                                                         \
        must($len(s) >= (off) + (rm), "tailshift range");     \
        memmove(*(s) + (off), *(s) + (off) + (rm),            \
                ($len(s) - (off) - (rm)) * sizeof(**(s)));    \
    }

#define zero(s) memset((void *)&(s), 0, sizeof(s))
#define zerop(s) memset((void *)s, 0, sizeof(*s))
#define $null(s)     \
    {                \
        s[0] = NULL; \
        s[1] = NULL; \
    }

// ABC-005: move the whole tail ($len-off-len), not len elements;
// the old form corrupted the tail and read OOB past term
#define $rm(s, off, len)                                    \
    {                                                       \
        memmove(s[0] + (off), s[0] + (off) + (len),         \
                ($len(s) - (off) - (len)) * sizeof(**s));   \
        s[1] -= len;                                        \
    }

// ABC-005: same tail-length fix as $rm ($tailshift is the reference)
#define $rm1(s, off)                                    \
    {                                                   \
        memmove(s[0] + (off), s[0] + (off) + 1,         \
                ($len(s) - (off) - 1) * sizeof(**s));   \
        s[1] -= 1;                                      \
    }

#define $rm1p(s, p)                                          \
    {                                                        \
        memmove(p, p + 1, ((u8c *)s[1]) - ((u8c *)(p + 1))); \
        s[1] -= 1;                                           \
    }

#endif  // LIBRDX_$_H
