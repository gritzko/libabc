#ifndef ABC_Y_H
#define ABC_Y_H
#include "BUF.h"
#include "OK.h"

#define Y_MAX_INPUTS 64


#define X(M, name) M##u8cs##name
#include "HEAPx.h"
#undef X

#endif
