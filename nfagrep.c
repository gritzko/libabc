//
// nfagrep — grep-like tool using Thompson NFA
// Usage: nfagrep PATTERN [FILE ...]
// Reads stdin if no files given.
//

#include "NFA.h"

#include "FILE.h"
#include "PRO.h"

#define NFA_CAP 4096

//  CODE-26B065bwvG: stdin is drained into a reserved map, files are mapped:
//  an fgets buffer split lines over 8 KB in two.
#define NFA_STDIN_MAX (1UL << 36)

ok64 grep_text(nfau8cs nfa, u32 *ws[2], u8cs text, u8c *fname) {
    sane(1);
    u64 lineno = 0;
    u8cs rest = {text[0], text[1]};
    while (!$empty(rest)) {
        u8cs eol = {rest[0], rest[1]};
        u8csFind(eol, '\n');
        u8cs line = {rest[0], eol[0]};
        rest[0] = $empty(eol) ? eol[0] : eol[0] + 1;
        lineno++;
        if (!NFAu8Search(nfa, line, ws)) continue;
        int len = (int)$len(line);
        if (fname)
            printf("%s:%" PRIu64 ":%.*s\n", fname, lineno, len, line[0]);
        else
            printf("%.*s\n", len, line[0]);
    }
    done;
}

ok64 nfagrep() {
    // ABC-017: sane($arglen>=2) killed the usage message in debug builds
    sane(1);
    if ($arglen < 2) {
        fprintf(stderr, "usage: nfagrep PATTERN [FILE ...]\n");
        fail(NFABADSYN);
    }

    a$rg(pat, 1);

    nfau8 buf[NFA_CAP];
    nfau8g prog = {buf, buf + NFA_CAP, buf};
    u32 pbuf[NFA_CAP * 2];
    u32 *pws[2] = {pbuf, pbuf + NFA_CAP * 2};

    ok64 o = NFAu8Compile(prog, pat, pws);
    if (o != OK) {
        fprintf(stderr, "nfagrep: bad pattern: %s\n", ok64str(o));
        return o;
    }

    nfau8cs nfa = {prog[2], prog[0]};
    u32 wbuf[NFA_CAP * 3];
    u32 *ws[2] = {wbuf, wbuf + NFA_CAP * 3};

    if ($arglen <= 2) {
        u8b in = {};
        call(u8bMap, in, NFA_STDIN_MAX);
        try(FILEdrainall, u8bIdle(in), STDIN_FILENO);
        then try(grep_text, nfa, ws, u8bDataC(in), NULL);
        u8bUnMap(in);
        nedo return __;
    } else {
        for (int i = 2; i < $arglen; i++) {
            a$rg(fn, i);
            // ABC-017: argv slices are NUL-terminated (PRO.h) — use them
            // directly; the 256-byte snprintf copy truncated long paths
            const char *fname = (const char *)fn[0];
            u8b map = {};
            ok64 mo = FILEMapOnce(map, fn);
            if (mo != OK) {
                fprintf(stderr, "nfagrep: %s: %s\n", fname, ok64str(mo));
                continue;
            }
            try(grep_text, nfa, ws, u8bDataC(map), (u8c *)fname);
            FILEUnMapOnce(map);
            nedo return __;
        }
    }

    done;
}

MAIN(nfagrep);
