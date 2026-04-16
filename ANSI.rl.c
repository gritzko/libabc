
/* #line 1 "abc/ANSI.c.rl" */
// ANSI CSI parser (ECMA-48 / VT100).
//
// Input side of the abc/ANSI module.  The output side (escfeed,
// escfeedBG256) lives in ANSI.h as inline formatters.
#include "ANSI.h"


/* #line 26 "abc/ANSI.c.rl" */



/* #line 10 "abc/ANSI.rl.c" */
static const char _csi_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 2, 2, 3
};

static const char _csi_key_offsets[] = {
	0, 0, 1, 2, 8, 13, 15, 19
};

static const unsigned char _csi_trans_keys[] = {
	27u, 91u, 48u, 57u, 60u, 63u, 64u, 126u, 
	59u, 48u, 57u, 64u, 126u, 48u, 57u, 48u, 
	57u, 64u, 126u, 0
};

static const char _csi_single_lengths[] = {
	0, 1, 1, 0, 1, 0, 0, 0
};

static const char _csi_range_lengths[] = {
	0, 0, 0, 3, 2, 1, 2, 0
};

static const char _csi_index_offsets[] = {
	0, 0, 2, 4, 8, 12, 14, 17
};

static const char _csi_indicies[] = {
	0, 1, 2, 1, 3, 4, 5, 1, 
	6, 3, 7, 1, 3, 1, 3, 5, 
	1, 1, 0
};

static const char _csi_trans_targs[] = {
	2, 0, 3, 4, 6, 7, 5, 7
};

static const char _csi_trans_actions[] = {
	0, 0, 0, 3, 1, 7, 5, 9
};

static const int csi_start = 1;

static const int csi_en_main = 1;


/* #line 29 "abc/ANSI.c.rl" */

ok64 ANSIu8sDrainCSI(u8cs input, csip out) {
    if (out == NULL) return ANSIBAD;
    *out = (csi){};
    if (input[0] == NULL || input[0] >= input[1]) return ANSIBAD;

    u8c *p = input[0];
    u8c *pe = input[1];
    u8c *eof = pe;
    int cs = 0;
    u32 cur = 0;

    
/* #line 67 "abc/ANSI.rl.c" */
	{
	cs = csi_start;
	}

/* #line 42 "abc/ANSI.c.rl" */
    
/* #line 70 "abc/ANSI.rl.c" */
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
	_keys = _csi_trans_keys + _csi_key_offsets[cs];
	_trans = _csi_index_offsets[cs];

	_klen = _csi_single_lengths[cs];
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

	_klen = _csi_range_lengths[cs];
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
	_trans = _csi_indicies[_trans];
	cs = _csi_trans_targs[_trans];

	if ( _csi_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _csi_actions + _csi_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
/* #line 11 "abc/ANSI.c.rl" */
	{ out->private = *p; }
	break;
	case 1:
/* #line 12 "abc/ANSI.c.rl" */
	{ cur = cur * 10 + (u32)(*p - '0'); }
	break;
	case 2:
/* #line 13 "abc/ANSI.c.rl" */
	{
        if (out->nparams < CSI_MAX_PARAMS)
            out->params[out->nparams++] = cur;
        cur = 0;
    }
	break;
	case 3:
/* #line 18 "abc/ANSI.c.rl" */
	{ out->final = *p; {p++; goto _out; } }
	break;
/* #line 159 "abc/ANSI.rl.c" */
		}
	}

_again:
	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	_out: {}
	}

/* #line 43 "abc/ANSI.c.rl" */

    if (cs < 7)
        return ANSIBAD;

    input[0] = p;
    return OK;
}
