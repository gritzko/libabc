#ifndef ABC_LEX_H
#define ABC_LEX_H

#include "BUF.h"

con ok64 LEXBADARG = 0x153a12ca34a6d0;
con ok64 LEXFULL = 0x153a13de555;
con ok64 LEXBAD = 0x54e84b28d;

#define LEXenum 0

#define LEX_TEMPL_C 0
#define LEX_TEMPL_GO 1
#define LEX_TEMPL_LANG_LEN 2

#define LEX_TEMPL_LEN 10

extern const u8c *LEX_TEMPL[LEX_TEMPL_LANG_LEN][LEX_TEMPL_LEN][2];

typedef struct {
    u8cs data;
    int lang;
    u8bp ct;

    u8c$ mod;

    u8cs cur;
    u32 ruleno;
} LEXstate;

ok64 LEXLexer(LEXstate *state);

#endif
