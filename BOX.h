#ifndef ABC_BOX_H
#define ABC_BOX_H
#include "HIT.h"
#include "OK.h"

//  ABC-26B02JHe_k: BOXx returns the shared codes (wiki/Containers.mkd):
//  OK, NONE, NOROOM, FULL, BADARG; see BOX.md.

//  PUPS-26B045Ffpb: the house, one X(,csb) of ABC_HIT_MAX_RUNS run cells; its
//  term is cell 62, so cells 62..63 are the memtable X(,b) (BOX.md "Layout").
#define PUP_HOUSE(c)                                                   \
    {(c), (c), (c), (c) + ABC_HIT_MAX_RUNS - 2 +                       \
         0 * sizeof(struct {                                           \
             static_assert(sizeof(c) == ABC_HIT_MAX_RUNS * sizeof(*(c)), \
                           "PUP_HOUSE: 64 cells");                     \
             int _;                                                    \
         })}
#define a_box(T, h)                       \
    T##cs _##h[ABC_HIT_MAX_RUNS] = {};    \
    T##cs *h[4] = PUP_HOUSE(_##h)

#endif
