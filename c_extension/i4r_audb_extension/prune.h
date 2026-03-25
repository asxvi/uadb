#ifndef PRUNE_H
#define PRUNE_H

#include "helperFunctions.h"

Int4Range prune_lt_internal(Int4Range a, Int4Range b, bool direction);
Int4Range prune_gt_internal(Int4Range a, Int4Range b, bool direction);
// Int4Range prune_eq_internal(Int4Range a, Int4Range b, char direction);
#endif