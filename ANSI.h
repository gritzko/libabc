#ifndef ABC_ANSI_H
#define ABC_ANSI_H

#include "BUF.h"

con ok64 ANSIBAD = 0x1d524b28d;

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

#endif
