#ifndef LIBRDX_PRO_H
#define LIBRDX_PRO_H

// It is not recommended to include this header from
// public .h files. It pollutes the namespace for non-ABC code.
// PRO.h use implies MAIN(), TEST(), FUZZ() or ABC_INIT in executables.

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include "BUF.h"
#include "OK.h"

con ok64 PROBADARG = 0x196d82ca34a6d0;
con ok64 PRONOROOM = 0x196d85d86d8616;

// CODE-26B065M0M4: call depth is per thread, like ABC_BASS
extern thread_local uint8_t _pro_depth;

// ===== ABC_BASS: thread-local scratch arena =====
//
// Buffer-Arena-Scratch-Stack — a per-thread LIFO arena that lives for
// the thread's lifetime.  Mapped by MAIN() / TEST() / FUZZ() for the
// main thread, by THREAD() or ABCThreadStart() for others; every
// call() / try() snapshots its boundaries on entry and rewinds them
// on return, so a procedure's BASS scratch dies automatically when
// control returns to the caller.  Forked children inherit BASS via
// CoW (MAP_PRIVATE).
//
// See abc/B.md §Arenas.  The macros a_lign / a_cquire / a_rent / a_ren
// / a_carve (below) act on BASS implicitly — there is no explicit
// arena parameter to pass down.
//
// Override ABC_BASS_BYTES before #include "PRO.h" to customize size.
// Default 1 GiB; mmap is lazy + MAP_NORESERVE so unused pages cost
// nothing until first touch.
#ifndef ABC_BASS_BYTES
#define ABC_BASS_BYTES (1ULL << 30)
#endif

extern thread_local u8 *ABC_BASS[4];

__attribute__((unused)) con char *_pro_indent =
    "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";

#define PROindent (_pro_indent + 32 - (_pro_depth & 31))

// Mandatory sanity checks; might be disabled in Release mode.
#ifndef ABC_INSANE
#define sane(c)                                                           \
    trace("%s>%s\n", PROindent, __func__);                                \
    if (!(c)) {                                                           \
        trace("%s<FAILSANITY at %s:%i\n", PROindent, __func__, __LINE__); \
        return FAILSANITY;                                                \
    }                                                                     \
    ok64 __ __attribute__((unused)) = OK;
#else
#define sane(c) ok64 __ __attribute__((unused)) = OK;
#endif

// `call` propagates errors by `return`-ing immediately on failure.  This
// implies a strict resource-management discipline: a function that owns
// resources (mmap, malloc, fd, raw-mode terminal, child pid…) cannot
// release them in scattered `if (failed) cleanup;` blocks, because any
// nested `call(...)` may abort before reaching them.
//
// The convention is **acquire at the top of the call chain**.  A
// top-level entry point opens every resource it needs, then delegates
// the work — passing buffers, file descriptors and state down by
// pointer.  Nested helpers never own.  
//
// In short: orthogonal resource mgmt vs control flow.  `call` only
// propagates errors; releasing belongs to the owner.
// `call` snapshots ABC_BASS (data+idle heads) on entry and rewinds
// on return, so any BASS scratch the callee acquired dies before
// control comes back.  Caller-side gauges / carves are preserved
// (their pointers sit below the snapshot).
#define call(f, ...)                                                    \
    {                                                                   \
        u8 __depth = _pro_depth++;                                      \
        a_dup(u8c, __bass, u8bDataC(ABC_BASS));                         \
        __ = (f(__VA_ARGS__));                                          \
        ((u8 **)ABC_BASS)[1] = (u8 *)__bass[0];                         \
        ((u8 **)ABC_BASS)[2] = (u8 *)__bass[1];                         \
        _pro_depth = __depth;                                           \
        if (__ != OK) {                                                 \
            _PRO_LIFT(__, __depth);                                     \
            trace("%s<%s at %s:%i\n", PROindent, ok64str(__), __func__, \
                  __LINE__);                                            \
            return __;                                                  \
        }                                                               \
    }

// e.g. scan(u64sDrain1, numbers, &i) { ... }  seen(SNODATA);
#define scan(f, ...) while (OK == (__ = f(__VA_ARGS__)))

// `try` is like `call` minus the on-failure return; BASS still
// rewinds either way.
#define try(f, ...)                                                     \
    {                                                                   \
        u8 __depth = _pro_depth++;                                      \
        a_dup(u8c, __bass, u8bDataC(ABC_BASS));                         \
        __ = (f(__VA_ARGS__));                                          \
        ((u8 **)ABC_BASS)[1] = (u8 *)__bass[0];                         \
        ((u8 **)ABC_BASS)[2] = (u8 *)__bass[1];                         \
        _pro_depth = __depth;                                           \
        if (__ != OK) {                                                 \
            _PRO_LIFT(__, __depth);                                     \
            trace("%s<%s at %s:%i\n", PROindent, ok64str(__), __func__, \
                  __LINE__);                                            \
        }                                                               \
    }

#define then if (__ == OK)

#define nedo if (__ != OK)

#define on(status) if (__ == status)

#define callsafe(fcall, ffail)                                          \
    {                                                                   \
        u8 __depth = _pro_depth++;                                      \
        a_dup(u8c, __bass, u8bDataC(ABC_BASS));                         \
        __ = (fcall);                                                   \
        ((u8 **)ABC_BASS)[1] = (u8 *)__bass[0];                         \
        ((u8 **)ABC_BASS)[2] = (u8 *)__bass[1];                         \
        _pro_depth = __depth;                                           \
        if (__ != OK) {                                                 \
            _PRO_LIFT(__, __depth);                                     \
            trace("%s<%s at %s:%i\n", PROindent, ok64str(__), __func__, \
                  __LINE__);                                            \
            ffail;                                                      \
            return __;                                                  \
        }                                                               \
    }

// Procedure return with no finalizations.
#define done return __;

// Procedure fails, skip to finalizations.
// CODE-26A24BvaL1: the fail site record, kept whatever ABC_TRACE says,
// so a failed TEST can name the check that failed.  fail() stamps its
// FILE:LINE:func unless the record already holds the same code at the
// same depth (then this fail() merely propagates it: the origin wins);
// call/try lift a failed callee's record to the caller's depth.  Output
// only: return paths are untouched.  Weak, so no executable defines it.
typedef struct {
    ok64 code;
    char const *file;
    char const *func;
    int line;
    u8 depth;
} PROsite;

__attribute__((weak)) thread_local PROsite PROfailed = {};

fun void PROfailAt(ok64 code, char const *file, int line, char const *func) {
    if (PROfailed.file && PROfailed.code == code &&
        PROfailed.depth == _pro_depth)
        return;
    PROfailed = (PROsite){code, file, func, line, _pro_depth};
}

#define _PRO_LIFT(o, d) \
    if (PROfailed.code == (o)) PROfailed.depth = (d);

#define fail(code)                                                     \
    {                                                                  \
        __typeof__(code) __pf = (code);                                \
        PROfailAt((ok64)__pf, __FILE__, __LINE__, __func__);           \
        trace("%s<%s at %s:%i\n", PROindent, ok64str(__pf), __func__,  \
              __LINE__);                                               \
        return __pf;                                                   \
    }

#define failc(code)                                                    \
    {                                                                  \
        __typeof__(code) __pf = (code);                                \
        PROfailAt((ok64)__pf, __FILE__, __LINE__, __func__);           \
        trace("%s<%s errno %i %s at %s:%i\n", PROindent, ok64str(__pf), \
              errno, strerror(errno), __func__, __LINE__);             \
        return __pf;                                                   \
    }

#define test(c, f) \
    if (!likely(c)) fail(f);

#define testsafe(c, f, cleanup) \
    if (!(c)) {                 \
        __ = f;                 \
        cleanup;                \
        fail(__);               \
    }

#define testc(c, f) \
    if (!(c)) failc(f);

#define otry(f, ...) __ = (f(__VA_ARGS__))

// `ocall` is `call` minus the BASS snapshot/rewind: the callee rents
// BASS to the caller, whose frame then owns it (ABC-26B07MlxmQ).
#define ocall(f, ...)                                                   \
    {                                                                   \
        u8 __depth = _pro_depth++;                                      \
        __ = (f(__VA_ARGS__));                                          \
        _pro_depth = __depth;                                           \
        if (__ != OK) {                                                 \
            _PRO_LIFT(__, __depth);                                     \
            trace("%s<%s at %s:%i\n", PROindent, ok64str(__), __func__, \
                  __LINE__);                                            \
            return __;                                                  \
        }                                                               \
    }

#define ofix(f) if (__ == (f) && OK == (__ = OK))

#define orly if (__ == OK)

#define oops if (__ != OK)

#define ocry(...)    \
    if (__ != OK) {  \
        __VA_ARGS__; \
        fail(__);    \
    }

#define is(f) (__ == f)

#define sure(f) \
    if (__ != f) fail(__);

#define seen(f)    \
    if (__ != f) { \
        fail(__);  \
    } else {       \
        __ = OK;   \
    }

#define mute(f, o)            \
    {                         \
        ok64 __ = (f);        \
        if (__ == o) __ = OK; \
        if (__ != OK) {       \
            fail(__);         \
        }                     \
    }

#define testeq(a, b)               \
    {                              \
        if (!likely((a) == (b))) { \
            fail(FAILEQ);          \
        }                          \
    }

#define testeqv(a, b, fmt)                                                     \
    {                                                                          \
        if (!likely((a) == (b))) {                                             \
            fprintf(stderr, "%sNot equal: " fmt " <> " fmt "\n", PROindent, a, \
                    b);                                                        \
            fail(FAILEQ)                                                       \
        }                                                                      \
    }

#define must(cond, msg)                                                 \
    if (!(cond)) {                                                      \
        fprintf(stderr, "%s assert fail %s at %s:%i\n", PROindent, msg, \
                __func__, __LINE__);                                    \
        __builtin_trap();                                               \
    }

// ===== BASS-implicit arena macros =====
//
// a_lign / a_cquire / a_rent / a_ren / a_carve all act on ABC_BASS.
// Each is one-liner sugar over the underlying u8a primitives (see
// abc/B.h u8aMark / u8aRewind / u8bAcquire / u8bAren and the typed
// gauge ops in Sx.h).  Within a procedure, all acquired bytes die at
// the surrounding call() / try() boundary — no manual release.
//
// IMPORTANT: these macros invoke the underlying function DIRECTLY
// (not via `call()`), because `call()` would snapshot+restore BASS
// and immediately undo the acquisition.  Errors propagate via `__`
// + `return __` exactly like `call()` does, just without the
// snapshot wrapper.
//
// Discipline: do NOT a_rent / a_carve between an a_lign and its
// matching a_cquire — those one-shot copies advance both DATA and
// IDLE boundaries and would corrupt the in-flight gauge.  The
// `must()` guards enforce DATA-empty at entry to those macros.

#define a_lign(T, gauge) T##gp gauge = T##bAlign(ABC_BASS)

#define a_cquire(T, slice) \
    T##cs slice = {};      \
    T##bAcq(ABC_BASS, slice)

#define a_rent(T, news, src)                                          \
    T##cs news = {};                                                  \
    do {                                                              \
        must(ABC_BASS[1] == ABC_BASS[2], "a_rent in open gauge");     \
        __ = T##bAren(ABC_BASS, news, src);                           \
        if (__ != OK) {                                               \
            trace("%s<%s at %s:%i\n", PROindent, ok64str(__),         \
                  __func__, __LINE__);                                \
            return __;                                                \
        }                                                             \
    } while (0)

#define a_ren(news, src) a_rent(u8, news, src)

// Note: B##T (non-const pointer array) instead of T##b (const-pointer
// array) — the function writes the pointers via const-cast, which the
// compiler is free to ignore on a const-declared variable.  B##T tells
// the compiler the pointers do change.
#define a_carve(T, child, cap)                                        \
    B##T child = {};                                                  \
    do {                                                              \
        must(ABC_BASS[1] == ABC_BASS[2], "a_carve in open gauge");    \
        __ = T##bAcquire(ABC_BASS, child, cap);                       \
        if (__ != OK) {                                               \
            trace("%s<%s at %s:%i\n", PROindent, ok64str(__),         \
                  __func__, __LINE__);                                \
            return __;                                                \
        }                                                             \
    } while (0)

#define $testeq(a, b)                                                         \
    {                                                                         \
        if (!likely($size(a) == $size(b) && 0 == memcmp(*a, *b, $size(a)))) { \
            fail(FAILEQ)                                                      \
        }                                                                     \
    }

#ifdef ABC_TRACE
#define trace(...) fprintf(stderr, __VA_ARGS__)
#else
#define trace(...) ;
#endif

// --- Process argv as a typed buffer ---
//
// `STD_ARGS` is a u8csb (buffer of u8cs slices) holding this process's
// argv as parsed by MAIN/TEST/FUZZ at startup.  Each element is a
// u8cs slice borrowing from the original argv[i] memory; argv[i] is
// NUL-terminated so the slice's pointers can be safely passed where
// a NUL-term char* is needed.
//
// Backing storage is acquired from ABC_BASS by `_parse_args` — sized
// exactly to argn, so there is no fixed cap.  MAIN must map ABC_BASS
// before calling `_parse_args`.
//
// Use `$arglen` for argv count, `$arg(i)` to get the i-th slice by
// value, `a$rg(name, i)` to bind a u8cs local, `an_arg(name, i)` for
// a safe optional bind (empty if i >= argc).  Pass `u8csbDataC(STD_ARGS)`
// to anything wanting a u8css over the whole argv (e.g. FILESpawn for
// self-exec).
extern u8cs *STD_ARGS[];

fun ok64 _parse_args(int argn, char **args) {
    if (argn < 0) return PROBADARG;
    ok64 o = u8csbAcquire(ABC_BASS, STD_ARGS, (size_t)argn);
    if (o != OK) return o;
    for (int i = 0; i < argn; ++i) {
        u8c *s[2] = $u8str(args[i]);
        memcpy(STD_ARGS[1] + i, s, sizeof(s));
    }
    STD_ARGS[2] = STD_ARGS[3];  // mark all DATA filled
    return OK;
}

#define $arglen $len(u8csbData(STD_ARGS))

#define $arg(i) (*u8cssAtP(u8csbData(STD_ARGS), i))

#define a$rg(name, i) \
    u8cs name = {};   \
    $mv(name, *u8cssAtP(u8csbData(STD_ARGS), i));

#define an_arg(name, i) \
    u8cs name = {};     \
    if ($arglen > (i)) $mv(name, *u8cssAtP(u8csbData(STD_ARGS), i));

// Redirect stderr to file for tracing
// If ABC_TRACE env var is set to a filename, use that; otherwise
// /tmp/name-pid.err Prints path to stderr before redirecting, returns OK on
// success
fun ok64 PROStderrToFile(char const *name) {
    char const *env = getenv("ABC_TRACE");
    char path[256];
    if (env && *env && *env != '1') {
        // ABC_TRACE=filename - use as-is
        int n = snprintf(path, sizeof(path), "%s", env);
        if (n < 0 || n >= (int)sizeof(path)) return PRONOROOM;
    } else {
        // Default: /tmp/basename-pid.err
        char const *base = name;
        for (char const *p = name; *p; p++) {
            if (*p == '/') base = p + 1;
        }
        int n = snprintf(path, sizeof(path), "/tmp/%s-%d.err", base, getpid());
        if (n < 0 || n >= (int)sizeof(path)) return PRONOROOM;
    }
    fprintf(stderr, "stderr: %s\n", path);
    if (!freopen(path, "w", stderr)) return FAILSANITY;
    return OK;
}

#ifdef ABC_TRACE
#define _PRO_TRACE_INIT(name) PROStderrToFile(name)
#else
#define _PRO_TRACE_INIT(name)
#endif

// CODE-26B065z50Q: a program with its own main() puts ABC_INIT at
// file scope (the PRO globals), brackets main with ABCStart/ABCStop.
// Before ABCStart, BASS is empty: rentals fail with BNOROOM.
#define ABC_INIT                            \
    thread_local uint8_t _pro_depth = 0;    \
    u8cs *STD_ARGS[4] = {};                 \
    thread_local u8 *ABC_BASS[4] = {};

// CODE-26B065M0M4: map / unmap the calling thread's ABC_BASS; any
// thread doing BASS-carving library work (FILE, URI, LEX) needs it.
fun ok64 ABCThreadStart() {
    sane(ABC_BASS[0] == NULL);
    if (u8bMap(ABC_BASS, ABC_BASS_BYTES) != OK) {
        fprintf(stderr, "ABC_BASS u8bMap failed\n");
        return FAILSANITY;
    }
    done;
}

fun void ABCThreadStop() { u8bUnMap(ABC_BASS); }

fun ok64 ABCStart(int argn, char **args) {
    _PRO_TRACE_INIT(args[0]);
    ok64 o = ABCThreadStart();
    if (o != OK) return o;
    o = _parse_args(argn, args);
    if (o != OK) {
        fprintf(stderr, "STD_ARGS acquire failed\n");
        ABCThreadStop();
    }
    return o;
}

fun void ABCStop() { ABCThreadStop(); }

// CODE-26B065M0M4: `THREAD(name) { body }` defines a pthread entry
// `void *name(void *arg)` that brackets the ok64 body with
// ABCThreadStart/Stop and returns its ok64 as the pointer (NULL is OK).
#define THREAD(n)                                         \
    ok64 n##Body(void *arg);                              \
    void *n(void *arg) {                                  \
        ok64 o = ABCThreadStart();                        \
        if (o != OK) return (void *)(uintptr_t)o;         \
        o = n##Body(arg);                                 \
        ABCThreadStop();                                  \
        return (void *)(uintptr_t)o;                      \
    }                                                     \
    ok64 n##Body(void *arg)

#define MAIN(f)                                                          \
    ABC_INIT                                                             \
    int main(int argn, char **args) {                                    \
        if (ABCStart(argn, args) != OK) return 1;                        \
        ok64 ret = f();                                                  \
        ABCStop();                                                       \
        if (ret != OK) {                                                 \
            trace("%s<%s at %s:%i\n", PROindent, ok64str(ret), __func__, \
                  __LINE__);                                             \
            fprintf(stderr, "Error: %s\n", ok64str(ret));                \
        }                                                                \
        /* ABC-016: raw ok64 truncates to low byte, 0x??00 == "ok" */    \
        return ret == OK ? 0 : 1;                                        \
    }

#endif
