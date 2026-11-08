
/* #line 1 "URI.c.rl" */
#include "INT.h"
#include "PRO.h"
#include "URI.h"

// action indices for the parser
#define URIenum 0
enum {
	URISegment = URIenum+12,
	URISegment_nz = URIenum+13,
	URIPath = URIenum+16,
	URIPathNoscheme = URIenum+18,
	URIScheme = URIenum+20,
	URIIPv4address = URIenum+22,
	URIIPvFuture = URIenum+23,
	URIIPv6address = URIenum+24,
	URIIP_literal = URIenum+25,
	URIUser = URIenum+27,
	URIHost = URIenum+28,
	URIPort = URIenum+29,
	URIAuthority = URIenum+30,
	URIFragment = URIenum+31,
	URIQuery = URIenum+32,
	URIPathRootless = URIenum+33,
	URIURI = URIenum+37,
	URIRoot = URIenum+38,
};

// user functions (callbacks) for the parser
ok64 URIonSegment (u8cs tok, URIstate* state);
ok64 URIonSegment_nz (u8cs tok, URIstate* state);
ok64 URIonPath (u8cs tok, URIstate* state);
ok64 URIonPathNoscheme (u8cs tok, URIstate* state);
ok64 URIonScheme (u8cs tok, URIstate* state);
ok64 URIonIPv4address (u8cs tok, URIstate* state);
ok64 URIonIPvFuture (u8cs tok, URIstate* state);
ok64 URIonIPv6address (u8cs tok, URIstate* state);
ok64 URIonIP_literal (u8cs tok, URIstate* state);
ok64 URIonUser (u8cs tok, URIstate* state);
ok64 URIonHost (u8cs tok, URIstate* state);
ok64 URIonPort (u8cs tok, URIstate* state);
ok64 URIonAuthority (u8cs tok, URIstate* state);
ok64 URIonFragment (u8cs tok, URIstate* state);
ok64 URIonQuery (u8cs tok, URIstate* state);
ok64 URIonPathRootless (u8cs tok, URIstate* state);
ok64 URIonURI (u8cs tok, URIstate* state);
ok64 URIonRoot (u8cs tok, URIstate* state);




/* #line 262 "URI.c.rl" */



/* #line 53 "URI.rl.c" */
static const char _URI_actions[] = {
	0, 1, 0, 1, 1, 1, 3, 1, 
	7, 1, 9, 1, 10, 1, 12, 1, 
	13, 1, 14, 1, 15, 1, 19, 1, 
	20, 1, 21, 1, 22, 1, 26, 1, 
	28, 1, 29, 2, 0, 1, 2, 1, 
	5, 2, 1, 7, 2, 1, 31, 2, 
	3, 31, 2, 11, 15, 2, 11, 21, 
	2, 17, 21, 2, 18, 19, 2, 18, 
	20, 2, 19, 22, 2, 20, 10, 2, 
	20, 16, 2, 20, 21, 2, 21, 25, 
	2, 23, 25, 2, 28, 29, 2, 34, 
	32, 3, 0, 1, 5, 3, 0, 1, 
	7, 3, 0, 1, 31, 3, 7, 33, 
	35, 3, 9, 24, 4, 3, 9, 30, 
	2, 3, 9, 33, 35, 3, 11, 21, 
	25, 3, 17, 21, 25, 3, 18, 20, 
	10, 3, 18, 20, 21, 3, 20, 21, 
	25, 3, 21, 23, 25, 3, 21, 25, 
	4, 3, 23, 25, 4, 3, 27, 33, 
	35, 3, 29, 33, 35, 3, 34, 32, 
	6, 4, 1, 5, 33, 35, 4, 1, 
	7, 33, 35, 4, 1, 31, 33, 35, 
	4, 3, 31, 33, 35, 4, 11, 21, 
	25, 4, 4, 17, 21, 25, 4, 4, 
	20, 21, 23, 25, 4, 20, 21, 25, 
	4, 4, 21, 23, 25, 4, 4, 21, 
	25, 33, 35, 4, 23, 25, 33, 35, 
	4, 26, 27, 33, 35, 4, 28, 29, 
	33, 35, 4, 34, 32, 8, 6, 4, 
	34, 32, 24, 4, 4, 34, 32, 33, 
	35, 5, 0, 1, 5, 33, 35, 5, 
	0, 1, 7, 33, 35, 5, 0, 1, 
	31, 33, 35, 5, 11, 21, 25, 33, 
	35, 5, 17, 21, 25, 33, 35, 5, 
	20, 21, 23, 25, 4, 5, 20, 21, 
	25, 33, 35, 5, 21, 23, 25, 33, 
	35, 6, 20, 21, 23, 25, 33, 35
	
};

static const short _URI_key_offsets[] = {
	0, 0, 6, 12, 18, 24, 30, 36, 
	42, 48, 54, 60, 66, 72, 78, 84, 
	98, 112, 118, 124, 131, 139, 148, 150, 
	153, 155, 158, 160, 163, 166, 167, 180, 
	186, 192, 198, 204, 210, 216, 222, 228, 
	234, 240, 243, 244, 247, 248, 254, 261, 
	274, 288, 294, 300, 306, 312, 318, 324, 
	330, 336, 342, 348, 354, 360, 366, 372, 
	378, 384, 390, 396, 402, 408, 414, 420, 
	437, 453, 464, 475, 489, 503, 517, 529, 
	541, 555, 569, 583, 603, 619, 634, 649, 
	664, 679, 694, 714, 734, 753, 773, 792, 
	812, 831, 850, 869, 885, 905, 922, 942, 
	959, 979, 996, 1000, 1014, 1028, 1042, 1056, 
	1059, 1079, 1099, 1115, 1135, 1150, 1170, 1186, 
	1202, 1218, 1238, 1258, 1277, 1297, 1317, 1336, 
	1356, 1375, 1395, 1415, 1434, 1454, 1471, 1491, 
	1511, 1531, 1548, 1569, 1583, 1597, 1611
};

static const unsigned char _URI_trans_keys[] = {
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 33u, 37u, 61u, 64u, 
	95u, 126u, 36u, 46u, 48u, 59u, 65u, 90u, 
	97u, 122u, 33u, 37u, 61u, 64u, 95u, 126u, 
	36u, 46u, 48u, 59u, 65u, 90u, 97u, 122u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 118u, 48u, 58u, 65u, 
	70u, 97u, 102u, 58u, 93u, 48u, 57u, 65u, 
	70u, 97u, 102u, 46u, 58u, 93u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 46u, 48u, 
	57u, 48u, 57u, 46u, 48u, 57u, 48u, 57u, 
	93u, 48u, 57u, 93u, 48u, 57u, 93u, 33u, 
	37u, 61u, 95u, 126u, 36u, 46u, 48u, 59u, 
	64u, 90u, 97u, 122u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	46u, 48u, 57u, 46u, 46u, 48u, 57u, 46u, 
	48u, 57u, 65u, 70u, 97u, 102u, 46u, 48u, 
	57u, 65u, 70u, 97u, 102u, 33u, 36u, 61u, 
	95u, 126u, 38u, 46u, 48u, 59u, 65u, 90u, 
	97u, 122u, 33u, 36u, 61u, 93u, 95u, 126u, 
	38u, 46u, 48u, 59u, 65u, 90u, 97u, 122u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 33u, 35u, 37u, 46u, 
	47u, 59u, 61u, 63u, 64u, 95u, 126u, 36u, 
	57u, 65u, 90u, 97u, 122u, 33u, 35u, 37u, 
	46u, 47u, 59u, 61u, 63u, 95u, 126u, 36u, 
	57u, 64u, 90u, 97u, 122u, 33u, 37u, 61u, 
	95u, 126u, 36u, 59u, 63u, 90u, 97u, 122u, 
	33u, 37u, 61u, 95u, 126u, 36u, 59u, 63u, 
	90u, 97u, 122u, 33u, 35u, 37u, 47u, 61u, 
	63u, 95u, 126u, 36u, 59u, 64u, 90u, 97u, 
	122u, 33u, 35u, 37u, 47u, 61u, 63u, 95u, 
	126u, 36u, 59u, 64u, 90u, 97u, 122u, 33u, 
	35u, 37u, 47u, 61u, 63u, 95u, 126u, 36u, 
	59u, 64u, 90u, 97u, 122u, 33u, 35u, 37u, 
	61u, 95u, 126u, 36u, 59u, 63u, 90u, 97u, 
	122u, 33u, 35u, 37u, 61u, 95u, 126u, 36u, 
	59u, 63u, 90u, 97u, 122u, 33u, 35u, 37u, 
	47u, 61u, 63u, 95u, 126u, 36u, 59u, 64u, 
	90u, 97u, 122u, 33u, 35u, 37u, 47u, 61u, 
	63u, 95u, 126u, 36u, 59u, 64u, 90u, 97u, 
	122u, 33u, 35u, 37u, 47u, 61u, 63u, 95u, 
	126u, 36u, 59u, 64u, 90u, 97u, 122u, 33u, 
	35u, 37u, 47u, 58u, 59u, 61u, 63u, 64u, 
	91u, 95u, 126u, 36u, 46u, 48u, 57u, 65u, 
	90u, 97u, 122u, 33u, 35u, 37u, 47u, 58u, 
	61u, 63u, 64u, 95u, 126u, 36u, 59u, 65u, 
	90u, 97u, 122u, 33u, 35u, 37u, 47u, 61u, 
	63u, 64u, 95u, 126u, 36u, 59u, 65u, 90u, 
	97u, 122u, 33u, 35u, 37u, 47u, 61u, 63u, 
	64u, 95u, 126u, 36u, 59u, 65u, 90u, 97u, 
	122u, 33u, 35u, 37u, 47u, 61u, 63u, 64u, 
	95u, 126u, 36u, 59u, 65u, 90u, 97u, 122u, 
	33u, 35u, 37u, 47u, 61u, 63u, 64u, 95u, 
	126u, 36u, 59u, 65u, 90u, 97u, 122u, 33u, 
	35u, 37u, 47u, 61u, 63u, 64u, 95u, 126u, 
	36u, 59u, 65u, 90u, 97u, 122u, 33u, 35u, 
	37u, 47u, 58u, 59u, 61u, 63u, 64u, 91u, 
	95u, 126u, 36u, 46u, 48u, 57u, 65u, 90u, 
	97u, 122u, 33u, 35u, 37u, 46u, 47u, 58u, 
	59u, 61u, 63u, 64u, 95u, 126u, 36u, 45u, 
	48u, 57u, 65u, 90u, 97u, 122u, 33u, 35u, 
	37u, 47u, 58u, 59u, 61u, 63u, 64u, 95u, 
	126u, 36u, 46u, 48u, 57u, 65u, 90u, 97u, 
	122u, 33u, 35u, 37u, 46u, 47u, 58u, 59u, 
	61u, 63u, 64u, 95u, 126u, 36u, 45u, 48u, 
	57u, 65u, 90u, 97u, 122u, 33u, 35u, 37u, 
	47u, 58u, 59u, 61u, 63u, 64u, 95u, 126u, 
	36u, 46u, 48u, 57u, 65u, 90u, 97u, 122u, 
	33u, 35u, 37u, 46u, 47u, 58u, 59u, 61u, 
	63u, 64u, 95u, 126u, 36u, 45u, 48u, 57u, 
	65u, 90u, 97u, 122u, 33u, 35u, 37u, 47u, 
	58u, 59u, 61u, 63u, 64u, 95u, 126u, 36u, 
	46u, 48u, 57u, 65u, 90u, 97u, 122u, 33u, 
	35u, 37u, 47u, 58u, 59u, 61u, 63u, 64u, 
	95u, 126u, 36u, 46u, 48u, 57u, 65u, 90u, 
	97u, 122u, 33u, 35u, 37u, 47u, 58u, 59u, 
	61u, 63u, 64u, 95u, 126u, 36u, 46u, 48u, 
	57u, 65u, 90u, 97u, 122u, 33u, 35u, 37u, 
	47u, 58u, 61u, 63u, 64u, 95u, 126u, 36u, 
	59u, 65u, 90u, 97u, 122u, 33u, 35u, 37u, 
	46u, 47u, 58u, 59u, 61u, 63u, 64u, 95u, 
	126u, 36u, 45u, 48u, 57u, 65u, 90u, 97u, 
	122u, 33u, 35u, 37u, 46u, 47u, 58u, 61u, 
	63u, 64u, 95u, 126u, 36u, 59u, 65u, 90u, 
	97u, 122u, 33u, 35u, 37u, 46u, 47u, 58u, 
	59u, 61u, 63u, 64u, 95u, 126u, 36u, 45u, 
	48u, 57u, 65u, 90u, 97u, 122u, 33u, 35u, 
	37u, 46u, 47u, 58u, 61u, 63u, 64u, 95u, 
	126u, 36u, 59u, 65u, 90u, 97u, 122u, 33u, 
	35u, 37u, 46u, 47u, 58u, 59u, 61u, 63u, 
	64u, 95u, 126u, 36u, 45u, 48u, 57u, 65u, 
	90u, 97u, 122u, 33u, 35u, 37u, 46u, 47u, 
	58u, 61u, 63u, 64u, 95u, 126u, 36u, 59u, 
	65u, 90u, 97u, 122u, 35u, 47u, 58u, 63u, 
	33u, 35u, 37u, 47u, 61u, 63u, 95u, 126u, 
	36u, 59u, 64u, 90u, 97u, 122u, 33u, 35u, 
	37u, 47u, 61u, 63u, 95u, 126u, 36u, 59u, 
	64u, 90u, 97u, 122u, 33u, 35u, 37u, 47u, 
	61u, 63u, 95u, 126u, 36u, 59u, 64u, 90u, 
	97u, 122u, 33u, 35u, 37u, 47u, 61u, 63u, 
	95u, 126u, 36u, 59u, 64u, 90u, 97u, 122u, 
	35u, 47u, 63u, 33u, 35u, 37u, 47u, 58u, 
	59u, 61u, 63u, 64u, 91u, 95u, 126u, 36u, 
	46u, 48u, 57u, 65u, 90u, 97u, 122u, 33u, 
	35u, 37u, 47u, 58u, 59u, 61u, 63u, 64u, 
	91u, 95u, 126u, 36u, 46u, 48u, 57u, 65u, 
	90u, 97u, 122u, 33u, 35u, 37u, 47u, 58u, 
	61u, 63u, 64u, 95u, 126u, 36u, 59u, 65u, 
	90u, 97u, 122u, 33u, 35u, 37u, 46u, 47u, 
	58u, 59u, 61u, 63u, 64u, 95u, 126u, 36u, 
	45u, 48u, 57u, 65u, 90u, 97u, 122u, 33u, 
	35u, 37u, 47u, 61u, 63u, 64u, 95u, 126u, 
	36u, 59u, 65u, 90u, 97u, 122u, 33u, 35u, 
	37u, 47u, 58u, 59u, 61u, 63u, 64u, 91u, 
	95u, 126u, 36u, 46u, 48u, 57u, 65u, 90u, 
	97u, 122u, 33u, 35u, 37u, 47u, 58u, 61u, 
	63u, 64u, 95u, 126u, 36u, 59u, 65u, 90u, 
	97u, 122u, 33u, 35u, 37u, 47u, 58u, 61u, 
	63u, 64u, 95u, 126u, 36u, 59u, 65u, 90u, 
	97u, 122u, 33u, 35u, 37u, 47u, 58u, 61u, 
	63u, 64u, 95u, 126u, 36u, 59u, 65u, 90u, 
	97u, 122u, 33u, 35u, 37u, 47u, 58u, 59u, 
	61u, 63u, 64u, 91u, 95u, 126u, 36u, 46u, 
	48u, 57u, 65u, 90u, 97u, 122u, 33u, 35u, 
	37u, 46u, 47u, 58u, 59u, 61u, 63u, 64u, 
	95u, 126u, 36u, 45u, 48u, 57u, 65u, 90u, 
	97u, 122u, 33u, 35u, 37u, 47u, 58u, 59u, 
	61u, 63u, 64u, 95u, 126u, 36u, 46u, 48u, 
	57u, 65u, 90u, 97u, 122u, 33u, 35u, 37u, 
	46u, 47u, 58u, 59u, 61u, 63u, 64u, 95u, 
	126u, 36u, 45u, 48u, 57u, 65u, 90u, 97u, 
	122u, 33u, 35u, 37u, 46u, 47u, 58u, 59u, 
	61u, 63u, 64u, 95u, 126u, 36u, 45u, 48u, 
	57u, 65u, 90u, 97u, 122u, 33u, 35u, 37u, 
	47u, 58u, 59u, 61u, 63u, 64u, 95u, 126u, 
	36u, 46u, 48u, 57u, 65u, 90u, 97u, 122u, 
	33u, 35u, 37u, 46u, 47u, 58u, 59u, 61u, 
	63u, 64u, 95u, 126u, 36u, 45u, 48u, 57u, 
	65u, 90u, 97u, 122u, 33u, 35u, 37u, 47u, 
	58u, 59u, 61u, 63u, 64u, 95u, 126u, 36u, 
	46u, 48u, 57u, 65u, 90u, 97u, 122u, 33u, 
	35u, 37u, 46u, 47u, 58u, 59u, 61u, 63u, 
	64u, 95u, 126u, 36u, 45u, 48u, 57u, 65u, 
	90u, 97u, 122u, 33u, 35u, 37u, 46u, 47u, 
	58u, 59u, 61u, 63u, 64u, 95u, 126u, 36u, 
	45u, 48u, 57u, 65u, 90u, 97u, 122u, 33u, 
	35u, 37u, 47u, 58u, 59u, 61u, 63u, 64u, 
	95u, 126u, 36u, 46u, 48u, 57u, 65u, 90u, 
	97u, 122u, 33u, 35u, 37u, 46u, 47u, 58u, 
	59u, 61u, 63u, 64u, 95u, 126u, 36u, 45u, 
	48u, 57u, 65u, 90u, 97u, 122u, 33u, 35u, 
	37u, 46u, 47u, 58u, 61u, 63u, 64u, 95u, 
	126u, 36u, 59u, 65u, 90u, 97u, 122u, 33u, 
	35u, 37u, 47u, 58u, 59u, 61u, 63u, 64u, 
	91u, 95u, 126u, 36u, 46u, 48u, 57u, 65u, 
	90u, 97u, 122u, 33u, 35u, 37u, 46u, 47u, 
	58u, 59u, 61u, 63u, 64u, 95u, 126u, 36u, 
	45u, 48u, 57u, 65u, 90u, 97u, 122u, 33u, 
	35u, 37u, 46u, 47u, 58u, 59u, 61u, 63u, 
	64u, 95u, 126u, 36u, 45u, 48u, 57u, 65u, 
	90u, 97u, 122u, 33u, 35u, 37u, 46u, 47u, 
	58u, 61u, 63u, 64u, 95u, 126u, 36u, 59u, 
	65u, 90u, 97u, 122u, 33u, 35u, 37u, 43u, 
	46u, 47u, 58u, 59u, 61u, 63u, 64u, 95u, 
	126u, 36u, 44u, 45u, 57u, 65u, 90u, 97u, 
	122u, 33u, 35u, 37u, 47u, 61u, 63u, 95u, 
	126u, 36u, 59u, 64u, 90u, 97u, 122u, 33u, 
	35u, 37u, 47u, 61u, 63u, 95u, 126u, 36u, 
	59u, 64u, 90u, 97u, 122u, 33u, 35u, 37u, 
	47u, 61u, 63u, 95u, 126u, 36u, 59u, 64u, 
	90u, 97u, 122u, 33u, 35u, 37u, 47u, 61u, 
	63u, 95u, 126u, 36u, 59u, 64u, 90u, 97u, 
	122u, 0
};

static const char _URI_single_lengths[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 6, 
	6, 0, 0, 1, 2, 3, 0, 1, 
	0, 1, 0, 1, 1, 1, 5, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 1, 1, 1, 0, 1, 5, 
	6, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 11, 
	10, 5, 5, 8, 8, 8, 6, 6, 
	8, 8, 8, 12, 10, 9, 9, 9, 
	9, 9, 12, 12, 11, 12, 11, 12, 
	11, 11, 11, 10, 12, 11, 12, 11, 
	12, 11, 4, 8, 8, 8, 8, 3, 
	12, 12, 10, 12, 9, 12, 10, 10, 
	10, 12, 12, 11, 12, 12, 11, 12, 
	11, 12, 12, 11, 12, 11, 12, 12, 
	12, 11, 13, 8, 8, 8, 8
};

static const char _URI_range_lengths[] = {
	0, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 3, 3, 3, 4, 
	4, 3, 3, 3, 3, 3, 1, 1, 
	1, 1, 1, 1, 1, 0, 4, 3, 
	3, 3, 3, 3, 3, 3, 3, 3, 
	3, 1, 0, 1, 0, 3, 3, 4, 
	4, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 4, 3, 3, 3, 3, 
	3, 3, 4, 4, 4, 4, 4, 4, 
	4, 4, 4, 3, 4, 3, 4, 3, 
	4, 3, 0, 3, 3, 3, 3, 0, 
	4, 4, 3, 4, 3, 4, 3, 3, 
	3, 4, 4, 4, 4, 4, 4, 4, 
	4, 4, 4, 4, 4, 3, 4, 4, 
	4, 3, 4, 3, 3, 3, 3
};

static const short _URI_index_offsets[] = {
	0, 0, 4, 8, 12, 16, 20, 24, 
	28, 32, 36, 40, 44, 48, 52, 56, 
	67, 78, 82, 86, 91, 97, 104, 106, 
	109, 111, 114, 116, 119, 122, 124, 134, 
	138, 142, 146, 150, 154, 158, 162, 166, 
	170, 174, 177, 179, 182, 184, 188, 193, 
	203, 214, 218, 222, 226, 230, 234, 238, 
	242, 246, 250, 254, 258, 262, 266, 270, 
	274, 278, 282, 286, 290, 294, 298, 302, 
	317, 331, 340, 349, 361, 373, 385, 395, 
	405, 417, 429, 441, 458, 472, 485, 498, 
	511, 524, 537, 554, 571, 587, 604, 620, 
	637, 653, 669, 685, 699, 716, 731, 748, 
	763, 780, 795, 800, 812, 824, 836, 848, 
	852, 869, 886, 900, 917, 930, 947, 961, 
	975, 989, 1006, 1023, 1039, 1056, 1073, 1089, 
	1106, 1122, 1139, 1156, 1172, 1189, 1204, 1221, 
	1238, 1255, 1270, 1288, 1300, 1312, 1324
};

static const unsigned char _URI_indicies[] = {
	0, 0, 0, 1, 2, 2, 2, 1, 
	3, 3, 3, 1, 4, 4, 4, 1, 
	5, 5, 5, 1, 6, 6, 6, 1, 
	7, 7, 7, 1, 8, 8, 8, 1, 
	9, 9, 9, 1, 10, 10, 10, 1, 
	11, 11, 11, 1, 12, 12, 12, 1, 
	13, 13, 13, 1, 14, 14, 14, 1, 
	15, 16, 15, 17, 15, 15, 15, 15, 
	15, 15, 1, 18, 19, 18, 20, 18, 
	18, 18, 18, 18, 18, 1, 21, 21, 
	21, 1, 18, 18, 18, 1, 23, 22, 
	22, 22, 1, 25, 26, 24, 25, 25, 
	1, 27, 25, 26, 24, 25, 25, 1, 
	28, 1, 29, 30, 1, 31, 1, 32, 
	33, 1, 34, 1, 36, 35, 1, 36, 
	37, 1, 36, 1, 38, 39, 38, 38, 
	38, 38, 38, 38, 38, 1, 40, 40, 
	40, 1, 41, 41, 41, 1, 42, 42, 
	42, 1, 43, 43, 43, 1, 44, 44, 
	44, 1, 45, 45, 45, 1, 46, 46, 
	46, 1, 47, 47, 47, 1, 48, 48, 
	48, 1, 49, 49, 49, 1, 32, 50, 
	1, 32, 1, 29, 51, 1, 29, 1, 
	52, 52, 52, 1, 53, 52, 52, 52, 
	1, 54, 54, 54, 54, 54, 54, 54, 
	54, 54, 1, 54, 54, 54, 55, 54, 
	54, 54, 54, 54, 54, 1, 56, 56, 
	56, 1, 57, 57, 57, 1, 58, 58, 
	58, 1, 59, 59, 59, 1, 60, 60, 
	60, 1, 61, 61, 61, 1, 62, 62, 
	62, 1, 63, 63, 63, 1, 64, 64, 
	64, 1, 65, 65, 65, 1, 66, 66, 
	66, 1, 67, 67, 67, 1, 68, 68, 
	68, 1, 69, 69, 69, 1, 70, 70, 
	70, 1, 71, 71, 71, 1, 72, 72, 
	72, 1, 73, 73, 73, 1, 74, 74, 
	74, 1, 75, 75, 75, 1, 76, 76, 
	76, 1, 77, 77, 77, 1, 78, 79, 
	80, 81, 82, 78, 78, 83, 78, 78, 
	78, 78, 84, 84, 1, 4, 85, 86, 
	6, 87, 4, 4, 88, 4, 4, 4, 
	4, 4, 1, 89, 90, 89, 89, 89, 
	89, 89, 89, 1, 2, 91, 2, 2, 
	2, 2, 2, 2, 1, 6, 85, 92, 
	87, 6, 88, 6, 6, 6, 6, 6, 
	1, 93, 94, 95, 96, 93, 97, 93, 
	93, 93, 93, 93, 1, 8, 98, 99, 
	100, 8, 101, 8, 8, 8, 8, 8, 
	1, 102, 103, 104, 102, 102, 102, 102, 
	102, 102, 1, 10, 105, 106, 10, 10, 
	10, 10, 10, 10, 1, 107, 108, 109, 
	110, 107, 111, 107, 107, 107, 107, 107, 
	1, 12, 112, 113, 114, 12, 115, 12, 
	12, 12, 12, 12, 1, 107, 108, 109, 
	116, 107, 111, 107, 107, 107, 107, 107, 
	1, 117, 118, 119, 120, 122, 117, 117, 
	123, 124, 125, 117, 117, 117, 121, 117, 
	117, 1, 14, 126, 127, 128, 129, 14, 
	130, 20, 14, 14, 14, 14, 14, 1, 
	73, 131, 132, 133, 73, 134, 135, 73, 
	73, 73, 73, 73, 1, 71, 131, 136, 
	133, 71, 134, 137, 71, 71, 71, 71, 
	71, 1, 59, 131, 138, 133, 59, 134, 
	139, 59, 59, 59, 59, 59, 1, 57, 
	131, 140, 133, 57, 134, 141, 57, 57, 
	57, 57, 57, 1, 18, 131, 19, 133, 
	18, 134, 20, 18, 18, 18, 18, 18, 
	1, 142, 118, 143, 120, 145, 142, 142, 
	123, 20, 125, 142, 142, 142, 144, 142, 
	142, 1, 14, 126, 127, 146, 128, 129, 
	14, 14, 130, 20, 14, 14, 14, 147, 
	14, 14, 1, 14, 126, 127, 128, 129, 
	14, 14, 130, 20, 14, 14, 14, 148, 
	14, 14, 1, 14, 126, 127, 149, 128, 
	129, 14, 14, 130, 20, 14, 14, 14, 
	150, 14, 14, 1, 14, 126, 127, 128, 
	129, 14, 14, 130, 20, 14, 14, 14, 
	151, 14, 14, 1, 14, 126, 127, 152, 
	128, 129, 14, 14, 130, 20, 14, 14, 
	14, 153, 14, 14, 1, 14, 126, 127, 
	128, 129, 14, 14, 130, 20, 14, 14, 
	14, 154, 14, 14, 1, 14, 155, 127, 
	156, 158, 14, 14, 159, 20, 14, 14, 
	14, 157, 14, 14, 1, 14, 155, 127, 
	156, 158, 14, 14, 159, 20, 14, 14, 
	14, 160, 14, 14, 1, 14, 155, 127, 
	156, 158, 14, 159, 20, 14, 14, 14, 
	14, 14, 1, 14, 126, 127, 152, 128, 
	129, 14, 14, 130, 20, 14, 14, 14, 
	161, 14, 14, 1, 14, 126, 127, 152, 
	128, 129, 14, 130, 20, 14, 14, 14, 
	14, 14, 1, 14, 126, 127, 149, 128, 
	129, 14, 14, 130, 20, 14, 14, 14, 
	162, 14, 14, 1, 14, 126, 127, 149, 
	128, 129, 14, 130, 20, 14, 14, 14, 
	14, 14, 1, 14, 126, 127, 146, 128, 
	129, 14, 14, 130, 20, 14, 14, 14, 
	163, 14, 14, 1, 14, 126, 127, 146, 
	128, 129, 14, 130, 20, 14, 14, 14, 
	14, 14, 1, 164, 165, 166, 167, 1, 
	47, 131, 168, 133, 47, 134, 47, 47, 
	47, 47, 47, 1, 45, 131, 169, 133, 
	45, 134, 45, 45, 45, 45, 45, 1, 
	43, 131, 170, 133, 43, 134, 43, 43, 
	43, 43, 43, 1, 41, 131, 171, 133, 
	41, 134, 41, 41, 41, 41, 41, 1, 
	131, 133, 134, 1, 142, 172, 143, 173, 
	145, 142, 142, 174, 20, 125, 142, 142, 
	142, 144, 142, 142, 1, 175, 172, 176, 
	173, 178, 175, 175, 174, 141, 125, 175, 
	175, 175, 177, 175, 175, 1, 14, 179, 
	127, 180, 129, 14, 181, 20, 14, 14, 
	14, 14, 14, 1, 14, 179, 127, 146, 
	180, 129, 14, 14, 181, 20, 14, 14, 
	14, 147, 14, 14, 1, 15, 131, 16, 
	133, 15, 134, 17, 15, 15, 15, 15, 
	15, 1, 182, 172, 183, 173, 178, 182, 
	182, 174, 135, 125, 182, 182, 182, 184, 
	182, 182, 1, 67, 179, 185, 180, 186, 
	67, 181, 137, 67, 67, 67, 67, 67, 
	1, 65, 179, 187, 180, 186, 65, 181, 
	139, 65, 65, 65, 65, 65, 1, 61, 
	179, 188, 180, 186, 61, 181, 141, 61, 
	61, 61, 61, 61, 1, 189, 172, 190, 
	173, 178, 189, 189, 174, 139, 125, 189, 
	189, 189, 191, 189, 189, 1, 61, 179, 
	188, 192, 180, 186, 61, 61, 181, 141, 
	61, 61, 61, 193, 61, 61, 1, 14, 
	179, 127, 180, 129, 14, 14, 181, 20, 
	14, 14, 14, 148, 14, 14, 1, 14, 
	179, 127, 146, 180, 129, 14, 14, 181, 
	20, 14, 14, 14, 163, 14, 14, 1, 
	67, 179, 185, 194, 180, 186, 67, 67, 
	181, 137, 67, 67, 67, 195, 67, 67, 
	1, 65, 179, 187, 180, 186, 65, 65, 
	181, 139, 65, 65, 65, 196, 65, 65, 
	1, 61, 179, 188, 197, 180, 186, 61, 
	61, 181, 141, 61, 61, 61, 198, 61, 
	61, 1, 14, 179, 127, 180, 129, 14, 
	14, 181, 20, 14, 14, 14, 151, 14, 
	14, 1, 14, 179, 127, 149, 180, 129, 
	14, 14, 181, 20, 14, 14, 14, 162, 
	14, 14, 1, 65, 179, 187, 199, 180, 
	186, 65, 65, 181, 139, 65, 65, 65, 
	200, 65, 65, 1, 61, 179, 188, 180, 
	186, 61, 61, 181, 141, 61, 61, 61, 
	201, 61, 61, 1, 14, 179, 127, 149, 
	180, 129, 14, 14, 181, 20, 14, 14, 
	14, 150, 14, 14, 1, 61, 179, 188, 
	192, 180, 186, 61, 181, 141, 61, 61, 
	61, 61, 61, 1, 202, 172, 203, 173, 
	178, 202, 202, 174, 137, 125, 202, 202, 
	202, 204, 202, 202, 1, 65, 179, 187, 
	199, 180, 186, 65, 65, 181, 139, 65, 
	65, 65, 205, 65, 65, 1, 61, 179, 
	188, 192, 180, 186, 61, 61, 181, 141, 
	61, 61, 61, 206, 61, 61, 1, 14, 
	179, 127, 146, 180, 129, 14, 181, 20, 
	14, 14, 14, 14, 14, 1, 4, 85, 
	86, 207, 6, 87, 208, 4, 4, 88, 
	4, 4, 4, 4, 207, 207, 207, 1, 
	209, 210, 211, 212, 209, 213, 209, 209, 
	209, 209, 209, 1, 75, 214, 215, 216, 
	75, 217, 75, 75, 75, 75, 75, 1, 
	218, 219, 220, 221, 218, 222, 218, 218, 
	218, 218, 218, 1, 77, 223, 224, 225, 
	77, 226, 77, 77, 77, 77, 77, 1, 
	0
};

static const unsigned char _URI_trans_targs[] = {
	2, 0, 74, 4, 72, 6, 75, 8, 
	77, 10, 79, 12, 81, 14, 84, 85, 
	55, 117, 16, 17, 90, 18, 20, 45, 
	21, 20, 106, 22, 23, 24, 43, 25, 
	26, 41, 27, 28, 106, 29, 107, 39, 
	32, 111, 34, 110, 36, 109, 38, 108, 
	40, 107, 42, 44, 46, 47, 48, 106, 
	50, 89, 52, 88, 54, 114, 56, 85, 
	58, 120, 60, 119, 62, 118, 64, 87, 
	66, 86, 68, 140, 70, 142, 72, 73, 
	3, 75, 80, 78, 138, 73, 3, 76, 
	78, 74, 1, 1, 5, 77, 73, 7, 
	76, 78, 73, 7, 76, 78, 79, 73, 
	9, 73, 9, 81, 73, 11, 83, 78, 
	73, 11, 82, 78, 82, 84, 73, 13, 
	82, 91, 15, 78, 90, 19, 73, 13, 
	82, 15, 78, 73, 65, 82, 78, 134, 
	63, 121, 51, 113, 49, 112, 84, 13, 
	91, 15, 92, 104, 93, 94, 102, 95, 
	96, 100, 97, 73, 82, 98, 15, 78, 
	99, 101, 103, 105, 73, 82, 30, 78, 
	37, 35, 33, 31, 73, 82, 78, 114, 
	53, 115, 116, 73, 82, 78, 118, 61, 
	125, 59, 116, 57, 53, 120, 57, 122, 
	123, 124, 126, 130, 127, 128, 129, 131, 
	133, 132, 119, 59, 135, 136, 137, 138, 
	139, 140, 73, 67, 80, 78, 73, 67, 
	141, 78, 142, 73, 69, 141, 78, 73, 
	69, 141, 78
};

static const short _URI_trans_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 27, 
	27, 65, 0, 0, 21, 0, 17, 13, 
	11, 0, 19, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 50, 0, 27, 27, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 15, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 157, 86, 
	157, 157, 231, 86, 226, 7, 0, 0, 
	7, 29, 29, 0, 0, 1, 93, 1, 
	35, 93, 41, 0, 3, 41, 31, 83, 
	31, 33, 0, 1, 89, 1, 0, 89, 
	38, 0, 3, 38, 35, 62, 133, 62, 
	196, 125, 129, 133, 59, 71, 77, 0, 
	141, 25, 77, 80, 0, 145, 80, 21, 
	0, 21, 0, 21, 0, 21, 23, 23, 
	68, 74, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 117, 181, 0, 53, 117, 
	0, 0, 0, 0, 121, 186, 56, 121, 
	0, 0, 0, 0, 191, 271, 191, 23, 
	23, 68, 74, 137, 201, 137, 23, 23, 
	68, 0, 25, 0, 0, 23, 23, 68, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 23, 23, 68, 0, 0, 0, 
	0, 109, 9, 109, 105, 9, 47, 0, 
	5, 47, 1, 97, 1, 35, 97, 44, 
	0, 3, 44
};

static const short _URI_eof_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 236, 
	101, 216, 149, 101, 247, 166, 221, 153, 
	241, 161, 241, 277, 206, 211, 211, 211, 
	211, 211, 277, 206, 206, 206, 206, 206, 
	206, 259, 259, 259, 206, 206, 206, 206, 
	206, 206, 265, 211, 211, 211, 211, 211, 
	289, 289, 283, 283, 211, 289, 283, 283, 
	283, 289, 283, 283, 283, 283, 283, 283, 
	283, 283, 283, 283, 283, 283, 289, 283, 
	283, 283, 101, 113, 176, 253, 171
};

static const int URI_start = 71;
static const int URI_first_final = 71;
static const int URI_error = 0;

static const int URI_en_main = 71;


/* #line 265 "URI.c.rl" */

// the public API function
ok64 URILexer(URIstate* state) {

    a_dup(u8c, data, state->data);
    sane($ok(data));

    int cs = 0;
    u8c *p = (u8c*) data[0];
    u8c *pe = (u8c*) data[1];
    u8c *eof = pe;
    u64 mark0[64] = {};
    ok64 o = OK;

    u8cs tok = {p, p};

    
/* #line 663 "URI.rl.c" */
	{
	cs = URI_start;
	}

/* #line 282 "URI.c.rl" */
    
/* #line 666 "URI.rl.c" */
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const unsigned char *_keys;

	if ( p == pe )
		goto _test_eof;
	if ( cs == 0 )
		goto _out;
_resume:
	_keys = _URI_trans_keys + _URI_key_offsets[cs];
	_trans = _URI_index_offsets[cs];

	_klen = _URI_single_lengths[cs];
	if ( _klen > 0 ) {
		const unsigned char *_lower = _keys;
		const unsigned char *_mid;
		const unsigned char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*p) < *_mid )
				_upper = _mid - 1;
			else if ( (*p) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (unsigned int)(_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _URI_range_lengths[cs];
	if ( _klen > 0 ) {
		const unsigned char *_lower = _keys;
		const unsigned char *_mid;
		const unsigned char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*p) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*p) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += (unsigned int)((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
	_trans = _URI_indicies[_trans];
	cs = _URI_trans_targs[_trans];

	if ( _URI_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _URI_actions + _URI_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
/* #line 57 "URI.c.rl" */
	{ mark0[URISegment] = p - data[0]; }
	break;
	case 1:
/* #line 58 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URISegment];
    tok[1] = p;
    o = URIonSegment(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 2:
/* #line 66 "URI.c.rl" */
	{ mark0[URISegment_nz] = p - data[0]; }
	break;
	case 3:
/* #line 67 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URISegment_nz];
    tok[1] = p;
    o = URIonSegment_nz(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 4:
/* #line 75 "URI.c.rl" */
	{ mark0[URIPath] = p - data[0]; }
	break;
	case 5:
/* #line 76 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIPath];
    tok[1] = p;
    o = URIonPath(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 6:
/* #line 84 "URI.c.rl" */
	{ mark0[URIPathNoscheme] = p - data[0]; }
	break;
	case 7:
/* #line 85 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIPathNoscheme];
    tok[1] = p;
    o = URIonPathNoscheme(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 8:
/* #line 93 "URI.c.rl" */
	{ mark0[URIScheme] = p - data[0]; }
	break;
	case 9:
/* #line 94 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIScheme];
    tok[1] = p;
    o = URIonScheme(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 10:
/* #line 102 "URI.c.rl" */
	{ mark0[URIIPv4address] = p - data[0]; }
	break;
	case 11:
/* #line 103 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIIPv4address];
    tok[1] = p;
    o = URIonIPv4address(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 12:
/* #line 111 "URI.c.rl" */
	{ mark0[URIIPvFuture] = p - data[0]; }
	break;
	case 13:
/* #line 112 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIIPvFuture];
    tok[1] = p;
    o = URIonIPvFuture(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 14:
/* #line 120 "URI.c.rl" */
	{ mark0[URIIPv6address] = p - data[0]; }
	break;
	case 15:
/* #line 121 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIIPv6address];
    tok[1] = p;
    o = URIonIPv6address(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 16:
/* #line 129 "URI.c.rl" */
	{ mark0[URIIP_literal] = p - data[0]; }
	break;
	case 17:
/* #line 130 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIIP_literal];
    tok[1] = p;
    o = URIonIP_literal(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 18:
/* #line 138 "URI.c.rl" */
	{ mark0[URIUser] = p - data[0]; }
	break;
	case 19:
/* #line 139 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIUser];
    tok[1] = p;
    o = URIonUser(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 20:
/* #line 147 "URI.c.rl" */
	{ mark0[URIHost] = p - data[0]; }
	break;
	case 21:
/* #line 148 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIHost];
    tok[1] = p;
    o = URIonHost(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 22:
/* #line 156 "URI.c.rl" */
	{ mark0[URIPort] = p - data[0]; }
	break;
	case 23:
/* #line 157 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIPort];
    tok[1] = p;
    o = URIonPort(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 24:
/* #line 165 "URI.c.rl" */
	{ mark0[URIAuthority] = p - data[0]; }
	break;
	case 25:
/* #line 166 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIAuthority];
    tok[1] = p;
    o = URIonAuthority(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 26:
/* #line 174 "URI.c.rl" */
	{ mark0[URIFragment] = p - data[0]; }
	break;
	case 28:
/* #line 183 "URI.c.rl" */
	{ mark0[URIQuery] = p - data[0]; }
	break;
	case 29:
/* #line 184 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIQuery];
    tok[1] = p;
    o = URIonQuery(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 30:
/* #line 192 "URI.c.rl" */
	{ mark0[URIPathRootless] = p - data[0]; }
	break;
	case 31:
/* #line 193 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIPathRootless];
    tok[1] = p;
    o = URIonPathRootless(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 32:
/* #line 201 "URI.c.rl" */
	{ mark0[URIURI] = p - data[0]; }
	break;
	case 34:
/* #line 210 "URI.c.rl" */
	{ mark0[URIRoot] = p - data[0]; }
	break;
/* #line 943 "URI.rl.c" */
		}
	}

_again:
	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	const char *__acts = _URI_actions + _URI_eof_actions[cs];
	unsigned int __nacts = (unsigned int) *__acts++;
	while ( __nacts-- > 0 ) {
		switch ( *__acts++ ) {
	case 0:
/* #line 57 "URI.c.rl" */
	{ mark0[URISegment] = p - data[0]; }
	break;
	case 1:
/* #line 58 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URISegment];
    tok[1] = p;
    o = URIonSegment(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 3:
/* #line 67 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URISegment_nz];
    tok[1] = p;
    o = URIonSegment_nz(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 5:
/* #line 76 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIPath];
    tok[1] = p;
    o = URIonPath(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 7:
/* #line 85 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIPathNoscheme];
    tok[1] = p;
    o = URIonPathNoscheme(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 9:
/* #line 94 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIScheme];
    tok[1] = p;
    o = URIonScheme(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 11:
/* #line 103 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIIPv4address];
    tok[1] = p;
    o = URIonIPv4address(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 17:
/* #line 130 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIIP_literal];
    tok[1] = p;
    o = URIonIP_literal(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 20:
/* #line 147 "URI.c.rl" */
	{ mark0[URIHost] = p - data[0]; }
	break;
	case 21:
/* #line 148 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIHost];
    tok[1] = p;
    o = URIonHost(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 23:
/* #line 157 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIPort];
    tok[1] = p;
    o = URIonPort(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 25:
/* #line 166 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIAuthority];
    tok[1] = p;
    o = URIonAuthority(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 26:
/* #line 174 "URI.c.rl" */
	{ mark0[URIFragment] = p - data[0]; }
	break;
	case 27:
/* #line 175 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIFragment];
    tok[1] = p;
    o = URIonFragment(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 28:
/* #line 183 "URI.c.rl" */
	{ mark0[URIQuery] = p - data[0]; }
	break;
	case 29:
/* #line 184 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIQuery];
    tok[1] = p;
    o = URIonQuery(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 31:
/* #line 193 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIPathRootless];
    tok[1] = p;
    o = URIonPathRootless(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 32:
/* #line 201 "URI.c.rl" */
	{ mark0[URIURI] = p - data[0]; }
	break;
	case 33:
/* #line 202 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIURI];
    tok[1] = p;
    o = URIonURI(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 34:
/* #line 210 "URI.c.rl" */
	{ mark0[URIRoot] = p - data[0]; }
	break;
	case 35:
/* #line 211 "URI.c.rl" */
	{
    tok[0] = data[0] + mark0[URIRoot];
    tok[1] = p;
    o = URIonRoot(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
/* #line 1126 "URI.rl.c" */
		}
	}
	}

	_out: {}
	}

/* #line 283 "URI.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < URI_first_final) 
        o = URIBAD;
    
    return o;
}
