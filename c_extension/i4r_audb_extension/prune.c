#include "postgres.h"
#include <stdlib.h>
#include <stdio.h>
// #include "arithmetic.h"
#include "logicalOperators.h"
#include "prune.h"

#define malloc palloc
#define free pfree

// Handles the 3 pruning cases for LT in each direction false=left, true=right
Int4Range prune_lt_internal(Int4Range a, Int4Range b, bool direction) {
    Int4Range result;
    result.isNull = false;
    
    if (direction == 0) {
        // strictly less than. Just return left side == A
        if (range_less_than(a, b) == 1) {
            return a;
        }
        // A is not LT B. result is NULL
        else if (range_less_than(a,b) == 0) {
            result.isNull = true;
            return result;
        }
        // otherwise return A pruned on UB by min
        result.lower = a.lower;
        result.upper = min2(a.upper, b.lower);
        if (!validRange(result)) {
            result.isNull = true;
            return result;
        }
    } 
    else if (direction == 1) {
        // strictly less than. Just return right side == B
        if (range_less_than(a, b) == 1) {
            return b;
        }
        // A is not LT B. result is NULL
        else if (range_less_than(a,b) == 0) {
            result.isNull = true;
            return result;
        }
        // otherwise return B pruned on LB by A
        result.lower = max2(a.upper, b.lower);
        result.upper = b.upper;
        if (!validRange(result)) {
            result.isNull = true;
            return result;
        }
    }
    else{
        elog(ERROR, "Error, input a boolean as the third parameter; (false=left, true=right)");
    }

    return result;
}

// Handles the 3 pruning cases for GT in each direction false=left, true=right
Int4Range prune_gt_internal(Int4Range a, Int4Range b, bool direction) {
    Int4Range result;
    result.isNull = false;

    if (direction == 0) {
        // strictly greater than. Just return left side == A
        if (range_greater_than(a, b) == 1) {
            return a;
        }
        // A is not GT B. result is NULL
        else if (range_greater_than(a,b) == 0) {
            result.isNull = true;
            return result;
        }
        // otherwise return A pruned on UB by B
        result.lower = max2(a.lower, b.lower);
        result.upper = a.upper;
    } 
    else if (direction == 1) {
        // strictly less than. Just return right side == B
        if (range_greater_than(a, b) == 1) {
            return b;
        }
        // A is not LT B. result is NULL
        else if (range_greater_than(a,b) == 0) {
            result.isNull = true;
            return result;
        }
        // otherwise return B pruned on LB by A
        result.lower = b.lower;
        result.upper = min2(a.upper, b.upper);
        if (!validRange(result)) {
            result.isNull = true;
            return result;
        }
    }
    else{
        elog(ERROR, "Error, input a boolean as the third parameter; (false=left, true=right)");
    }

    return result;
}

// Handles the 3 pruning cases for LT in each direction false=left, true=right
Int4Range prune_lte_internal(Int4Range a, Int4Range b, bool direction) {
    Int4Range result;
    result.isNull = false;
    
    if (direction == 0) {
        // strictly less than. Just return left side == A
        if (range_less_than_equal(a, b) == 1) {
            return a;
        }
        // A is not LT B. result is NULL
        else if (range_less_than_equal(a,b) == 0) {
            result.isNull = true;
            return result;
        }
        // otherwise return A pruned on UB by min
        result.lower = a.lower;
        result.upper = min2(a.upper, b.lower);
        if (!validRange(result)) {
            result.isNull = true;
            return result;
        }
    } 
    else if (direction == 1) {
        // strictly less than. Just return right side == B
        if (range_less_than_equal(a, b) == 1) {
            return b;
        }
        // A is not LT B. result is NULL
        else if (range_less_than_equal(a,b) == 0) {
            result.isNull = true;
            return result;
        }
        // otherwise return B pruned on LB by A
        result.lower = max2(a.upper, b.lower);
        result.upper = b.upper;
        if (!validRange(result)) {
            result.isNull = true;
            return result;
        }
    }
    else{
        elog(ERROR, "Error, input a boolean as the third parameter; (false=left, true=right)");
    }

    return result;
}

// Handles the 3 pruning cases for GT in each direction false=left, true=right
Int4Range prune_gte_internal(Int4Range a, Int4Range b, bool direction) {
    Int4Range result;
    result.isNull = false;

    if (direction == 0) {
        // strictly greater than. Just return left side == A
        if (range_greater_than_equal(a, b) == 1) {
            return a;
        }
        // A is not GT B. result is NULL
        else if (range_greater_than_equal(a,b) == 0) {
            result.isNull = true;
            return result;
        }
        // otherwise return A pruned on UB by B
        result.lower = max2(a.lower, b.lower);
        result.upper = a.upper;
    } 
    else if (direction == 1) {
        // strictly less than. Just return right side == B
        if (range_greater_than_equal(a, b) == 1) {
            return b;
        }
        // A is not LT B. result is NULL
        else if (range_greater_than_equal(a,b) == 0) {
            result.isNull = true;
            return result;
        }
        // otherwise return B pruned on LB by A
        result.lower = b.lower;
        result.upper = min2(a.upper, b.upper);
        if (!validRange(result)) {
            result.isNull = true;
            return result;
        }
    }
    else{
        elog(ERROR, "Error, input a boolean as the third parameter; (false=left, true=right)");
    }

    return result;
}