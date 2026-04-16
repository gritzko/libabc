// ANSI CSI parser (ECMA-48 / VT100).
//
// Input side of the abc/ANSI module.  The output side (escfeed,
// escfeedBG256) lives in ANSI.h as inline formatters.
#include "ANSI.h"

%%{
    machine csi;
    alphtype unsigned char;

    action priv_h    { out->private = *p; }
    action digit     { cur = cur * 10 + (u32)(*p - '0'); }
    action param_end {
        if (out->nparams < CSI_MAX_PARAMS)
            out->params[out->nparams++] = cur;
        cur = 0;
    }
    action final_h   { out->final = *p; fbreak; }

    private = ( [<=>?] ) @priv_h ;
    param   = ( digit @digit )+ %param_end ;
    params  = param ( ';' param )* ;
    final   = ( 0x40 .. 0x7E ) @final_h ;

    main := 0x1B '[' private? params? final ;
}%%

%% write data nofinal noerror;

ok64 ANSIu8sDrainCSI(u8cs input, csip out) {
    if (out == NULL) return ANSIBAD;
    *out = (csi){};
    if (input[0] == NULL || input[0] >= input[1]) return ANSIBAD;

    u8c *p = input[0];
    u8c *pe = input[1];
    u8c *eof = pe;
    int cs = 0;
    u32 cur = 0;

    %% write init;
    %% write exec;

    if (cs < %%{ write first_final; }%%)
        return ANSIBAD;

    input[0] = p;
    return OK;
}
