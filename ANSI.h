#ifndef ABC_ANSI_H
#define ABC_ANSI_H

#include "BUF.h"

con ok64 ANSIBAD = 0xa5dc48b28d;

// --- CSI parser (input side) ---
//
// Control Sequence Introducer (ECMA-48): ESC '[' [private] [params] final
//   private : optional 0x3C..0x3F prefix ('<' '=' '>' '?')
//   params  : optional decimal integers separated by ';'
//   final   : single byte 0x40..0x7E (the operation code)
//
// Examples:
//   "\033[H"          → final='H', no params  (cursor home)
//   "\033[5n"         → final='n', params={5}  (status request)
//   "\033[12;40H"     → final='H', params={12,40}  (cursor move)
//   "\033[<0;5;10M"   → private='<', final='M', params={0,5,10}  (SGR mouse)

#define CSI_MAX_PARAMS 8

typedef struct {
    u8  private;                  // private prefix byte, or 0
    u8  final;                    // final byte
    u8  nparams;                  // count of decoded params
    u32 params[CSI_MAX_PARAMS];   // decoded decimal values
} csi;

typedef csi *csip;
typedef csi const *csicp;

// Parse one CSI sequence from the head of `input`.  Advances input[0]
// past the sequence on success.  Returns ANSIBAD on malformed input or
// truncated sequence; the caller may need more bytes and retry.
ok64 ANSIu8sDrainCSI(u8cs input, csip out);

typedef enum {
    BOLD = 1,
    WEAK = 2,
    HIGHLIGHT = 3,
    UNDERLINE = 4,
    STRIKETHROUGH = 9,
    BLACK = 30,
    DARK_RED = 31,
    DARK_GREEN = 32,
    DARK_YELLOW = 33,
    DARK_BLUE = 34,
    DARK_PINK = 35,
    DARK_CYAN = 36,
    BLACK_BG = 40,
    DARK_RED_BG = 41,
    DARK_GREEN_BG = 42,
    DARK_YELLOW_BG = 43,
    DARK_BLUE_BG = 44,
    DARK_PINK_BG = 45,
    DARK_CYAN_BG = 46,
    GRAY = 90,
    LIGHT_RED = 91,
    LIGHT_GREEN = 92,
    LIGHT_YELLOW = 93,
    LIGHT_BLUE = 94,
    LIGHT_PINK = 95,
    LIGHT_CYAN = 96,
    LIGHT_GRAY = 97,
    GRAY_BG = 100,
    LIGHT_RED_BG = 101,
    LIGHT_GREEN_BG = 102,
    LIGHT_YELLOW_BG = 103,
    LIGHT_BLUE_BG = 104,
    LIGHT_PINK_BG = 105,
    LIGHT_CYAN_BG = 106,
    LIGHT_GRAY_BG = 107,
} ANSI_COLOR;

fun ok64 escfeed($u8 data, u8 esc) {
    if (!$ok(data) || $size(data) < 7) return BADARG;
    u8sFeed1(data, 033);  //"\033[91m"
    u8sFeed1(data, '[');
    utf8sFeed10(data, esc);
    u8sFeed1(data, 'm');
    return OK;
}

// 256-color background: \033[48;5;Nm
fun ok64 escfeedBG256($u8 data, u8 color) {
    if (!$ok(data) || $size(data) < 12) return BADARG;
    u8sFeed1(data, 033);
    u8sFeed1(data, '[');
    u8sFeed1(data, '4');
    u8sFeed1(data, '8');
    u8sFeed1(data, ';');
    u8sFeed1(data, '5');
    u8sFeed1(data, ';');
    utf8sFeed10(data, color);
    u8sFeed1(data, 'm');
    return OK;
}

// --- SGR state (output side) ---
//
// Packed display-state for one screen cell.  Renderers feed
// ANSIu8sFeedDelta(out, want, prev) at every cell whose state differs
// from the previously-emitted one — runs of identical-style cells
// share a single open SGR; one final ANSIu8sFeedReset closes the row.
//
//   bits  0..23 (24): fg color value (palette idx or 0xRRGGBB)
//   bits 24..27 ( 4): fg mode
//   bits 28..51 (24): bg color value
//   bits 52..55 ( 4): bg mode
//   bits 56..63 ( 8): attr flags (ANSI_BOLD | ANSI_REVERSE | ...)
//
// Mode 0 means "default" (terminal's own fg/bg) — color value is then
// ignored.  Mode 1 stores a basic 30..37 / 90..97 SGR code directly
// (so the emitter just writes the number).  Modes 2 and 3 are reserved
// for 256-color and RGB; the layout has the 24 bits to hold them so
// callers can move to truecolor without changing the type.
typedef u64 ansi64;

#define ANSI_MODE_DEFAULT  0u
#define ANSI_MODE_BASIC    1u
#define ANSI_MODE_256      2u
#define ANSI_MODE_RGB      3u

#define ANSI_BOLD       0x01u
#define ANSI_FAINT      0x02u
#define ANSI_ITALIC     0x04u
#define ANSI_UNDERLINE  0x08u
#define ANSI_BLINK      0x10u
#define ANSI_REVERSE    0x20u
#define ANSI_STRIKE     0x40u

#define ANSI_DEFAULT  ((ansi64)0)

fun ansi64 ansi64Pack(u32 fg, u8 fg_mode, u32 bg, u8 bg_mode, u8 flags) {
    return ((u64)(fg & 0xFFFFFFu))
         | ((u64)(fg_mode & 0xFu) << 24)
         | ((u64)(bg & 0xFFFFFFu) << 28)
         | ((u64)(bg_mode & 0xFu) << 52)
         | ((u64)flags << 56);
}
fun u32 ansi64Fg     (ansi64 s) { return (u32)(s & 0xFFFFFFu); }
fun u8  ansi64FgMode (ansi64 s) { return (u8)((s >> 24) & 0xFu); }
fun u32 ansi64Bg     (ansi64 s) { return (u32)((s >> 28) & 0xFFFFFFu); }
fun u8  ansi64BgMode (ansi64 s) { return (u8)((s >> 52) & 0xFu); }
fun u8  ansi64Flags  (ansi64 s) { return (u8)(s >> 56); }

// Emit a delta SGR carrying only the attributes that transitioned
// from `prev` to `want`.  No-op when they match.  The caller maintains
// the current state (one ansi64 per output stream); this function
// neither reads nor writes that state — it just spells the delta.
// Returns BADARG when the slice is malformed or has < 64 bytes idle
// (worst case is ~40 bytes for RGB fg + RGB bg + 7 flag transitions).
ok64 ANSIu8sFeedDelta(u8s out, ansi64 want, ansi64 prev);

// Close any open attrs with `\033[0m`.  No-op when state is default.
fun ok64 ANSIu8sFeedReset(u8s out, ansi64 cur) {
    if (cur == ANSI_DEFAULT) return OK;
    return escfeed(out, 0);
}

#endif
