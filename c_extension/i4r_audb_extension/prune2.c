// #include "postgres.h"
// #include <stdlib.h>
// #include <stdio.h>
// #include "logicalOperators.h"
// #include "prune.h"

// #define malloc palloc
// #define free pfree

// #define INT_MIN_LOCAL INT32_MIN
// #define INT_MAX_LOCAL INT32_MAX


// // // Handles the 3 pruning cases for LT in each direction false=left, true=right
// // Int4Range prune_lt_internal_range(Int4Range a, Int4Range b, bool direction) {
// //     Int4Range result;
// //     result.isNull = false;
    
// //     if (direction == 0) {
// //         // strictly less than. Just return left side == A
// //         if (range_less_than(a, b) == 1) {
// //             return a;
// //         }
// //         // A is not LT B. result is NULL
// //         else if (range_less_than(a,b) == 0) {
// //             result.isNull = true;
// //             return result;
// //         }
// //         // otherwise return A pruned on UB by min
// //         result.lower = a.lower;
// //         result.upper = min2(a.upper, b.lower);
// //         if (!validRange(result)) {
// //             result.isNull = true;
// //             return result;
// //         }
// //     } 
// //     else if (direction == 1) {
// //         // strictly less than. Just return right side == B
// //         if (range_less_than(a, b) == 1) {
// //             return b;
// //         }
// //         // A is not LT B. result is NULL
// //         else if (range_less_than(a,b) == 0) {
// //             result.isNull = true;
// //             return result;
// //         }
// //         // otherwise return B pruned on LB by A
// //         result.lower = max2(a.upper, b.lower);
// //         result.upper = b.upper;
// //         if (!validRange(result)) {
// //             result.isNull = true;
// //             return result;
// //         }
// //     }
// //     else{
// //         elog(ERROR, "Error, input a boolean as the third parameter; (false=left, true=right)");
// //     }

// //     return result;
// // }

// // // Handles the 3 pruning cases for GT in each direction false=left, true=right
// // Int4Range prune_gt_internal_range(Int4Range a, Int4Range b, bool direction) {
// //     Int4Range result;
// //     result.isNull = false;

// //     if (direction == 0) {
// //         // strictly greater than. Just return left side == A
// //         if (range_greater_than(a, b) == 1) {
// //             return a;
// //         }
// //         // A is not GT B. result is NULL
// //         else if (range_greater_than(a,b) == 0) {
// //             result.isNull = true;
// //             return result;
// //         }
// //         // otherwise return A pruned on UB by B
// //         result.lower = max2(a.lower, b.lower);
// //         result.upper = a.upper;
// //     } 
// //     else if (direction == 1) {
// //         // strictly less than. Just return right side == B
// //         if (range_greater_than(a, b) == 1) {
// //             return b;
// //         }
// //         // A is not LT B. result is NULL
// //         else if (range_greater_than(a,b) == 0) {
// //             result.isNull = true;
// //             return result;
// //         }
// //         // otherwise return B pruned on LB by A
// //         result.lower = b.lower;
// //         result.upper = min2(a.upper, b.upper);
// //         if (!validRange(result)) {
// //             result.isNull = true;
// //             return result;
// //         }
// //     }
// //     else{
// //         elog(ERROR, "Error, input a boolean as the third parameter; (false=left, true=right)");
// //     }

// //     return result;
// // }

// // // Handles the 3 pruning cases for LTE in each direction false=left, true=right
// // Int4Range prune_lte_internal_range(Int4Range a, Int4Range b, bool direction) {
// //     Int4Range result;
// //     result.isNull = false;
    
// //     if (direction == 0) {
// //         // strictly less than. Just return left side == A
// //         if (range_less_than_equal(a, b) == 1) {
// //             return a;
// //         }
// //         // A is not LT B. result is NULL
// //         else if (range_less_than_equal(a,b) == 0) {
// //             result.isNull = true;
// //             return result;
// //         }
// //         // otherwise return A pruned on UB by min
// //         result.lower = a.lower;
// //         result.upper = min2(a.upper, b.lower);
// //         if (!validRange(result)) {
// //             result.isNull = true;
// //             return result;
// //         }
// //     } 
// //     else if (direction == 1) {
// //         // strictly less than. Just return right side == B
// //         if (range_less_than_equal(a, b) == 1) {
// //             return b;
// //         }
// //         // A is not LT B. result is NULL
// //         else if (range_less_than_equal(a,b) == 0) {
// //             result.isNull = true;
// //             return result;
// //         }
// //         // otherwise return B pruned on LB by A
// //         result.lower = max2(a.upper, b.lower);
// //         result.upper = b.upper;
// //         if (!validRange(result)) {
// //             result.isNull = true;
// //             return result;
// //         }
// //     }
// //     else{
// //         elog(ERROR, "Error, input a boolean as the third parameter; (false=left, true=right)");
// //     }

// //     return result;
// // }

// // // Handles the 3 pruning cases for GTE in each direction false=left, true=right
// // Int4Range prune_gte_internal_range(Int4Range a, Int4Range b, bool direction) {
// //     Int4Range result;
// //     result.isNull = false;

// //     if (direction == 0) {
// //         // strictly greater than. Just return left side == A
// //         if (range_greater_than_equal(a, b) == 1) {
// //             return a;
// //         }
// //         // A is not GT B. result is NULL
// //         else if (range_greater_than_equal(a,b) == 0) {
// //             result.isNull = true;
// //             return result;
// //         }
// //         // otherwise return A pruned on UB by B
// //         result.lower = max2(a.lower, b.lower);
// //         result.upper = a.upper;
// //     } 
// //     else if (direction == 1) {
// //         // strictly less than. Just return right side == B
// //         if (range_greater_than_equal(a, b) == 1) {
// //             return b;
// //         }
// //         // A is not LT B. result is NULL
// //         else if (range_greater_than_equal(a,b) == 0) {
// //             result.isNull = true;
// //             return result;
// //         }
// //         // otherwise return B pruned on LB by A
// //         result.lower = b.lower;
// //         result.upper = min2(a.upper, b.upper);
// //         if (!validRange(result)) {
// //             result.isNull = true;
// //             return result;
// //         }
// //     }
// //     else{
// //         elog(ERROR, "Error, input a boolean as the third parameter; (false=left, true=right)");
// //     }

// //     return result;
// // }

// // // all points of intersection. both sides shrink
// // Int4Range prune_eq_internal_range(Int4Range a, Int4Range b) {
// //     Int4Range result;
// //     result.isNull = false;

// //     result.lower = max2(a.lower, b.lower);
// //     result.upper = min2(a.upper, b.upper);

// //     if (!validRange(result)) {
// //         result.isNull = true;
// //         result.lower = -1;
// //         result.upper = -1;
// //         return result;
// //     }
// //     return result;
// // }

// // // NOTE: and is binary not unary. combining conditions required stacking ands
// // // all points of intersection. both sides shrink
// // Int4Range prune_AND_internal_range(Int4Range a, Int4Range b) {
// //     return prune_eq_internal_range(a, b);
// // }
// // // evaluate the expressions first, and then feed them into prune_and_internal_range()
// // // Int4Range prune_and_internal_range_expr(Int4Range a, Int4Range b) {    
// // // }

// // // doesnt work!
// // Int4RangeSet prune_OR_internal_range(Int4Range a, Int4Range b) {
// //     Int4RangeSet result;
// //     result.count = 0;
// //     result.containsNull = false;
// //     result.ranges = (Int4Range*) malloc(sizeof(Int4Range) * 2); 

// //     if (a.isNull && b.isNull) {
// //         result.containsNull = true;
// //         return result;
// //     }

// //     if (a.isNull) {
// //         result.ranges[0] = b;
// //         result.count = 1;
// //         return result;
// //     }

// //     if (b.isNull) {
// //         result.ranges[0] = a;
// //         result.count = 1;
// //         return result;
// //     }

// //     // Overlapping or contiguous ranges
// //     if (a.upper > b.lower && b.upper > a.lower) {}
// //         result.ranges[0].lower = (a.lower < b.lower) ? a.lower : b.lower;
// //         result.ranges[0].upper = (a.upper > b.upper) ? a.upper : b.upper;
// //         result.ranges[0].isNull = false;
// //         result.count = 1;
// //     } 
// //     else {
// //         // Disjoint ranges → both kept in order
// //         if (a.lower < b.lower) {
// //             result.ranges[0] = a;
// //             result.ranges[1] = b;
// //         } else {
// //             result.ranges[0] = b;
// //             result.ranges[1] = a;
// //         }
// //         result.count = 2;
// //     }

// //     return normalize(result);
// //     // return result;
// // }

// // // finds the complement of the 1 range. results in 2 disjoint sets
// // Int4RangeSet prune_NOT_internal_range(Int4Range a) {
// //     Int4RangeSet result;
// //     Int4Range left, right;

// //     result.count = 0;
// //     result.containsNull = false;
// //     result.ranges = (Int4Range*) malloc(sizeof(Int4Range) * 2);

// //     // Left range
// //     if (a.lower > INT32_MIN) {
// //         left.lower = INT32_MIN;
// //         left.upper = a.lower - 1;
// //         left.isNull = false;
// //         result.ranges[result.count++] = left;
// //     }

// //     // Right range
// //     if (a.upper < INT32_MAX) {
// //         right.lower = a.upper;
// //         right.upper = INT32_MAX;
// //         right.isNull = false;
// //         result.ranges[result.count++] = right;
// //     }

// //     return result;
// // }