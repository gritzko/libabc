#include "PATH.h"

#include <limits.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "PRO.h"
#include "UTF8.h"

// Internal scratch-buffer size for in-place Norm/Abs/Join.
#define PATH_SCRATCH_LEN 4096

// --- Slice validators ---

ok64 PATHu8sVerifySegment(u8cs segment) {
    if (!$ok(segment)) return PATHBAD;
    a_dup(u8c, check, segment);
    if (utf8sValid(check) != OK) return PATHBAD;
    $for(u8c, c, segment) {
        if (*c == '\r' || *c == '\t' || *c == '\n' || *c == '\0' || *c == '/')
            return PATHBAD;
    }
    return OK;
}

ok64 PATHu8sVerify(u8cs path) {
    sane($ok(path));
    a_dup(u8c, check, path);
    test(utf8sValid(check) == OK, PATHBAD);
    $for(u8c, c, path) {
        test(*c != '\r' && *c != '\t' && *c != '\n' && *c != '\0', PATHBAD);
    }
    done;
}

// --- Slice extractors ---

void PATHu8sBase(u8csp out, u8cs path) {
    if (!$ok(path) || $empty(path)) {
        out[0] = out[1] = NULL;
        return;
    }
    u8cp p = path[1];
    while (p > path[0] && *(p - 1) != '/') p--;
    out[0] = p;
    out[1] = path[1];
}

// Immutable "." for the no-slash-but-non-empty case. Callers treat
// the view as read-only (u8cs is const-value), so the aliasing is safe.
static u8c PATH_DOT[1] = {'.'};

void PATHu8sDir(u8csp out, u8cs path) {
    if (!$ok(path) || $empty(path)) {
        out[0] = out[1] = NULL;
        return;
    }
    u8cp p = path[1];
    while (p > path[0] && *(p - 1) != '/') p--;
    if (p == path[0]) {
        // No slash: synthesize "." view.
        out[0] = PATH_DOT;
        out[1] = PATH_DOT + 1;
        return;
    }
    // Drop trailing slash unless root
    u8cp end = p - 1;
    if (end == path[0] && *path[0] == '/') end++;
    out[0] = path[0];
    out[1] = end;
}

void PATHu8sExt(u8csp out, u8cs path) {
    out[0] = out[1] = NULL;
    if (!$ok(path) || $empty(path)) return;
    u8cp bstart = path[1];
    while (bstart > path[0] && *(bstart - 1) != '/') bstart--;
    if (bstart >= path[1]) return;
    u8cp p = path[1];
    while (p > bstart && *(p - 1) != '.') p--;
    if (p <= bstart || p == bstart + 1) return;
    out[0] = p;
    out[1] = path[1];
}

// --- Slice iteration ---

ok64 PATHu8sDrain(u8cs cursor, u8csp seg) {
    sane($ok(cursor) && seg != NULL);
    if ($empty(cursor)) return END;
    if (*cursor[0] == '/') cursor[0]++;
    if ($empty(cursor)) return END;
    u8cp start = cursor[0];
    while (!$empty(cursor) && *cursor[0] != '/') cursor[0]++;
    seg[0] = start;
    seg[1] = cursor[0];
    done;
}

ok64 PATHu8sDrainNE(u8cs cursor, u8csp seg) {
    for (;;) {
        ok64 o = PATHu8sDrain(cursor, seg);
        if (o != OK) return o;
        if (seg[1] > seg[0]) return OK;
    }
}

// --- Buffer composition ---

ok64 PATHu8bFeed(path8b p, u8cs data) {
    sane(u8bOK(p) && $ok(data));
    call(PATHu8sVerify, data);
    u8sp idle = u8bIdle(p);
    test($len(idle) > $len(data), PATHNOROOM);
    call(u8sFeed, idle, data);
    call(PATHu8bTerm, p);
    done;
}

ok64 PATHu8bPush(path8b p, u8cs segment) {
    sane(u8bOK(p) && $ok(segment));
    call(PATHu8sVerifySegment, segment);
    u8sp idle = u8bIdle(p);
    // Insert separator if DATA non-empty and doesn't end with '/'.
    if (u8bDataLen(p) > 0) {
        u8c last = *(u8bIdleHead(p) - 1);
        if (last != '/') {
            test($len(idle) > 1, PATHNOROOM);
            call(u8sFeed1, idle, '/');
        }
    }
    test($len(idle) > $len(segment), PATHNOROOM);
    call(u8sFeed, idle, segment);
    call(PATHu8bTerm, p);
    done;
}

ok64 PATHu8bPop(path8b p) {
    sane(u8bOK(p));
    if (u8bDataLen(p) == 0) done;
    u8p end = u8bIdleHead(p);
    u8p head = u8bDataHead(p);
    while (end > head && *(end - 1) != '/') end--;
    if (end > head) end--;  // Eat the separator
    // Root case: keep at least '/'
    if (end == head && u8bDataLen(p) > 0 && *head == '/') end++;
    *u8bIdle(p) = end;
    call(PATHu8bTerm, p);
    done;
}

ok64 PATHu8bDup(path8b into, u8cs src) {
    sane(u8bOK(into) && $ok(src));
    u8bReset(into);
    call(PATHu8bFeed, into, src);
    done;
}

ok64 PATHu8bAdd(path8b into, u8cs rel) {
    sane(u8bOK(into) && $ok(rel));
    a_dup(u8c, rem, rel);
    u8cs seg = {};
    while (PATHu8sDrain(rem, seg) == OK) {
        if ($empty(seg)) continue;  // tolerate double-slashes in input
        call(PATHu8bPush, into, seg);
    }
    call(PATHu8bTerm, into);
    done;
}

ok64 PATHu8bJoin(path8b out, u8cs a, u8cs b) {
    sane(u8bOK(out) && $ok(a) && $ok(b));
    u8bReset(out);
    if (!$empty(a)) call(PATHu8bFeed, out, a);
    if (!$empty(b)) call(PATHu8bAdd, out, b);
    // Normalize in place
    a_pad(u8, tmp, PATH_SCRATCH_LEN);
    call(PATHu8bDup, tmp, u8bDataC(out));
    call(PATHu8bNorm, out, u8bDataC(tmp));
    done;
}

ok64 PATHu8bAddTmp(path8b p, u8cs tmpl) {
    sane(u8bOK(p) && $ok(tmpl) && !$empty(tmpl));
    u8sp idle = u8bIdle(p);
    // Add separator if DATA non-empty and doesn't end with '/'.
    if (u8bDataLen(p) > 0) {
        u8c last = *(u8bIdleHead(p) - 1);
        if (last != '/') {
            test($len(idle) > 1, PATHNOROOM);
            call(u8sFeed1, idle, '/');
        }
    }
    u8p start = *idle;
    test($len(idle) > $len(tmpl), PATHNOROOM);
    call(u8sFeed, idle, tmpl);
    call(PATHu8bTerm, p);
    // Randomize 'X' chars via LCG seeded by pid^time
    static u32 seed = 0;
    if (seed == 0) seed = (u32)getpid() ^ (u32)time(NULL);
    for (u8p q = start; q < *idle; q++) {
        if (*q == 'X') {
            seed = seed * 1103515245 + 12345;
            *q = "abcdefghijklmnopqrstuvwxyz0123456789"[(seed >> 16) % 36];
        }
    }
    done;
}

ok64 PATHu8bNorm(path8b out, u8cs orig) {
    sane(u8bOK(out) && $ok(orig));

    // Collect segments (views into `orig`) resolving . and ..
    u8cp segs[256][2];
    u64 n = 0;
    b8 is_abs = PATHu8sIsAbsolute(orig);

    a_dup(u8c, rem, orig);
    u8cs seg = {};
    while (PATHu8sDrain(rem, seg) == OK) {
        if ($empty(seg)) continue;
        if ($len(seg) == 1 && seg[0][0] == '.') continue;
        if ($len(seg) == 2 && seg[0][0] == '.' && seg[0][1] == '.') {
            if (n > 0) {
                u8cp ps = segs[n - 1][0], pe = segs[n - 1][1];
                b8 prev_dd = (pe - ps == 2 && ps[0] == '.' && ps[1] == '.');
                if (!prev_dd) { n--; continue; }
            }
            if (!is_abs) {
                test(n < 256, PATHFAIL);
                segs[n][0] = seg[0]; segs[n][1] = seg[1]; n++;
            }
            continue;
        }
        test(n < 256, PATHFAIL);
        segs[n][0] = seg[0]; segs[n][1] = seg[1]; n++;
    }

    // Build result into `out`.  When out aliases orig, the collected
    // segment pointers still reference orig's original storage; we must
    // avoid overwriting before we've read — so stage into scratch.
    a_pad(u8, tmp, PATH_SCRATCH_LEN);
    u8sp ti = u8bIdle(tmp);
    if (is_abs) call(u8sFeed1, ti, '/');
    for (u64 i = 0; i < n; i++) {
        if (i > 0) call(u8sFeed1, ti, '/');
        u8cs si = {segs[i][0], segs[i][1]};
        call(u8sFeed, ti, si);
    }
    if (u8bDataLen(tmp) == 0) call(u8sFeed1, ti, '.');
    call(PATHu8bTerm, tmp);

    u8bReset(out);
    call(u8bFeed, out, u8bDataC(tmp));
    call(PATHu8bTerm, out);
    done;
}

ok64 PATHu8bAbs(path8b out, u8cs base, u8cs rel) {
    sane(u8bOK(out) && $ok(base) && $ok(rel));
    a_pad(u8, tmp, PATH_SCRATCH_LEN);
    if (PATHu8sIsAbsolute(rel)) {
        call(PATHu8bFeed, tmp, rel);
    } else {
        call(PATHu8bFeed, tmp, base);
        call(PATHu8bAdd, tmp, rel);
    }
    u8bReset(out);
    call(PATHu8bNorm, out, u8bDataC(tmp));
    done;
}

ok64 PATHu8bRel(path8b out, u8cs absbase, u8cs abs) {
    sane(u8bOK(out) && $ok(absbase) && $ok(abs));
    test(PATHu8sIsAbsolute(absbase) && PATHu8sIsAbsolute(abs), PATHBAD);

    u8bReset(out);

    // Identical paths
    if ($len(absbase) == $len(abs) && 0 == $cmp(absbase, abs)) {
        u8c dot[1] = {'.'};
        u8cs dot_seg = {dot, dot + 1};
        call(PATHu8bPush, out, dot_seg);
        done;
    }

    // abs starts with absbase as a directory prefix
    size_t base_len = $len(absbase);
    u8cs abs_prefix = {abs[0], abs[0] + base_len};
    if ($len(abs) > base_len && $eq(absbase, abs_prefix) &&
        (absbase[1][-1] == '/' || abs[0][base_len] == '/')) {
        u8cp start = abs[0] + base_len;
        if (*start == '/') start++;
        u8cs remainder = {start, abs[1]};
        call(PATHu8bFeed, out, remainder);
        done;
    }

    // Skip common prefix segments
    a_dup(u8c, brem, absbase);
    a_dup(u8c, arem, abs);
    u8cs bs = {}, as = {};
    ok64 bo = OK, ao = OK;
    for (;;) {
        bo = PATHu8sDrainNE(brem, bs);
        ao = PATHu8sDrainNE(arem, as);
        if (bo != OK || ao != OK) break;
        if ($len(bs) != $len(as)) break;
        if (0 != $cmp(bs, as)) break;
    }

    u64 ups = (bo == OK) ? 1 : 0;
    u8cs skip = {};
    while (PATHu8sDrainNE(brem, skip) == OK) ups++;

    if (ao != OK) as[0] = as[1] = NULL;

    u8c dotdot[2] = {'.', '.'};
    u8cs dotdot_seg = {dotdot, dotdot + 2};
    for (u64 i = 0; i < ups; i++) call(PATHu8bPush, out, dotdot_seg);

    if (!$empty(as)) call(PATHu8bPush, out, as);
    while (PATHu8sDrainNE(arem, as) == OK) call(PATHu8bPush, out, as);

    if (u8bDataLen(out) == 0) {
        u8c dot[1] = {'.'};
        u8cs dot_seg = {dot, dot + 1};
        call(PATHu8bPush, out, dot_seg);
    }

    call(PATHu8bTerm, out);
    done;
}

ok64 PATHu8bReal(path8b out, u8cs path) {
    sane(u8bOK(out) && $ok(path));
    test(!$empty(path), PATHBAD);
    // path's NUL-terminator sits at path[1][0] by contract.
    char resolved[PATH_MAX];
    if (realpath((char const *)path[0], resolved) == NULL) return PATHFAIL;
    u8bReset(out);
    size_t rlen = 0;
    while (resolved[rlen] != 0 && rlen < PATH_MAX) rlen++;
    u8cs res = {(u8cp)resolved, (u8cp)resolved + rlen};
    call(PATHu8bFeed, out, res);
    done;
}

ok64 PATHu8bBuildN(path8b p, u8c *const **slices) {
    sane(u8bOK(p));
    if (!*slices) return OK;
    u8cs first = {(*slices)[0], (*slices)[1]};
    call(PATHu8bFeed, p, first);
    for (slices++; *slices; slices++) {
        // Each slice may itself be a sub-path (e.g. "dir/sub"); add it
        // segment-wise so '/' inside is handled as a separator, not a
        // forbidden char.  Empty-segment tolerance matches PATHu8bAdd.
        u8cs s = {(*slices)[0], (*slices)[1]};
        call(PATHu8bAdd, p, s);
    }
    done;
}
