#ifndef ABC_PATH_H
#define ABC_PATH_H

//  PATH: UTF-8 path manipulation
//
//  Valid paths: UTF-8, no \r\t\n\0, / separated.
//  Buffer ops keep the byte past DATA (idle[0]) NUL, so `$path(buf)`
//  yields a C-string-compatible view.
//
//  Types:
//    path8s   -- const slice view; [head, term), *term == 0 invariant
//    path8b   -- owned buffer for composition; DATA is the live path
//
//  Naming:
//    PATHu8s*  -- slice-level ops (read-only: validate, iterate, extract)
//    PATHu8b*  -- buffer-level ops (mutate: feed, push, pop, normalize)

#include "01.h"
#include "BUF.h"
#include "OK.h"

// Slice view into a path (const, NUL-terminated just past term).
typedef u8cs  path8s;
typedef u8csc path8sc;
typedef u8csp path8sp;

// Owned buffer for path composition.
typedef u8b   path8b;
typedef u8bp  path8bp;

// A u8cs view over a path buffer's DATA. Caller must have terminated
// the buffer (PATHu8bTerm / Feed / Push) so *buf[2] == 0.
#define $path(buf) u8bDataC(buf)

// Const path slice from C string (no NUL in the slice; NUL sits at term).
#define PATHu8csOf(str) {(u8cp)(str), (u8cp)(str) + strlen(str)}

// Error codes
con ok64 PATHFAIL   = 0x64a7513ca495;   // general failure
con ok64 PATHBAD    = 0x1929d44b28d;    // invalid UTF-8 / forbidden char
con ok64 PATHNOROOM = 0x64a7515d86d8616; // no room for NUL or data

// --- Slice predicates / validators ---

fun b8 PATHu8sIsAbsolute(u8cs path) {
    if (!$ok(path) || $empty(path)) return NO;
    return *path[0] == '/';
}

// Full UTF-8 + forbidden-char check.
ok64 PATHu8sVerify(u8cs path);

// Segment validation (no '/' in segment).
ok64 PATHu8sVerifySegment(u8cs segment);

// --- Slice extractors (views into path, no copy) ---

// Basename view. Empty when path ends in '/'.
void PATHu8sBase(u8csp out, u8cs path);

// Directory view. Empty when path has no '/' (caller may substitute "." ).
void PATHu8sDir(u8csp out, u8cs path);

// File extension view (excluding the dot). Empty when none.
void PATHu8sExt(u8csp out, u8cs path);

// --- Slice iteration ---

// Drain the next segment from cursor, advancing cursor[0].
// Emits empty segments for runs like "//".  Returns END when exhausted.
ok64 PATHu8sDrain(u8cs cursor, u8csp seg);

// Same, but skip empty segments.  Use this in almost all practical code.
ok64 PATHu8sDrainNE(u8cs cursor, u8csp seg);

// Iterate non-empty segments of `path`.
// Usage:
//   $eachseg(seg, some_path) {
//       // seg is u8cs, one non-empty segment
//   }
// Expands to a for-loop; seg is in-scope only inside the loop body.
#define $eachseg(seg, path)                                         \
    for (u8cs _cur_##seg = {(path)[0], (path)[1]},                  \
              seg = {NULL, NULL};                                   \
         PATHu8sDrainNE(_cur_##seg, seg) == OK; )

// --- Buffer composition (each op NUL-terminates idle[0]) ---

// Write NUL at idle[0] (idempotent).  No-op on empty buffer returns
// PATHNOROOM only when idle is exhausted.
fun ok64 PATHu8bTerm(path8b p) {
    if ($empty(u8bIdleC(p))) return PATHNOROOM;
    *u8bIdleHead(p) = 0;
    return OK;
}

// Append `data` verbatim (validated), NUL-terminate.
ok64 PATHu8bFeed(path8b p, u8csc data);

// Append one validated segment (inserts '/' if needed), NUL-terminate.
ok64 PATHu8bPush(path8b p, u8csc segment);

// Drop last segment (keeps leading '/' for absolute paths).
ok64 PATHu8bPop(path8b p);

// Reset `into` then copy `src`.
ok64 PATHu8bDup(path8b into, u8csc src);

// Append `rel` segment-by-segment.
ok64 PATHu8bAdd(path8b into, u8csc rel);

// Concatenate `a` and `b` into `out` (reset first), normalized.
ok64 PATHu8bJoin(path8b out, u8cs a, u8cs b);

// Append / + tmpl with any 'X' chars randomized (mkstemp-style).
ok64 PATHu8bAddTmp(path8b p, u8cs tmpl);

// Write the normalized form of `orig` into `out` (reset first).
// Safe when `out`'s DATA aliases `orig`.
ok64 PATHu8bNorm(path8b out, u8cs orig);

// Resolve `rel` against `base` (`out` reset first).  Both treated as
// directories; result is normalized.
ok64 PATHu8bAbs(path8b out, u8cs base, u8cs rel);

// Compute `abs` relative to `absbase` (`out` reset first).
// Both must be absolute.
ok64 PATHu8bRel(path8b out, u8cs absbase, u8cs abs);

// realpath(3) of `path` into `out` (reset first).
ok64 PATHu8bReal(path8b out, u8cs path);

// Varargs helper: Feed first slice as base, Push rest.  Slice list is
// NULL-terminated.  u8c *const * accepts both u8cs and u8csc.
ok64 PATHu8bBuildN(path8b p, u8c *const **slices);

// PATHu8bAren: like u8bAren, but additionally writes a trailing NUL
// byte after the copy and parks it in PAST so the rented slice is
// NUL-terminated (path8b invariant) and survives subsequent rentals.
// `ren[1]` points at the NUL byte (it sits one past the slice end).
// Returns BNOROOM if the arena can't fit path bytes + NUL.
fun ok64 PATHu8bAren(u8 *const *arena, u8csp ren, u8csc orig) {
    size_t n = (size_t)(orig[1] - orig[0]);
    if (arena[2] + n + 1 > arena[3]) return BNOROOM;
    u8 *base = arena[2];
    if (n) memcpy(base, orig[0], n);
    base[n] = 0;
    ren[0] = base;
    ren[1] = base + n;
    ((u8 **)arena)[1] = base + n + 1;
    ((u8 **)arena)[2] = base + n + 1;
    return OK;
}

// a_ren_path(news, arena, orig): u8 path rental that preserves the
// NUL terminator at `news[1]` (path8b-compatible).  Uses call(); must
// run inside a sane()'d function.
#define a_ren_path(news, arena, orig) \
    u8cs news = {};                   \
    call(PATHu8bAren, arena, news, orig)

// PATHu8bAcq: NUL-preserving counterpart to u8bAcq.  Writes a NUL
// byte at the current IDLE start, snapshots DATA into `ren`, then
// parks both DATA bytes AND the NUL into PAST so the slice stays
// NUL-terminated across subsequent rentals.  `*ren[1]` is the NUL.
// Pair with `u8bAlign(arena)` to open the rental.  Returns BNOROOM
// if no byte is left for the NUL.
fun ok64 PATHu8bAcq(u8 *const *arena, u8csp ren) {
    if (arena[2] >= arena[3]) return BNOROOM;
    arena[2][0] = 0;
    ren[0] = arena[1];
    ren[1] = arena[2];
    ((u8 **)arena)[1] = arena[2] + 1;
    ((u8 **)arena)[2] = arena[2] + 1;
    return OK;
}

// a_cq_path(news, buf): path-flavored a_cq — declare `news` (u8cs),
// seal current DATA with a NUL parked in PAST.  Uses call(); must
// run inside a sane()'d function.
#define a_cq_path(news, buf) \
    u8cs news = {};          \
    call(PATHu8bAcq, buf, news)

// --- Stack allocation macros ---

// a_path(name)                    -- empty path buffer, NUL-terminated
// a_path(name, base)              -- feed base slice
// a_path(name, base, seg1, seg2)  -- feed base, push segments
#define a_path(n, ...)                                              \
    a_pad(u8, n, FILE_PATH_MAX_LEN);                                \
    PATHu8bTerm(n);                                                 \
    __VA_OPT__({                                                    \
        u8c *const *_sl_##n[] = {__VA_ARGS__, NULL};                \
        PATHu8bBuildN(n, _sl_##n);                                  \
    })

// a_abspath(name, seg1, ...) -- absolute path (starts with '/')
#define a_abspath(n, ...)                                           \
    a_pad(u8, n, FILE_PATH_MAX_LEN);                                \
    u8sFeed1(n##_idle, '/');                                        \
    PATHu8bTerm(n);                                                 \
    __VA_OPT__({                                                    \
        u8c *const *_sl_##n[] = {__VA_ARGS__, NULL};                \
        PATHu8bBuildN(n, _sl_##n);                                  \
    })

#endif  // ABC_PATH_H
