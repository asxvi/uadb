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

// combines all ranges into a set. Returns at most size 2 set
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

// handles strict less than: e1 < e2 for sets
Int4RangeSet prune_lt_set_internal(Int4RangeSet a, Int4RangeSet b) {
    Int4RangeSet result;
    Int4Range temp;
    int maxSize, i, j;
    
    result.count = 0;
    result.containsNull = false;
    
    // worst case: every pair produces a range
    maxSize = a.count * b.count;
    result.ranges = palloc(sizeof(Int4Range) * maxSize);

    for (i = 0; i < a.count; i++) {
        for (j = 0; j < b.count; j++) {
            temp = prune_lt_internal_range(a.ranges[i], b.ranges[j], false);

            if (!temp.isNull) {
                result.ranges[result.count++] = temp;
            }
        }
    }

    return normalize(result);
}

// andles strict less than or equal: e1 < e2
Int4RangeSet prune_lte_set_internal(Int4RangeSet a, Int4RangeSet b) {
    Int4RangeSet result;
    Int4Range temp;
    int maxSize, i, j;
    
    result.count = 0;
    result.containsNull = false;
    
    // worst case: every pair produces a range
    maxSize = a.count * b.count;
    result.ranges = palloc(sizeof(Int4Range) * maxSize);

    for (i = 0; i < a.count; i++) {
        for (j = 0; j < b.count; j++) {
            temp = prune_lte_internal_range(a.ranges[i], b.ranges[j], false);

            if (!temp.isNull) {
                result.ranges[result.count++] = temp;
            }
        }
    }

    return normalize(result);
}

// handles strict greater than: e1 > e2 for sets
Int4RangeSet prune_gt_set_internal(Int4RangeSet a, Int4RangeSet b) {
    Int4RangeSet result;
    Int4Range temp;
    int maxSize, i, j;
    
    result.count = 0;
    result.containsNull = false;
    
    // worst case: every pair produces a range
    maxSize = a.count * b.count;
    result.ranges = palloc(sizeof(Int4Range) * maxSize);

    for (i = 0; i < a.count; i++) {
        for (j = 0; j < b.count; j++) {
            temp = prune_gt_internal_range(a.ranges[i], b.ranges[j], false);

            if (!temp.isNull) {
                result.ranges[result.count++] = temp;
            }
        }
    }

    return normalize(result);
}

// handles strict greater than or equal: e1 > e2 for sets
Int4RangeSet prune_gte_set_internal(Int4RangeSet a, Int4RangeSet b) {
    Int4RangeSet result;
    Int4Range temp;
    int maxSize, i, j;
    
    result.count = 0;
    result.containsNull = false;
    
    // worst case: every pair produces a range
    maxSize = a.count * b.count;
    result.ranges = palloc(sizeof(Int4Range) * maxSize);

    for (i = 0; i < a.count; i++) {
        for (j = 0; j < b.count; j++) {
            temp = prune_gte_internal_range(a.ranges[i], b.ranges[j], false);

            if (!temp.isNull) {
                result.ranges[result.count++] = temp;
            }
        }
    }

    return normalize(result);
}

// all points of intersection fpr set. both sides shrink
// pairwise intersection
Int4RangeSet prune_eq_set_internal(Int4RangeSet a, Int4RangeSet b) {
    Int4RangeSet result;
    Int4Range temp;
    int maxSize, i, j;

    result.count = 0;
    result.containsNull = false;

    maxSize = a.count * b.count;
    result.ranges = palloc(sizeof(Int4Range) * maxSize);

    for (i = 0; i < a.count; i++) {
        for (j = 0; j < b.count; j++) {
            temp = intersect_range(a.ranges[i], b.ranges[j]);

            if (!temp.isNull) {
                result.ranges[result.count++] = temp;
            }
        }
    }

    return normalize(result);
}

// NOTE: and is binary not unary. combining conditions required stacking ands
// pairwise intersection
Int4RangeSet prune_AND_internal_set(Int4RangeSet a, Int4RangeSet b) {
    return prune_eq_set_internal(a, b);
}

// concat and normalize
Int4RangeSet prune_OR_internal_set(Int4RangeSet a, Int4RangeSet b) {
    Int4RangeSet result;

    result.count = 0;
    result.containsNull = a.containsNull || b.containsNull;

    result.ranges = palloc(sizeof(Int4Range) * (a.count + b.count));

    // copy A
    for (int i = 0; i < a.count; i++) {
        result.ranges[result.count++] = a.ranges[i];
    }

    // copy B
    for (int i = 0; i < b.count; i++) {
        result.ranges[result.count++] = b.ranges[i];
    }

    return normalize(result);
}

// or compute pairwise union and concat. explodes of course
// Int4RangeSet prune_OR_internal_range(Int4RangeSet a, Int4RangeSet b) {
//     Int4RangeSet result, temp;
//     int maxSize, i, j, k;

//     result.count = 0;
//     result.containsNull = false;

//     maxSize = 2 * a.count * b.count;
//     result.ranges = palloc(sizeof(Int4Range) * maxSize);
    
//     for (i = 0; i < a.count; i++) {
//         for (j = 0; j < b.count; j++) {
//             temp = union_set(a.ranges[i], b.ranges[j]);

//             for (k = 0; k < temp.count; k++) {
//                 if (!temp.ranges[k].isNull) {
//                     result.ranges[result.count++] = temp.ranges[k];
//                 }
//             }
//         }
//     }
//     return normalize(result);
// }