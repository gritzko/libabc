//  abc/bench/BOXb.c — BENCH-26B0559PCK: one BOX instance per BOXb.h record,
//  default ABC_BOX_SEG, youngest wins; an erase puts a BB_DEAD row.
#include "BOXb.h"

#include "PRO.h"

#define X(M, name) M##e4##name
#include "Bx.h"
#include "QSORTx.h"
#include "BOXx.h"
#include "BOXbx.h"
#undef X

#define X(M, name) M##e8##name
#include "Bx.h"
#include "QSORTx.h"
#include "BOXx.h"
#include "BOXbx.h"
#undef X

#define X(M, name) M##e16##name
#include "Bx.h"
#include "QSORTx.h"
#include "BOXx.h"
#include "BOXbx.h"
#undef X

#define X(M, name) M##e32##name
#include "Bx.h"
#include "QSORTx.h"
#include "BOXx.h"
#include "BOXbx.h"
#undef X

#define X(M, name) M##e64##name
#include "Bx.h"
#include "QSORTx.h"
#include "BOXx.h"
#include "BOXbx.h"
#undef X
