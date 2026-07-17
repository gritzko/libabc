#include "LEX.h"

#include <fcntl.h>
#include <stdio.h>

#include "BUF.h"
#include "PRO.h"

a_cstr(ext, ".lex");

ok64 lex2rl(u8cs mod, u8cs lang);

ok64 lexcli() {
    // ABC-017: sane($arglen==3) killed the usage message in debug builds
    sane(1);
    if ($arglen != 3) {
        fprintf(stderr, "Usage: lex MOD [c|go]\n");
        fail(BADARG);
    }
    a$rg(name, 1);
    a$rg(lang, 2);
    call(lex2rl, name, lang);
    done;
}

MAIN(lexcli);
