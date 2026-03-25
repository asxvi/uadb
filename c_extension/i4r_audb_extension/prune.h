#ifndef PRUNE_H
#define PRUNE_H

#include "helperFunctions.h"

Int4Range prune_lt_internal_range(Int4Range a, Int4Range b, bool direction);
Int4Range prune_gt_internal_range(Int4Range a, Int4Range b, bool direction);
Int4Range prune_lte_internal_range(Int4Range a, Int4Range b, bool direction);
Int4Range prune_gte_internal_range(Int4Range a, Int4Range b, bool direction);
Int4Range prune_eq_internal_range(Int4Range a, Int4Range b);

#endif