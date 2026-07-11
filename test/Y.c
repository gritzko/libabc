#include "Y.h"

#include "B.h"
#include "INT.h"
#include "OK.h"
#include "TEST.h"

// ABC-007: a Yx instance whose y-merge keeps every tied record,
// so a silently dropped record becomes visible in the output.
fun ok64 Yu64x(u8cs s, u8cs rest) { return $u8take(s, rest, sizeof(u64)); }

fun b8 Yu64z($cu8c *a, $cu8c *b) { return u64Z((u64c *)**a, (u64c *)**b); }

fun ok64 Yu64y($u8 into, u8css eqs) {
    $for(u8csc, e, eqs) {
        ok64 o = u8sFeed(into, *e);
        if (o != OK) return o;
    }
    return OK;
}

#define _X(name) Yu64##name
#include "Yx.h"
#undef _X

// ABC-007: Y_MAX_INPUTS tied runs fit the tie pad exactly;
// the merge must succeed and keep every record.
ok64 Yties64() {
    sane(1);
#define YN Y_MAX_INPUTS
    u64 vals[YN];
    aBpad2(u8cs, runs, YN);
    aBpad2(u8, out, YN * sizeof(u64));
    for (int i = 0; i < YN; ++i) {
        vals[i] = 42;
        u8cs r = {(u8c *)&vals[i], (u8c *)(&vals[i] + 1)};
        call(u8cssFeed1, runsidle, r);
    }
    call(Yu64merge, outidle, runsdata);
    want($len(outdata) == YN * sizeof(u64));
#undef YN
    done;
}

// ABC-007 repro: 65 (> Y_MAX_INPUTS) tied runs; an OK status must
// mean every record reached the output — no silent loss of the 65th.
ok64 Yties65() {
    sane(1);
#define YN (Y_MAX_INPUTS + 1)
    u64 vals[YN];
    aBpad2(u8cs, runs, YN);
    aBpad2(u8, out, YN * sizeof(u64));
    for (int i = 0; i < YN; ++i) {
        vals[i] = 42;
        u8cs r = {(u8c *)&vals[i], (u8c *)(&vals[i] + 1)};
        call(u8cssFeed1, runsidle, r);
    }
    ok64 o = Yu64merge(outidle, runsdata);
    want(o != OK || $len(outdata) == YN * sizeof(u64));
#undef YN
    done;
}

ok64 Ytest() {
    sane(1);
    call(Yties64);
    call(Yties65);
    done;
}

TEST(Ytest);
