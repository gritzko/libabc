// ANSI CSI parser (ECMA-48 / VT100).
//
// Input side of the abc/ANSI module.  The output side (escfeed,
// escfeedBG256) lives in ANSI.h as inline formatters.
#include "ANSI.h"

%%{
    machine csi;
    alphtype unsigned char;

    action priv_h  { out->private = *p; }
    action digit   { cur = cur * 10 + (u32)(*p - '0'); any = 1; }
    action sep     { CSIPush(out, cur, sub); cur = 0; sub = (*p == ':'); any = 1; }
    action inter_h { out->inter[out->ninter++] = *p; }
    action final_h { if (any) CSIPush(out, cur, sub); out->final = *p; fbreak; }

    #  STD-26B068qhvl: empty params are 0, `:` marks a sub-param (ECMA-48 5.4.2);
    #  the {0,2} bound is CSI_MAX_INTER, longer runs fail as ANSIBAD
    private = ( [<=>?] ) @priv_h ;
    params  = ( digit @digit | [;:] @sep )* ;
    inter   = ( 0x20 .. 0x2F @inter_h ){0,2} ;
    final   = ( 0x40 .. 0x7E ) @final_h ;

    main := 0x1B '[' private? params inter final ;
}%%

%% write data nofinal noerror;

//  Excess params are dropped, as before; the sequence still parses.
static void CSIPush(csip out, u32 val, b8 sub) {
    if (out->nparams >= CSI_MAX_PARAMS) return;
    if (sub) out->subs |= (u16)(1u << out->nparams);
    out->params[out->nparams++] = val;
}

ok64 ANSIu8sDrainCSI(u8cs input, csip out) {
    if (out == NULL) return ANSIBAD;
    *out = (csi){};
    if (input[0] == NULL || input[0] >= input[1]) return ANSIBAD;

    u8c *p = input[0];
    u8c *pe = input[1];
    u8c *eof = pe;
    int cs = 0;
    u32 cur = 0;
    b8 sub = 0, any = 0;

    %% write init;
    %% write exec;

    if (cs < %%{ write first_final; }%%)
        return ANSIBAD;

    input[0] = p;
    return OK;
}
