//  ANSI SGR delta emitter.  Header has the inline accessors and the
//  short ANSIu8sFeedReset; the per-cell delta speller lives here so
//  PRO.h's sane()/done can guard the slice and the function bodies
//  don't get inlined into every translation unit that just needs the
//  ansi64 type.
#include "ANSI.h"

#include <unistd.h>

#include "OK.h"
#include "PRO.h"

//  Cached isatty(STDOUT_FILENO).  0 = unprobed; 1 = NO; 2 = YES.
//  Process-global; one probe at first use, overridable via ANSISetTTY.
static u8 ansi_tty_cache = 0;

b8 ANSIIsTTY(void) {
    if (ansi_tty_cache == 0)
        ansi_tty_cache = isatty(STDOUT_FILENO) ? 2 : 1;
    return ansi_tty_cache == 2 ? YES : NO;
}

void ANSISetTTY(b8 v) { ansi_tty_cache = v ? 2 : 1; }

//  Append the colour-mode portion of an SGR sequence.  `kind` is '3'
//  for fg or '4' for bg; it picks 38;5;N / 48;5;N / 38;2;R;G;B etc.
//  Caller has already written the leading `ESC [` (and any separator).
static void ansi_feed_color(u8s out, u8 kind, u8 mode, u32 val) {
    if (mode == ANSI_MODE_BASIC) {
        utf8sFeed10(out, val);
    } else if (mode == ANSI_MODE_256) {
        u8sFeed1(out, kind); u8sFeed1(out, '8'); u8sFeed1(out, ';');
        u8sFeed1(out, '5'); u8sFeed1(out, ';');
        utf8sFeed10(out, val & 0xFFu);
    } else if (mode == ANSI_MODE_RGB) {
        u8sFeed1(out, kind); u8sFeed1(out, '8'); u8sFeed1(out, ';');
        u8sFeed1(out, '2'); u8sFeed1(out, ';');
        utf8sFeed10(out, (val >> 16) & 0xFFu); u8sFeed1(out, ';');
        utf8sFeed10(out, (val >>  8) & 0xFFu); u8sFeed1(out, ';');
        utf8sFeed10(out,  val        & 0xFFu);
    } else {
        //  DEFAULT: 39 (fg) / 49 (bg)
        utf8sFeed10(out, (kind == '3') ? 39u : 49u);
    }
}

//  Per-flag on/off SGR codes.  Bold and Faint share the off code 22:
//  callers turning off only one of them must re-emit the on code for
//  the other.  BRO currently exercises BOLD and REVERSE only.
static u8 ansi_flag_on(u8 bit) {
    switch (bit) {
    case ANSI_BOLD:      return 1;
    case ANSI_FAINT:     return 2;
    case ANSI_ITALIC:    return 3;
    case ANSI_UNDERLINE: return 4;
    case ANSI_BLINK:     return 5;
    case ANSI_REVERSE:   return 7;
    case ANSI_STRIKE:    return 9;
    default:             return 0;
    }
}
static u8 ansi_flag_off(u8 bit) {
    switch (bit) {
    case ANSI_BOLD:      return 22;
    case ANSI_FAINT:     return 22;
    case ANSI_ITALIC:    return 23;
    case ANSI_UNDERLINE: return 24;
    case ANSI_BLINK:     return 25;
    case ANSI_REVERSE:   return 27;
    case ANSI_STRIKE:    return 29;
    default:             return 0;
    }
}

ok64 ANSIu8sFeedDelta(u8s out, ansi64 want, ansi64 prev) {
    sane(u8sOK(out));
    if (want == prev) done;
    if (u8sLen(out) < 64) fail(NOROOM);

    u32 wfg = ansi64Fg(want),     pfg = ansi64Fg(prev);
    u8  wfm = ansi64FgMode(want), pfm = ansi64FgMode(prev);
    u32 wbg = ansi64Bg(want),     pbg = ansi64Bg(prev);
    u8  wbm = ansi64BgMode(want), pbm = ansi64BgMode(prev);
    u8  wfl = ansi64Flags(want),  pfl = ansi64Flags(prev);

    u8sFeed1(out, 033);
    u8sFeed1(out, '[');
    b8 first = YES;

    //  Flag transitions: off-codes first, then on-codes — keeps any
    //  shared off-code from clobbering a flag we're enabling.
    u8 turn_off = pfl & ~wfl;
    u8 turn_on  = wfl & ~pfl;
    for (u8 b = 1; b; b <<= 1) {
        if (turn_off & b) {
            if (!first) u8sFeed1(out, ';');
            utf8sFeed10(out, ansi_flag_off(b));
            first = NO;
        }
    }
    for (u8 b = 1; b; b <<= 1) {
        if (turn_on & b) {
            if (!first) u8sFeed1(out, ';');
            utf8sFeed10(out, ansi_flag_on(b));
            first = NO;
        }
    }
    if (wfg != pfg || wfm != pfm) {
        if (!first) u8sFeed1(out, ';');
        ansi_feed_color(out, '3', wfm, wfg);
        first = NO;
    }
    if (wbg != pbg || wbm != pbm) {
        if (!first) u8sFeed1(out, ';');
        ansi_feed_color(out, '4', wbm, wbg);
        first = NO;
    }
    if (first) u8sFeed1(out, '0');  //  shouldn't happen given want != prev
    u8sFeed1(out, 'm');
    done;
}
