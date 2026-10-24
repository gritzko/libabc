#ifndef LIBRDX_TEST_H
#define LIBRDX_TEST_H

#include <assert.h>

#include "OK.h"
#include "PRO.h"

con ok64 TESTFAIL = 0x74e71d3ca495;
con ok64 TESTFAILEQ = 0x74e71d3ca49539a;

// CODE-26A24BvaL1: a failed check names itself on stderr, ABC_TRACE or not
#define want(cond)                                                     \
    if (!(cond)) {                                                     \
        fprintf(stderr, "%s:%i: %s: want(%s) failed\n", __FILE__,      \
                __LINE__, __func__, #cond);                            \
        fail(TESTFAIL);                                                \
    }

#define same(a, b)                                                     \
    if ((a) != (b)) {                                                  \
        fprintf(stderr, "%s:%i: %s: same(%s, %s) failed\n", __FILE__,  \
                __LINE__, __func__, #a, #b);                           \
        fail(TESTFAILEQ);                                              \
    }

#define TEST(f)                                                       \
    uint8_t _pro_depth = 0;                                           \
    u8cs *STD_ARGS[4] = {};                                           \
    thread_local u8 *ABC_BASS[4] = {};                               \
    int main(int argn, char **args) {                                 \
        _PRO_TRACE_INIT(args[0]);                                     \
        if (u8bMap(ABC_BASS, ABC_BASS_BYTES) != OK) {                 \
            fprintf(stderr, "ABC_BASS u8bMap failed\n");              \
            return 1;                                                 \
        }                                                             \
        (void)argn; (void)args;                                       \
        ok64 ret = f();                                               \
        u8bUnMap(ABC_BASS);                                           \
        if (ret != OK && PROfailed.file && PROfailed.code == ret) {   \
            fprintf(stderr, "%s<%s in %s at %s:%i: %s\ntest fail\n",   \
                    PROindent, ok64str(ret), #f, PROfailed.file,      \
                    PROfailed.line, PROfailed.func);                  \
        } else if (ret != OK) {                                       \
            fprintf(stderr, "%s<%s in %s\ntest fail\n", PROindent,     \
                    ok64str(ret), #f);                                \
        }                                                             \
        /* ABC-016: raw ok64 truncates to low byte, 0x??00 == "ok" */ \
        return ret == OK ? 0 : 1;                                     \
    }

#define FUZZ(T, n)                                                 \
    ok64 n($##T##c input);                                         \
    uint8_t _pro_depth = 0;                                        \
    thread_local u8 *ABC_BASS[4] = {};                            \
    int LLVMFuzzerInitialize(int *argc, char ***argv) {            \
        (void)argc; (void)argv;                                    \
        if (u8bMap(ABC_BASS, ABC_BASS_BYTES) != OK) return 1;      \
        return 0;                                                  \
    }                                                              \
    int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) { \
        T const *p = (T const *)Data;                              \
        $##T##c data = {p, p + (Size / sizeof(T))};                \
        ok64 o = n(data);                                          \
        if (o!=OK) __builtin_trap();                                           \
        return 0;                                                  \
    }                                                              \
    ok64 n($##T##c input)
#endif
