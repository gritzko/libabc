
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




#line 239 "HTTP.c.rl"



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

static const short _HTTP_key_offsets[] = {
	0, 0, 18, 36, 52, 56, 60, 61, 
	62, 63, 64, 65, 67, 68, 70, 71, 
	72, 88, 89, 105, 111, 112, 128, 134, 
	151, 168, 185, 200, 202, 203, 205, 206, 
	208, 210, 212, 213, 219, 225, 226, 242, 
	243, 259, 265, 266, 282, 288, 288, 288, 
	288
};

static const unsigned char _HTTP_trans_keys[] = {
	9u, 32u, 33u, 72u, 124u, 126u, 35u, 39u, 
	42u, 43u, 45u, 46u, 48u, 57u, 65u, 90u, 
	94u, 122u, 9u, 32u, 33u, 72u, 124u, 126u, 
	35u, 39u, 42u, 43u, 45u, 46u, 48u, 57u, 
	65u, 90u, 94u, 122u, 32u, 33u, 124u, 126u, 
	35u, 39u, 42u, 43u, 45u, 46u, 48u, 57u, 
	65u, 90u, 94u, 122u, 13u, 32u, 9u, 10u, 
	13u, 32u, 9u, 10u, 72u, 84u, 84u, 80u, 
	47u, 48u, 57u, 46u, 48u, 57u, 13u, 10u, 
	13u, 33u, 124u, 126u, 35u, 39u, 42u, 43u, 
	45u, 46u, 48u, 57u, 65u, 90u, 94u, 122u, 
	10u, 33u, 58u, 124u, 126u, 35u, 39u, 42u, 
	43u, 45u, 46u, 48u, 57u, 65u, 90u, 94u, 
	122u, 9u, 13u, 32u, 127u, 0u, 31u, 10u, 
	13u, 33u, 124u, 126u, 35u, 39u, 42u, 43u, 
	45u, 46u, 48u, 57u, 65u, 90u, 94u, 122u, 
	13u, 127u, 0u, 8u, 10u, 31u, 32u, 33u, 
	84u, 124u, 126u, 35u, 39u, 42u, 43u, 45u, 
	46u, 48u, 57u, 65u, 90u, 94u, 122u, 32u, 
	33u, 84u, 124u, 126u, 35u, 39u, 42u, 43u, 
	45u, 46u, 48u, 57u, 65u, 90u, 94u, 122u, 
	32u, 33u, 80u, 124u, 126u, 35u, 39u, 42u, 
	43u, 45u, 46u, 48u, 57u, 65u, 90u, 94u, 
	122u, 32u, 33u, 47u, 124u, 126u, 35u, 39u, 
	42u, 43u, 45u, 57u, 65u, 90u, 94u, 122u, 
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
	0
};

static const char _HTTP_single_lengths[] = {
	0, 6, 6, 4, 2, 2, 1, 1, 
	1, 1, 1, 0, 1, 0, 1, 1, 
	4, 1, 4, 4, 1, 4, 2, 5, 
	5, 5, 5, 0, 1, 0, 1, 0, 
	0, 0, 1, 2, 2, 1, 4, 1, 
	4, 4, 1, 4, 2, 0, 0, 0, 
	0
};

static const char _HTTP_range_lengths[] = {
	0, 6, 6, 6, 1, 1, 0, 0, 
	0, 0, 0, 1, 0, 1, 0, 0, 
	6, 0, 6, 1, 0, 6, 2, 6, 
	6, 6, 5, 1, 0, 1, 0, 1, 
	1, 1, 0, 2, 2, 0, 6, 0, 
	6, 1, 0, 6, 2, 0, 0, 0, 
	0
};

static const short _HTTP_index_offsets[] = {
	0, 0, 13, 26, 37, 41, 45, 47, 
	49, 51, 53, 55, 57, 59, 61, 63, 
	65, 76, 78, 89, 95, 97, 108, 113, 
	125, 137, 149, 160, 162, 164, 166, 168, 
	170, 172, 174, 176, 181, 186, 188, 199, 
	201, 212, 218, 220, 231, 236, 237, 238, 
	239
};

static const char _HTTP_indicies[] = {
	0, 0, 2, 3, 2, 2, 2, 2, 
	2, 2, 2, 2, 1, 4, 4, 5, 
	6, 5, 5, 5, 5, 5, 5, 5, 
	5, 1, 7, 8, 8, 8, 8, 8, 
	8, 8, 8, 8, 1, 1, 1, 1, 
	9, 1, 11, 1, 10, 12, 1, 13, 
	1, 14, 1, 15, 1, 16, 1, 17, 
	1, 18, 1, 19, 1, 20, 1, 21, 
	1, 22, 23, 23, 23, 23, 23, 23, 
	23, 23, 23, 1, 24, 1, 25, 26, 
	25, 25, 25, 25, 25, 25, 25, 25, 
	1, 27, 28, 27, 1, 1, 29, 30, 
	1, 31, 32, 32, 32, 32, 32, 32, 
	32, 32, 32, 1, 34, 1, 1, 1, 
	33, 7, 8, 35, 8, 8, 8, 8, 
	8, 8, 8, 8, 1, 7, 8, 36, 
	8, 8, 8, 8, 8, 8, 8, 8, 
	1, 7, 8, 37, 8, 8, 8, 8, 
	8, 8, 8, 8, 1, 7, 8, 38, 
	8, 8, 8, 8, 8, 8, 8, 1, 
	39, 1, 40, 1, 41, 1, 42, 1, 
	43, 1, 44, 1, 45, 1, 46, 1, 
	48, 1, 1, 1, 47, 50, 1, 1, 
	1, 49, 51, 1, 52, 53, 53, 53, 
	53, 53, 53, 53, 53, 53, 1, 54, 
	1, 55, 56, 55, 55, 55, 55, 55, 
	55, 55, 55, 1, 57, 58, 57, 1, 
	1, 59, 60, 1, 61, 62, 62, 62, 
	62, 62, 62, 62, 62, 62, 1, 64, 
	1, 1, 1, 63, 65, 66, 67, 68, 
	0
};

static const char _HTTP_trans_targs[] = {
	2, 0, 3, 23, 2, 3, 23, 4, 
	3, 5, 5, 6, 7, 8, 9, 10, 
	11, 12, 13, 14, 15, 16, 17, 18, 
	45, 18, 19, 19, 20, 22, 21, 17, 
	18, 22, 20, 24, 25, 26, 27, 28, 
	29, 30, 31, 32, 33, 34, 35, 36, 
	37, 36, 37, 38, 39, 40, 47, 40, 
	41, 41, 42, 44, 43, 39, 40, 44, 
	42, 46, 46, 48, 48
};

static const char _HTTP_trans_actions[] = {
	68, 0, 92, 99, 0, 50, 73, 1, 
	0, 3, 0, 5, 7, 0, 0, 0, 
	0, 0, 0, 0, 9, 0, 11, 42, 
	0, 0, 13, 15, 33, 15, 0, 19, 
	46, 0, 17, 0, 0, 0, 0, 0, 
	0, 0, 9, 23, 0, 0, 25, 27, 
	39, 0, 29, 0, 31, 54, 0, 0, 
	13, 15, 33, 15, 0, 19, 46, 0, 
	17, 36, 0, 21, 0
};

static const char _HTTP_eof_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 85, 58, 79, 
	63
};

static const int HTTP_start = 1;
static const int HTTP_first_final = 45;
static const int HTTP_error = 0;

static const int HTTP_en_main = 1;


#line 242 "HTTP.c.rl"

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

    
#line 236 "HTTP.rl.c"
	{
	cs = HTTP_start;
	}

#line 259 "HTTP.c.rl"
    
#line 239 "HTTP.rl.c"
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
#line 470 "HTTP.rl.c"
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
#line 548 "HTTP.rl.c"
		}
	}
	}

	_out: {}
	}

#line 260 "HTTP.c.rl"

    state->data[0] = p;
    if (o==OK && cs < HTTP_first_final) 
        o = HTTPBAD;
    
    return o;
}
