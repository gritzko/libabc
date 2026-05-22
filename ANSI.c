//  ANSI SGR delta emitter.  Header has the inline accessors and the
//  short ANSIu8sFeedReset; the per-cell delta speller lives here so
//  PRO.h's sane()/done can guard the slice and the function bodies
//  don't get inlined into every translation unit that just needs the
//  ansi64 type.
#include "ANSI.h"

#include <fcntl.h>
#include <poll.h>
#include <termios.h>
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

//  OSC 11 bg-color probe with process-wide cache.  States:
//    0 = unprobed; 1 = probed, no reply / parse failure (cached_err
//        holds the code to return); 2 = probed OK (cached_bg holds it).
static u8     ansi_bg_state    = 0;
static ansi64 ansi_bg_cached   = 0;
static ok64   ansi_bg_cached_err = OK;

//  Parse one '/'-separated 16-bit hex channel; returns 0..255 (high
//  byte) or -1 on bad input.  Accepts 1..4 hex digits per channel
//  (xterm spec; most terminals emit 4).
static int ansi_parse_hex16(u8c **pp, u8c *end) {
    u8c *p = *pp;
    u32 v = 0;
    int n = 0;
    while (p < end && n < 4) {
        u8 c = *p;
        u32 d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else break;
        v = (v << 4) | d;
        p++; n++;
    }
    if (n == 0) return -1;
    *pp = p;
    //  Scale to high byte: 1-digit → v<<4, 2-digit → v, 3-digit →
    //  v>>4, 4-digit → v>>8.  All converge on an 8-bit channel.
    if (n == 1) return (int)((v << 4) & 0xFF);
    if (n == 2) return (int)(v & 0xFF);
    if (n == 3) return (int)((v >> 4) & 0xFF);
    return (int)((v >> 8) & 0xFF);
}

ok64 ANSIBgColor(ansi64 *bg) {
    sane(bg != NULL);
    if (ansi_bg_state != 0) {
        *bg = ansi_bg_cached;
        return ansi_bg_cached_err;
    }
    ansi_bg_state = 1;            //  default to "probe-and-failed"
    ansi_bg_cached = ANSI_DEFAULT;
    ansi_bg_cached_err = ANSINOREPLY;

    int fd = open("/dev/tty", O_RDWR | O_NOCTTY);
    if (fd < 0) {
        ansi_bg_cached_err = ANSINOTTY;
        *bg = ANSI_DEFAULT;
        fail(ANSINOTTY);
    }

    struct termios old_tio, raw_tio;
    if (tcgetattr(fd, &old_tio) != 0) {
        close(fd);
        ansi_bg_cached_err = ANSINOTTY;
        *bg = ANSI_DEFAULT;
        fail(ANSINOTTY);
    }
    raw_tio = old_tio;
    raw_tio.c_lflag &= ~(ICANON | ECHO);
    raw_tio.c_cc[VMIN]  = 0;
    raw_tio.c_cc[VTIME] = 0;
    (void)tcsetattr(fd, TCSANOW, &raw_tio);

    static u8c const QUERY[] = "\033]11;?\007";
    ssize_t wn = write(fd, QUERY, sizeof(QUERY) - 1);
    (void)wn;

    //  Drain up to 64 bytes within 100 ms total (one tight loop in
    //  case the terminal dribbles the reply across reads).
    u8 buf[64];
    int n = 0;
    int budget_ms = 100;
    while (n < (int)sizeof(buf)) {
        struct pollfd pfd = { .fd = fd, .events = POLLIN };
        int pr = poll(&pfd, 1, budget_ms);
        if (pr <= 0) break;
        ssize_t r = read(fd, buf + n, sizeof(buf) - (size_t)n);
        if (r <= 0) break;
        n += (int)r;
        //  Stop once we've seen BEL or ST terminator.
        if (buf[n - 1] == 0x07) break;
        if (n >= 2 && buf[n - 2] == 0x1B && buf[n - 1] == '\\') break;
        budget_ms = 30;             //  shorter wait for trailing bytes
    }

    (void)tcsetattr(fd, TCSANOW, &old_tio);
    close(fd);

    //  Expect: ESC ] 11 ; rgb : RRRR / GGGG / BBBB  (ST | BEL)
    if (n < 14) { *bg = ANSI_DEFAULT; fail(ANSINOREPLY); }
    u8c *p = buf, *end = buf + n;
    if (p[0] != 0x1B || p[1] != ']') { *bg = ANSI_DEFAULT; fail(ANSINOREPLY); }
    p += 2;
    while (p < end && *p != ';') p++;
    if (p >= end) { *bg = ANSI_DEFAULT; fail(ANSINOREPLY); }
    p++;
    //  "rgb:" prefix
    if (end - p < 4 ||
        p[0] != 'r' || p[1] != 'g' || p[2] != 'b' || p[3] != ':') {
        *bg = ANSI_DEFAULT; fail(ANSINOREPLY);
    }
    p += 4;
    int r = ansi_parse_hex16(&p, end);
    if (r < 0 || p >= end || *p != '/') { *bg = ANSI_DEFAULT; fail(ANSINOREPLY); }
    p++;
    int g = ansi_parse_hex16(&p, end);
    if (g < 0 || p >= end || *p != '/') { *bg = ANSI_DEFAULT; fail(ANSINOREPLY); }
    p++;
    int b = ansi_parse_hex16(&p, end);
    if (b < 0) { *bg = ANSI_DEFAULT; fail(ANSINOREPLY); }

    u32 rgb = ((u32)(r & 0xFF) << 16) | ((u32)(g & 0xFF) << 8) | (u32)(b & 0xFF);
    ansi64 packed = ANSI64_PACK(0, ANSI_MODE_DEFAULT, rgb, ANSI_MODE_RGB, 0);
    ansi_bg_cached = packed;
    ansi_bg_cached_err = OK;
    ansi_bg_state = 2;
    *bg = packed;
    done;
}
