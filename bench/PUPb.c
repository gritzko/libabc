//  abc/bench/PUPb.c — BENCH-26B0559PCK: one PUPS instance per BOXb.h record,
//  youngest wins, BB_DEAD rows the tombstones (PUPx_DEAD).
#include "PUPb.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "FILE.h"
#include "PRO.h"
#include "PUPS.h"

#define BB_PUP

#define X(M, name) M##e4##name
#include "Bx.h"
#include "QSORTx.h"
#define PUPx_DEAD(r) bbe4_dead(r)
#include "PUPx.h"
#include "BOXbx.h"
#undef X

#define X(M, name) M##e8##name
#include "Bx.h"
#include "QSORTx.h"
#define PUPx_DEAD(r) bbe8_dead(r)
#include "PUPx.h"
#include "BOXbx.h"
#undef X

#define X(M, name) M##e16##name
#include "Bx.h"
#include "QSORTx.h"
#define PUPx_DEAD(r) bbe16_dead(r)
#include "PUPx.h"
#include "BOXbx.h"
#undef X

#define X(M, name) M##e32##name
#include "Bx.h"
#include "QSORTx.h"
#define PUPx_DEAD(r) bbe32_dead(r)
#include "PUPx.h"
#include "BOXbx.h"
#undef X

#define X(M, name) M##e64##name
#include "Bx.h"
#include "QSORTx.h"
#define PUPx_DEAD(r) bbe64_dead(r)
#include "PUPx.h"
#include "BOXbx.h"
#undef X
