#ifndef HELPER_FUNCTION_H
#define HELPER_FUNCTION_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#define min2(a, b) (((a) <= (b)) ? (a) : (b))
#define max2(a, b) (((a) >= (b)) ? (a) : (b))

// [Inclusive LB, Exclusive UB)
typedef struct{ 
    int lower; // inclusive
    int upper; // exclusive
    bool isNull;
} Int4Range;

// array of Int4Range's and the tot count of array
typedef struct{
    Int4Range* ranges;
    size_t count;
    bool containsNull;
} Int4RangeSet;

// **DELETE ME** will break certain outdated code
// [Inlcusive LB, Inclusive UB]
typedef struct{
    int lower;  //inclusive
    int upper;  //inclusive
} Multiplicity;

// // sum aggregate state data type
// typedef struct {
//     Int4RangeSet accumulated;        // potential duplicates
//     Int4Range mult;               // closed interval
//     bool has_null;              // only true if every row contains (0,0) || mult = [0,n]
//     int triggerSize;
// } IntervalAggState;
// typedef struct {
//     Int4Range accumulated;
//     Int4Range mult;
//     // bool has_null;
// } RangeRowType;

typedef struct {
    Int4RangeSet ranges;
    int resizeTrigger;
    int sizeLimit;
} SumAggState;

typedef struct {
    Int4RangeSet ranges;
    int resizeTrigger;
    int sizeLimit;
    bool callNormalize;
    
    long reduceCalls;               //how many times reduceSize() fired
    long maxIntervalCount;          //peak number of intervals seen
    long totalIntervalCount;        //sum of counts across all agg
    long combineCalls;              //number of times merged new input

    long minEffectiveIntervalCount; // smallest number of intervals after reduction
    long convergedToTotSize;             //the min size of the fully converged result

} SumAggStateTest;

typedef struct {
    Int4Range sum;
    Int4Range count;
} rAvgAggState;

typedef struct {
    Int4RangeSet sum;
    Int4Range count;
} sAvgAggState;

// add extra utilites for working with defined type
void printRange(Int4Range a);
void printRangeSet(Int4RangeSet a);
bool validRange(Int4Range a);
bool validRangeStrict(Int4Range a);

// for finding the min int in array. probably not optimal method
int MIN(int My_array[], int len);
int MAX(int My_array[], int len);

// for aggregate functions
Int4Range min_range(Int4Range range1, Int4Range range2);
Int4Range max_range(Int4Range range1, Int4Range range2);
Int4RangeSet min_rangeSet(Int4RangeSet a, Int4RangeSet b);
Int4RangeSet max_rangeSet(Int4RangeSet a, Int4RangeSet b);

Int4Range floatIntervalSetMult(Int4RangeSet a, Multiplicity mult);
bool overlap(Int4Range a, Int4Range b);
bool contains(Int4Range a, Int4Range b);
int range_distance(Int4Range a, Int4Range b);
Int4Range lift_scalar_local(int x);
Int4RangeSet lift_range(Int4Range a);
Int4RangeSet sort(Int4RangeSet vals);
Int4RangeSet normalize(Int4RangeSet vals);
Int4RangeSet reduceSize(Int4RangeSet vals, int numRangesKeep);
Int4RangeSet filterOutNulls(Int4RangeSet vals);
Int4RangeSet interval_agg_combine_set_mult(Int4RangeSet set1, Int4Range mult);
long totalSpan(Int4RangeSet vals);

Int4RangeSet reduceSizeNN(Int4RangeSet vals, int numRangesKeep);

#endif