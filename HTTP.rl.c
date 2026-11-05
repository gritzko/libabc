
#line 1 "HTTP.c.rl"
#include "INT.h"
#include "PRO.h"
#include "HTTP.h"

// action indices for the parser
#define HTTPenum 0
enum {
	HTTPMethod = HTTPenum+22,
	HTTPRequestURI = HTTPenum+23,
	HTTPHTTPVersion = HTTPenum+24,
	HTTPRequestLine = HTTPenum+25,
	HTTPFieldName = HTTPenum+26,
	HTTPFieldValue = HTTPenum+27,
	HTTPMessageHeader = HTTPenum+28,
	HTTPRequestHead = HTTPenum+29,
	HTTPBody = HTTPenum+30,
	HTTPRequest = HTTPenum+31,
	HTTPStatusCode = HTTPenum+32,
	HTTPReasonPhrase = HTTPenum+33,
	HTTPStatusLine = HTTPenum+34,
	HTTPResponse = HTTPenum+35,
	HTTPMessage = HTTPenum+36,
	HTTPRoot = HTTPenum+37,
};

// user functions (callbacks) for the parser
ok64 HTTPonMethod (u8cs tok, HTTPstate* state);
ok64 HTTPonRequestURI (u8cs tok, HTTPstate* state);
ok64 HTTPonHTTPVersion (u8cs tok, HTTPstate* state);
ok64 HTTPonRequestLine (u8cs tok, HTTPstate* state);
ok64 HTTPonFieldName (u8cs tok, HTTPstate* state);
ok64 HTTPonFieldValue (u8cs tok, HTTPstate* state);
ok64 HTTPonMessageHeader (u8cs tok, HTTPstate* state);
ok64 HTTPonRequestHead (u8cs tok, HTTPstate* state);
ok64 HTTPonBody (u8cs tok, HTTPstate* state);
ok64 HTTPonRequest (u8cs tok, HTTPstate* state);
ok64 HTTPonStatusCode (u8cs tok, HTTPstate* state);
ok64 HTTPonReasonPhrase (u8cs tok, HTTPstate* state);
ok64 HTTPonStatusLine (u8cs tok, HTTPstate* state);
ok64 HTTPonResponse (u8cs tok, HTTPstate* state);
ok64 HTTPonMessage (u8cs tok, HTTPstate* state);
ok64 HTTPonRoot (u8cs tok, HTTPstate* state);




#line 246 "HTTP.c.rl"



#line 49 "HTTP.rl.c"
static const char _HTTP_actions[] = {
	0, 1, 1, 1, 2, 1, 3, 1, 
	4, 1, 5, 1, 7, 1, 9, 1, 
	10, 1, 11, 1, 13, 1, 16, 1, 
	20, 1, 21, 1, 22, 1, 23, 1, 
	25, 2, 10, 11, 2, 15, 16, 2, 
	22, 23, 3, 7, 12, 8, 3, 13, 
	12, 8, 3, 14, 6, 0, 3, 25, 
	12, 8, 4, 17, 19, 29, 31, 4, 
	17, 27, 29, 31, 4, 30, 28, 18, 
	26, 5, 14, 6, 0, 24, 4, 5, 
	16, 17, 27, 29, 31, 6, 15, 16, 
	17, 19, 29, 31, 6, 30, 28, 18, 
	14, 6, 0, 9, 30, 28, 18, 14, 
	6, 0, 26, 24, 4
};

static const unsigned char _HTTP_key_offsets[] = {
	0, 0, 9, 18, 19, 20, 21, 22, 
	23, 24, 25, 29, 33, 34, 35, 36, 
	37, 38, 40, 41, 43, 44, 45, 61, 
	62, 78, 84, 85, 101, 107, 108, 109, 
	110, 111, 112, 113, 115, 116, 117, 118, 
	119, 120, 122, 123, 125, 126, 128, 130, 
	132, 133, 139, 145, 146, 162, 163, 179, 
	185, 186, 202, 208, 209, 210, 211, 212, 
	213, 214, 216, 217, 218, 219, 220, 220, 
	220, 220
};

static const unsigned char _HTTP_trans_keys[] = {
	9u, 32u, 67u, 68u, 71u, 72u, 79u, 80u, 
	84u, 9u, 32u, 67u, 68u, 71u, 72u, 79u, 
	80u, 84u, 79u, 78u, 78u, 69u, 67u, 84u, 
	32u, 13u, 32u, 9u, 10u, 13u, 32u, 9u, 
	10u, 72u, 84u, 84u, 80u, 47u, 48u, 57u, 
	46u, 48u, 57u, 13u, 10u, 13u, 33u, 124u, 
	126u, 35u, 39u, 42u, 43u, 45u, 46u, 48u, 
	57u, 65u, 90u, 94u, 122u, 10u, 33u, 58u, 
	124u, 126u, 35u, 39u, 42u, 43u, 45u, 46u, 
	48u, 57u, 65u, 90u, 94u, 122u, 9u, 13u, 
	32u, 127u, 0u, 31u, 10u, 13u, 33u, 124u, 
	126u, 35u, 39u, 42u, 43u, 45u, 46u, 48u, 
	57u, 65u, 90u, 94u, 122u, 13u, 127u, 0u, 
	8u, 10u, 31u, 69u, 76u, 69u, 84u, 69u, 
	69u, 69u, 84u, 65u, 68u, 84u, 80u, 47u, 
	48u, 57u, 46u, 48u, 57u, 32u, 48u, 57u, 
	48u, 57u, 48u, 57u, 32u, 13u, 127u, 0u, 
	8u, 10u, 31u, 13u, 127u, 0u, 8u, 10u, 
	31u, 10u, 13u, 33u, 124u, 126u, 35u, 39u, 
	42u, 43u, 45u, 46u, 48u, 57u, 65u, 90u, 
	94u, 122u, 10u, 33u, 58u, 124u, 126u, 35u, 
	39u, 42u, 43u, 45u, 46u, 48u, 57u, 65u, 
	90u, 94u, 122u, 9u, 13u, 32u, 127u, 0u, 
	31u, 10u, 13u, 33u, 124u, 126u, 35u, 39u, 
	42u, 43u, 45u, 46u, 48u, 57u, 65u, 90u, 
	94u, 122u, 13u, 127u, 0u, 8u, 10u, 31u, 
	80u, 84u, 73u, 79u, 78u, 83u, 79u, 85u, 
	83u, 82u, 65u, 67u, 0
};

static const char _HTTP_single_lengths[] = {
	0, 9, 9, 1, 1, 1, 1, 1, 
	1, 1, 2, 2, 1, 1, 1, 1, 
	1, 0, 1, 0, 1, 1, 4, 1, 
	4, 4, 1, 4, 2, 1, 1, 1, 
	1, 1, 1, 2, 1, 1, 1, 1, 
	1, 0, 1, 0, 1, 0, 0, 0, 
	1, 2, 2, 1, 4, 1, 4, 4, 
	1, 4, 2, 1, 1, 1, 1, 1, 
	1, 2, 1, 1, 1, 1, 0, 0, 
	0, 0
};

static const char _HTTP_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 1, 1, 0, 0, 0, 0, 
	0, 1, 0, 1, 0, 0, 6, 0, 
	6, 1, 0, 6, 2, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 0, 1, 0, 1, 1, 1, 
	0, 2, 2, 0, 6, 0, 6, 1, 
	0, 6, 2, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0
};

static const short _HTTP_index_offsets[] = {
	0, 0, 10, 20, 22, 24, 26, 28, 
	30, 32, 34, 38, 42, 44, 46, 48, 
	50, 52, 54, 56, 58, 60, 62, 73, 
	75, 86, 92, 94, 105, 110, 112, 114, 
	116, 118, 120, 122, 125, 127, 129, 131, 
	133, 135, 137, 139, 141, 143, 145, 147, 
	149, 151, 156, 161, 163, 174, 176, 187, 
	193, 195, 206, 211, 213, 215, 217, 219, 
	221, 223, 226, 228, 230, 232, 234, 235, 
	236, 237
};

static const char _HTTP_indicies[] = {
	0, 0, 2, 3, 4, 5, 6, 7, 
	8, 1, 9, 9, 10, 11, 12, 13, 
	14, 15, 16, 1, 17, 1, 18, 1, 
	19, 1, 20, 1, 21, 1, 22, 1, 
	23, 1, 1, 1, 1, 24, 1, 26, 
	1, 25, 27, 1, 28, 1, 29, 1, 
	30, 1, 31, 1, 32, 1, 33, 1, 
	34, 1, 35, 1, 36, 1, 37, 38, 
	38, 38, 38, 38, 38, 38, 38, 38, 
	1, 39, 1, 40, 41, 40, 40, 40, 
	40, 40, 40, 40, 40, 1, 42, 43, 
	42, 1, 1, 44, 45, 1, 46, 47, 
	47, 47, 47, 47, 47, 47, 47, 47, 
	1, 49, 1, 1, 1, 48, 50, 1, 
	51, 1, 52, 1, 53, 1, 22, 1, 
	21, 1, 54, 55, 1, 56, 1, 22, 
	1, 57, 1, 58, 1, 59, 1, 60, 
	1, 61, 1, 62, 1, 63, 1, 64, 
	1, 65, 1, 66, 1, 67, 1, 69, 
	1, 1, 1, 68, 71, 1, 1, 1, 
	70, 72, 1, 73, 74, 74, 74, 74, 
	74, 74, 74, 74, 74, 1, 75, 1, 
	76, 77, 76, 76, 76, 76, 76, 76, 
	76, 76, 1, 78, 79, 78, 1, 1, 
	80, 81, 1, 82, 83, 83, 83, 83, 
	83, 83, 83, 83, 83, 1, 85, 1, 
	1, 1, 84, 86, 1, 87, 1, 88, 
	1, 89, 1, 90, 1, 22, 1, 91, 
	21, 1, 21, 1, 92, 1, 93, 1, 
	53, 1, 94, 95, 96, 97, 0
};

static const char _HTTP_trans_targs[] = {
	2, 0, 3, 29, 34, 35, 59, 65, 
	67, 2, 3, 29, 34, 35, 59, 65, 
	67, 4, 5, 6, 7, 8, 9, 10, 
	11, 11, 12, 13, 14, 15, 16, 17, 
	18, 19, 20, 21, 22, 23, 24, 70, 
	24, 25, 25, 26, 28, 27, 23, 24, 
	28, 26, 30, 31, 32, 33, 36, 38, 
	37, 39, 40, 41, 42, 43, 44, 45, 
	46, 47, 48, 49, 50, 51, 50, 51, 
	52, 53, 54, 72, 54, 55, 55, 56, 
	58, 57, 53, 54, 58, 56, 60, 61, 
	62, 63, 64, 66, 68, 69, 71, 71, 
	73, 73
};

static const char _HTTP_trans_actions[] = {
	68, 0, 92, 92, 92, 99, 92, 92, 
	92, 0, 50, 50, 50, 73, 50, 50, 
	50, 0, 0, 0, 0, 0, 0, 1, 
	3, 0, 5, 7, 0, 0, 0, 0, 
	0, 0, 0, 9, 0, 11, 42, 0, 
	0, 13, 15, 33, 15, 0, 19, 46, 
	0, 17, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 9, 
	23, 0, 0, 25, 27, 39, 0, 29, 
	0, 31, 54, 0, 0, 13, 15, 33, 
	15, 0, 19, 46, 0, 17, 0, 0, 
	0, 0, 0, 0, 0, 0, 36, 0, 
	21, 0
};

static const char _HTTP_eof_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 85, 58, 
	79, 63
};

static const int HTTP_start = 1;
static const int HTTP_first_final = 70;
static const int HTTP_error = 0;

static const int HTTP_en_main = 1;


#line 249 "HTTP.c.rl"

// the public API function
ok64 HTTPLexer(HTTPstate* state) {

    a_dup(u8c, data, state->data);
    sane($ok(data));

    int cs = 0;
    u8c *p = (u8c*) data[0];
    u8c *pe = (u8c*) data[1];
    u8c *eof = pe;
    u64 mark0[64] = {};
    ok64 o = OK;

    u8cs tok = {p, p};

    
#line 249 "HTTP.rl.c"
	{
	cs = HTTP_start;
	}

#line 266 "HTTP.c.rl"
    
#line 252 "HTTP.rl.c"
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
	_keys = _HTTP_trans_keys + _HTTP_key_offsets[cs];
	_trans = _HTTP_index_offsets[cs];

	_klen = _HTTP_single_lengths[cs];
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

	_klen = _HTTP_range_lengths[cs];
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
	_trans = _HTTP_indicies[_trans];
	cs = _HTTP_trans_targs[_trans];

	if ( _HTTP_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _HTTP_actions + _HTTP_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 53 "HTTP.c.rl"
	{ mark0[HTTPMethod] = p - data[0]; }
	break;
	case 1:
#line 54 "HTTP.c.rl"
	{
    tok[0] = data[0] + mark0[HTTPMethod];
    tok[1] = p;
    o = HTTPonMethod(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 2:
#line 62 "HTTP.c.rl"
	{ mark0[HTTPRequestURI] = p - data[0]; }
	break;
	case 3:
#line 63 "HTTP.c.rl"
	{
    tok[0] = data[0] + mark0[HTTPRequestURI];
    tok[1] = p;
    o = HTTPonRequestURI(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 4:
#line 71 "HTTP.c.rl"
	{ mark0[HTTPHTTPVersion] = p - data[0]; }
	break;
	case 5:
#line 72 "HTTP.c.rl"
	{
    tok[0] = data[0] + mark0[HTTPHTTPVersion];
    tok[1] = p;
    o = HTTPonHTTPVersion(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 6:
#line 80 "HTTP.c.rl"
	{ mark0[HTTPRequestLine] = p - data[0]; }
	break;
	case 7:
#line 81 "HTTP.c.rl"
	{
    tok[0] = data[0] + mark0[HTTPRequestLine];
    tok[1] = p;
    o = HTTPonRequestLine(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 8:
#line 89 "HTTP.c.rl"
	{ mark0[HTTPFieldName] = p - data[0]; }
	break;
	case 9:
#line 90 "HTTP.c.rl"
	{
    tok[0] = data[0] + mark0[HTTPFieldName];
    tok[1] = p;
    o = HTTPonFieldName(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 10:
#line 98 "HTTP.c.rl"
	{ mark0[HTTPFieldValue] = p - data[0]; }
	break;
	case 11:
#line 99 "HTTP.c.rl"
	{
    tok[0] = data[0] + mark0[HTTPFieldValue];
    tok[1] = p;
    o = HTTPonFieldValue(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 12:
#line 107 "HTTP.c.rl"
	{ mark0[HTTPMessageHeader] = p - data[0]; }
	break;
	case 13:
#line 108 "HTTP.c.rl"
	{
    tok[0] = data[0] + mark0[HTTPMessageHeader];
    tok[1] = p;
    o = HTTPonMessageHeader(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 14:
#line 116 "HTTP.c.rl"
	{ mark0[HTTPRequestHead] = p - data[0]; }
	break;
	case 15:
#line 117 "HTTP.c.rl"
	{
    tok[0] = data[0] + mark0[HTTPRequestHead];
    tok[1] = p;
    o = HTTPonRequestHead(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 16:
#line 125 "HTTP.c.rl"
	{ mark0[HTTPBody] = p - data[0]; }
	break;
	case 18:
#line 134 "HTTP.c.rl"
	{ mark0[HTTPRequest] = p - data[0]; }
	break;
	case 20:
#line 143 "HTTP.c.rl"
	{ mark0[HTTPStatusCode] = p - data[0]; }
	break;
	case 21:
#line 144 "HTTP.c.rl"
	{
    tok[0] = data[0] + mark0[HTTPStatusCode];
    tok[1] = p;
    o = HTTPonStatusCode(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 22:
#line 152 "HTTP.c.rl"
	{ mark0[HTTPReasonPhrase] = p - data[0]; }
	break;
	case 23:
#line 153 "HTTP.c.rl"
	{
    tok[0] = data[0] + mark0[HTTPReasonPhrase];
    tok[1] = p;
    o = HTTPonReasonPhrase(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 24:
#line 161 "HTTP.c.rl"
	{ mark0[HTTPStatusLine] = p - data[0]; }
	break;
	case 25:
#line 162 "HTTP.c.rl"
	{
    tok[0] = data[0] + mark0[HTTPStatusLine];
    tok[1] = p;
    o = HTTPonStatusLine(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 26:
#line 170 "HTTP.c.rl"
	{ mark0[HTTPResponse] = p - data[0]; }
	break;
	case 28:
#line 179 "HTTP.c.rl"
	{ mark0[HTTPMessage] = p - data[0]; }
	break;
	case 30:
#line 188 "HTTP.c.rl"
	{ mark0[HTTPRoot] = p - data[0]; }
	break;
#line 483 "HTTP.rl.c"
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
	const char *__acts = _HTTP_actions + _HTTP_eof_actions[cs];
	unsigned int __nacts = (unsigned int) *__acts++;
	while ( __nacts-- > 0 ) {
		switch ( *__acts++ ) {
	case 15:
#line 117 "HTTP.c.rl"
	{
    tok[0] = data[0] + mark0[HTTPRequestHead];
    tok[1] = p;
    o = HTTPonRequestHead(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 16:
#line 125 "HTTP.c.rl"
	{ mark0[HTTPBody] = p - data[0]; }
	break;
	case 17:
#line 126 "HTTP.c.rl"
	{
    tok[0] = data[0] + mark0[HTTPBody];
    tok[1] = p;
    o = HTTPonBody(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 19:
#line 135 "HTTP.c.rl"
	{
    tok[0] = data[0] + mark0[HTTPRequest];
    tok[1] = p;
    o = HTTPonRequest(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 27:
#line 171 "HTTP.c.rl"
	{
    tok[0] = data[0] + mark0[HTTPResponse];
    tok[1] = p;
    o = HTTPonResponse(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 29:
#line 180 "HTTP.c.rl"
	{
    tok[0] = data[0] + mark0[HTTPMessage];
    tok[1] = p;
    o = HTTPonMessage(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
	case 31:
#line 189 "HTTP.c.rl"
	{
    tok[0] = data[0] + mark0[HTTPRoot];
    tok[1] = p;
    o = HTTPonRoot(tok, state); 
    if (o!=OK) {
        p--; {p++; goto _out; }
    }
}
	break;
#line 561 "HTTP.rl.c"
		}
	}
	}

	_out: {}
	}

#line 267 "HTTP.c.rl"

    state->data[0] = p;
    if (o==OK && cs < HTTP_first_final) 
        o = HTTPBAD;
    
    return o;
}
