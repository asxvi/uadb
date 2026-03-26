#include "postgres.h"
#include <stdlib.h>
#include <stdio.h>
#include "logicalOperators.h"
#include "prune.h"

#define malloc palloc
#define free pfree

#define INT_MIN_LOCAL INT32_MIN
#define INT_MAX_LOCAL INT32_MAX

// handles strict less than: e1 < e2
Int4Range prune_lt_internal_range(Int4Range a, Int4Range b, bool direction) {
    Int4Range result;
    result.isNull = false;

    // 0 = left, 1 = right
    if (direction == 0) { 
        Int4Range bound;
        bound.lower = INT32_MIN;
        bound.upper = b.upper - 1;  // [-inf, max(b)-1]
        bound.isNull = false;
        result = intersect_range(a, bound);
    } 
    else { 
        Int4Range bound;
        bound.lower = a.lower + 1;  // [min(a)+1, +inf]
        bound.upper = INT32_MAX;
        bound.isNull = false;
        result = intersect_range(b, bound);
    }

    if (!validRange(result)) result.isNull = true;
    return result;
}

// andles strict less than or equal: e1 < e2
Int4Range prune_lte_internal_range(Int4Range a, Int4Range b, bool direction) {
    Int4Range result;
    result.isNull = false;

    // 0 = left, 1 = right
    if (direction == 0) { 
        Int4Range bound;
        bound.lower = INT32_MIN;
        bound.upper = b.upper;  // [-inf, max(b)]
        bound.isNull = false;
        result = intersect_range(a, bound);
    } 
    else { 
        Int4Range bound;
        bound.lower = a.lower;  // [min(a), +inf]
        bound.upper = INT32_MAX;
        bound.isNull = false;
        result = intersect_range(b, bound);
    }

    if (!validRange(result)) result.isNull = true;
    return result;
}

// handles strict greater than: e1 > e2
Int4Range prune_gt_internal_range(Int4Range a, Int4Range b, bool direction) {
    Int4Range result;
    result.isNull = false;

    if (direction == 0) {
        Int4Range bound;
        bound.lower = b.lower + 1; // [min(b)+1, +inf]
        bound.upper = INT32_MAX;
        bound.isNull = false;
        result = intersect_range(a, bound);
    } 
    else {
        Int4Range bound;
        bound.lower = INT32_MIN;
        bound.upper = a.upper - 1; // [-inf, max(a)-1]
        bound.isNull = false;
        result = intersect_range(b, bound);
    }

    if (!validRange(result)) result.isNull = true;
    return result;
}

// handles greater than or equal: e1 >= e2
Int4Range prune_gte_internal_range(Int4Range a, Int4Range b, bool direction) {
    Int4Range result;
    result.isNull = false;

    if (direction == 0) {
        Int4Range bound;
        bound.lower = b.lower; // [min(b), +inf]
        bound.upper = INT32_MAX;
        bound.isNull = false;
        result = intersect_range(a, bound);
    } 
    else {
        Int4Range bound;
        bound.lower = INT32_MIN;
        bound.upper = a.upper; // [-inf, max(a)]
        bound.isNull = false;
        result = intersect_range(b, bound);
    }

    if (!validRange(result)) result.isNull = true;
    return result;
}

// all points of intersection. both sides shrink
Int4Range prune_eq_internal_range(Int4Range a, Int4Range b) {
    Int4Range result;
    result.isNull = false;

    result.lower = max2(a.lower, b.lower);
    result.upper = min2(a.upper, b.upper);

    if (!validRange(result)) {
        result.isNull = true;
        result.lower = -1;
        result.upper = -1;
        return result;
    }
    return result;
}

// NOTE: and is binary not unary. combining conditions required stacking ands
// all points of intersection. both sides shrink
Int4Range prune_AND_internal_range(Int4Range a, Int4Range b) {
    return prune_eq_internal_range(a, b);
}
// evaluate the expressions first, and then feed them into prune_and_internal_range()
// Int4Range prune_and_internal_range_expr(Int4Range a, Int4Range b) {    
// }

// doesnt work!
Int4RangeSet prune_OR_internal_range(Int4Range a, Int4Range b) {
    Int4RangeSet result;
    result.count = 0;
    result.containsNull = false;
    result.ranges = (Int4Range*) malloc(sizeof(Int4Range) * 2); 

    if (a.isNull && b.isNull) {
        result.containsNull = true;
        return result;
    }

    if (a.isNull) {
        result.ranges[0] = b;
        result.count = 1;
        return result;
    }

    if (b.isNull) {
        result.ranges[0] = a;
        result.count = 1;
        return result;
    }

    // Overlapping or contiguous ranges
    if (a.upper > b.lower && b.upper > a.lower) {
        result.ranges[0].lower = (a.lower < b.lower) ? a.lower : b.lower;
        result.ranges[0].upper = (a.upper > b.upper) ? a.upper : b.upper;
        result.ranges[0].isNull = false;
        result.count = 1;
    } 
    else {
        // Disjoint ranges → both kept in order
        if (a.lower < b.lower) {
            result.ranges[0] = a;
            result.ranges[1] = b;
        } else {
            result.ranges[0] = b;
            result.ranges[1] = a;
        }
        result.count = 2;
    }

    return normalize(result);
    // return result;
}

// finds the complement of the 1 range. results in 2 disjoint sets
Int4RangeSet prune_NOT_internal_range(Int4Range a) {
    Int4RangeSet result;
    Int4Range left, right;

    result.count = 0;
    result.containsNull = false;
    result.ranges = (Int4Range*) malloc(sizeof(Int4Range) * 2);

    // Left range
    if (a.lower > INT32_MIN) {
        left.lower = INT32_MIN;
        left.upper = a.lower - 1;
        left.isNull = false;
        result.ranges[result.count++] = left;
    }

    // Right range
    if (a.upper < INT32_MAX) {
        right.lower = a.upper;
        right.upper = INT32_MAX;
        right.isNull = false;
        result.ranges[result.count++] = right;
    }

    return result;
}