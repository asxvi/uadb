// source files
#include "postgres.h"           // root
#include "fmgr.h"               // "..must be included by all Postgres modules"
#include "utils/rangetypes.h"   // RangeType
#include "utils/array.h"        // ArrayType
#include "utils/typcache.h"     // data type cache
#include "utils/lsyscache.h"    // "Convenience routines for common queries in the system catalog cache."
#include "catalog/pg_type_d.h"  // pg_type oid macros
#include "catalog/namespace.h"  // type helpers
#include "funcapi.h"


// local code
#include "arithmetic.h"         // logic for arithmetic 
#include "logicalOperators.h"   // logic for logical ops
#include "helperFunctions.h"    // logic for helpers
#include "serialization.h"      // serial and deserial helpers

PG_MODULE_MAGIC;

/*(Arithmetic Functions)*/
PG_FUNCTION_INFO_V1(range_add);
PG_FUNCTION_INFO_V1(range_subtract);
PG_FUNCTION_INFO_V1(range_multiply);
PG_FUNCTION_INFO_V1(range_divide);
PG_FUNCTION_INFO_V1(set_add);
PG_FUNCTION_INFO_V1(set_subtract);
PG_FUNCTION_INFO_V1(set_multiply);
PG_FUNCTION_INFO_V1(set_divide);

/*(Logical Operator Functions)*/
PG_FUNCTION_INFO_V1(range_lt);
PG_FUNCTION_INFO_V1(range_lte);
PG_FUNCTION_INFO_V1(range_gt);
PG_FUNCTION_INFO_V1(range_gte);
PG_FUNCTION_INFO_V1(range_eq);
PG_FUNCTION_INFO_V1(set_lt);
PG_FUNCTION_INFO_V1(set_lte);
PG_FUNCTION_INFO_V1(set_gt);
PG_FUNCTION_INFO_V1(set_gte);
PG_FUNCTION_INFO_V1(set_eq);

/*(Helper Functions)*/
PG_FUNCTION_INFO_V1(lift_scalar);
PG_FUNCTION_INFO_V1(set_sort);
PG_FUNCTION_INFO_V1(set_normalize);
PG_FUNCTION_INFO_V1(set_reduce_size);

/*(Aggregate Functions)*/
//sum
PG_FUNCTION_INFO_V1(combine_range_mult_sum);
PG_FUNCTION_INFO_V1(agg_sum_range_transfunc);
PG_FUNCTION_INFO_V1(combine_set_mult_sum);
PG_FUNCTION_INFO_V1(agg_sum_set_transfunc);
PG_FUNCTION_INFO_V1(agg_sum_set_finalfunc);

PG_FUNCTION_INFO_V1(agg_sum_set_transfuncTest);
PG_FUNCTION_INFO_V1(agg_sum_set_transfuncTestNN);
PG_FUNCTION_INFO_V1(agg_sum_set_finalfuncTest);

// min/max
PG_FUNCTION_INFO_V1(combine_range_mult_min);
PG_FUNCTION_INFO_V1(combine_range_mult_max);
PG_FUNCTION_INFO_V1(agg_min_range_transfunc);
PG_FUNCTION_INFO_V1(agg_max_range_transfunc);
PG_FUNCTION_INFO_V1(combine_set_mult_min);
PG_FUNCTION_INFO_V1(combine_set_mult_max);
PG_FUNCTION_INFO_V1(agg_min_set_transfunc);
PG_FUNCTION_INFO_V1(agg_max_set_transfunc);
PG_FUNCTION_INFO_V1(agg_min_max_set_finalfunc);

// count -- assumes mult is RangeType.. easy fix if not
PG_FUNCTION_INFO_V1(agg_count_transfunc);

// avg- uses agg_sum_set_transfunc as transition function
PG_FUNCTION_INFO_V1(agg_avg_range_transfunc);
PG_FUNCTION_INFO_V1(agg_avg_range_finalfunc);
PG_FUNCTION_INFO_V1(agg_avg_set_transfunc);
PG_FUNCTION_INFO_V1(agg_avg_set_finalfunc);


// easy change for future implementation. currently only affects lift funciton
#define PRIMARY_DATA_TYPE "int4range"

/// check for NULLS parameters. Different from empty range check
// returns the parameter that is not null
#define CHECK_BINARY_PGARG_NULL_ARGS()                          \
    do {                                                        \
        if (PG_ARGISNULL(0) && PG_ARGISNULL(1))                 \
            PG_RETURN_NULL();                                   \
        else if (PG_ARGISNULL(0))                               \
            PG_RETURN_DATUM(PG_GETARG_DATUM(1));                \
        else if (PG_ARGISNULL(1))                               \
            PG_RETURN_DATUM(PG_GETARG_DATUM(0));                \
    } while (0)

// check for NULL param on either PGARG(0) OR PGARG(1)
#define CHECK_BINARY_PGARG_NULL_OR()                            \
    do {                                                        \
        if (PG_ARGISNULL(0) || PG_ARGISNULL(1))                 \
            PG_RETURN_NULL();                                   \
    } while (0)

/* takes in 2 RangeType parameters, and returns a single RangeType with provided operator result */
#define DEFINE_RANGE_ARITHMETIC_FUNC(func_name, internal_func)      \
Datum func_name(PG_FUNCTION_ARGS)                                   \
{                                                                   \
    RangeType *r1;                                                  \
    RangeType *r2;                                                  \
    RangeType *output;                                              \
    CHECK_BINARY_PGARG_NULL_ARGS();                                 \
    r1 = PG_GETARG_RANGE_P(0);                                      \
    r2 = PG_GETARG_RANGE_P(1);                                      \
    output = arithmetic_range_helper(r1, r2, internal_func);        \
    PG_RETURN_RANGE_P(output);                                      \
}

/* takes in 2 ArrayType parameters, and returns a single ArrayType with provided operator result */
#define DEFINE_SET_ARITHMETIC_FUNC(func_name, internal_func)        \
Datum func_name(PG_FUNCTION_ARGS)                                   \
{                                                                   \
    ArrayType *a1;                                                  \
    ArrayType *a2;                                                  \
    ArrayType *output;                                              \
    CHECK_BINARY_PGARG_NULL_ARGS();                                 \
    a1 = PG_GETARG_ARRAYTYPE_P(0);                                  \
    a2 = PG_GETARG_ARRAYTYPE_P(1);                                  \
    output = arithmetic_set_helper(a1, a2, internal_func);          \
    PG_RETURN_ARRAYTYPE_P(output);                                  \
}

/* takes in 2 RangeType parameters, and returns a 3VL boolean after comparison*/
#define DEFINE_RANGE_LOGICAL_FUNC(func_name, internal_func)         \
Datum func_name(PG_FUNCTION_ARGS)                                   \
{                                                                   \
    RangeType *r1;                                                  \
    RangeType *r2;                                                  \
    int rv;                                                         \
    CHECK_BINARY_PGARG_NULL_ARGS();                                 \
    r1 = PG_GETARG_RANGE_P(0);                                  \
    r2 = PG_GETARG_RANGE_P(1);                                  \
    rv = logical_range_helper(r1, r2, internal_func);               \
    if (rv == -1){                                                  \
        PG_RETURN_NULL();                                           \
    }                                                               \
    PG_RETURN_BOOL((bool)rv);                                       \
}

/* takes in 2 ArrayType parameters, and returns a 3VL boolean after comparison*/
#define DEFINE_SET_LOGICAL_FUNC(func_name, internal_func)           \
Datum func_name(PG_FUNCTION_ARGS)                                   \
{                                                                   \
    ArrayType *a1;                                                  \
    ArrayType *a2;                                                  \
    int rv;                                                         \
    CHECK_BINARY_PGARG_NULL_ARGS();                                 \
    a1 = PG_GETARG_ARRAYTYPE_P(0);                                  \
    a2 = PG_GETARG_ARRAYTYPE_P(1);                                  \
    rv = logical_set_helper(a1, a2, internal_func);                 \
    if (rv == -1){                                                  \
        PG_RETURN_NULL();                                           \
    }                                                               \
    PG_RETURN_BOOL((bool)rv);                                       \
}

/*Function declarations*/

RangeType* arithmetic_range_helper(RangeType *input1, RangeType *input2, Int4Range (*callback)(Int4Range, Int4Range));
ArrayType* arithmetic_set_helper(ArrayType *input1, ArrayType *input2, Int4RangeSet (*callback)(Int4RangeSet, Int4RangeSet));
int logical_range_helper(RangeType *input1, RangeType *input2, int (*callback)(Int4Range, Int4Range) );
int logical_set_helper(ArrayType *input1, ArrayType *input2, int (*callback)(Int4RangeSet, Int4RangeSet) );
ArrayType* helperFunctions_helper( ArrayType *input, Int4RangeSet (*callback)() );

// for min/max agg
// can all be added to helperFunctions.h
Int4Range range_mult_combine_helper_sum(Int4Range set1, Int4Range mult, int neutralElement);
Int4RangeSet set_mult_combine_helper_sum(Int4RangeSet set1, Int4Range mult, int neutralElement);
Int4Range range_mult_combine_helper(Int4Range range, Int4Range mult, int neutralElement);
Int4RangeSet set_mult_combine_helper(Int4RangeSet set1, Int4Range mult, int neutralElement);

/////////////////////
    // Arithmetic
/////////////////////
DEFINE_RANGE_ARITHMETIC_FUNC(range_add, range_add_internal)
DEFINE_RANGE_ARITHMETIC_FUNC(range_subtract, range_subtract_internal)
DEFINE_RANGE_ARITHMETIC_FUNC(range_multiply, range_multiply_internal)
DEFINE_RANGE_ARITHMETIC_FUNC(range_divide, range_divide_internal)

DEFINE_SET_ARITHMETIC_FUNC(set_add, range_set_add_internal)
DEFINE_SET_ARITHMETIC_FUNC(set_subtract, range_set_subtract_internal)
DEFINE_SET_ARITHMETIC_FUNC(set_multiply, range_set_multiply_internal)
DEFINE_SET_ARITHMETIC_FUNC(set_divide, range_set_divide_internal)

/////////////////////
    // Comparison
/////////////////////
DEFINE_RANGE_LOGICAL_FUNC(range_gt, range_greater_than)
DEFINE_RANGE_LOGICAL_FUNC(range_gte, range_greater_than_equal)
DEFINE_RANGE_LOGICAL_FUNC(range_lt, range_less_than)
DEFINE_RANGE_LOGICAL_FUNC(range_lte, range_less_than_equal)
DEFINE_RANGE_LOGICAL_FUNC(range_eq, range_equal_internal)

DEFINE_SET_LOGICAL_FUNC(set_gt, set_greater_than)
DEFINE_SET_LOGICAL_FUNC(set_gte, set_greater_than_equal)
DEFINE_SET_LOGICAL_FUNC(set_lt, set_less_than)
DEFINE_SET_LOGICAL_FUNC(set_lte, set_less_than_equal)
DEFINE_SET_LOGICAL_FUNC(set_eq, set_equal_internal)

/////////////////////
 // Helper Functions
/////////////////////

/* lift expects 1 parameter x for example and returns a valid int4range [x, x+1) */
// Lift an Integer x into a RangeType [x, x+1)
Datum
lift_scalar(PG_FUNCTION_ARGS)
{
    Oid rangeTypeOID;
    TypeCacheEntry *typcache;
    int unlifted;
    Int4Range result;
    RangeBound lb, ub;
    RangeType *output;

    // check for NULLS. Diff from empty check
    if (PG_ARGISNULL(0)){
        PG_RETURN_NULL();
    }

    rangeTypeOID = TypenameGetTypid(PRIMARY_DATA_TYPE);
    typcache = lookup_type_cache(rangeTypeOID, TYPECACHE_RANGE_INFO);
    
    unlifted = PG_GETARG_INT32(0);
    
    result = lift_scalar_local(unlifted);
    
    lb = make_range_bound(result.lower, true, true);
    ub = make_range_bound(result.upper, false, false);
        
    output = make_range(typcache, &lb, &ub, false, NULL);

    PG_RETURN_RANGE_P(output);
}

// FIXME- fix the local code for this. need to account for NULL. should be simple fix
// also figure out of can make a single helperFunction_helper that takes in optinal parameters
Datum
set_reduce_size(PG_FUNCTION_ARGS)
{
    ArrayType *inputArray;
    int32 numRangesKeep;
    Oid rangeTypeOID;
    TypeCacheEntry *typcache;
    Int4RangeSet set1;
    Int4RangeSet result;
    ArrayType *output;

    // check for NULLS. Diff from empty check
    if (PG_ARGISNULL(0) || PG_ARGISNULL(1)){
        PG_RETURN_NULL();
    }

    inputArray = PG_GETARG_ARRAYTYPE_P(0);
    numRangesKeep = PG_GETARG_INT32(1);

    // assign typcache based on RangeType input
    rangeTypeOID = ARR_ELEMTYPE(inputArray);
    typcache = lookup_type_cache(rangeTypeOID, TYPECACHE_RANGE_INFO);

    set1 = deserialize_ArrayType(inputArray, typcache);
    
    // return NULL sorted range == NULL. 
    if (set1.count == 0){
        PG_RETURN_NULL();
    }

    // reduce the set to numRangesKeep
    result = reduceSize(set1, numRangesKeep);

    // the reduced size should always be less than equal to numRangesKeep
    if (result.count < numRangesKeep) {
        ereport(ERROR,
            (errcode(ERRCODE_DATA_CORRUPTED),
            errmsg("result.count < numRangesKeep when reducing. Impossible result")));
    }

    output = serialize_ArrayType(result, typcache);
    
    pfree(set1.ranges);
    pfree(result.ranges);

    PG_RETURN_ARRAYTYPE_P(output);
}

Datum
set_sort(PG_FUNCTION_ARGS)
{
    ArrayType *inputArray;
    ArrayType *output;
    
    // check for NULLS. Diff from empty check
    if (PG_ARGISNULL(0)){
        PG_RETURN_NULL();
    }

    inputArray = PG_GETARG_ARRAYTYPE_P(0);
    output = helperFunctions_helper(inputArray, sort);

    PG_RETURN_ARRAYTYPE_P(output);
}

Datum
set_normalize(PG_FUNCTION_ARGS)
{
    ArrayType *inputArray;
    ArrayType *output;
    
    // check for NULLS. Diff from empty check
    if (PG_ARGISNULL(0)){
        PG_RETURN_NULL();
    }

    inputArray = PG_GETARG_ARRAYTYPE_P(0);
    output = helperFunctions_helper(inputArray, normalize);

    PG_RETURN_ARRAYTYPE_P(output);
}

/*
Takes in 2 parameters: Array: Int4RangeSet, and the function ptr callback: Int4RangeSet function() 
Generally called for helper functions that modify 1 Int4RangeSet param passed in
*/
ArrayType*
helperFunctions_helper(ArrayType *input, Int4RangeSet (*callback)(Int4RangeSet) )
{
    Oid rangeTypeOID;
    TypeCacheEntry *typcache;
    Int4RangeSet set1;
    Int4RangeSet result;
    ArrayType *output;

    rangeTypeOID = ARR_ELEMTYPE(input);
    typcache = lookup_type_cache(rangeTypeOID, TYPECACHE_RANGE_INFO);

    set1 = deserialize_ArrayType(input, typcache);
    result = callback(set1);
    output = serialize_ArrayType(result, typcache);
    
    pfree(set1.ranges);
    pfree(result.ranges);

    return output;
}

/*
Generic Helper for arithmetic operations on RangeTypes.
Deserializes data, performs operation on data, serializes it back to native PG RangeType.
* Parameters(3): 
    -input1 RangeType: Int4Range, NON-NULL
    -input2 RangeType: Int4Range, NON-NULL
    -function ptr callback: Int4Range function()   
* Return(1):
    -RangeType result
*/
RangeType*
arithmetic_range_helper(RangeType *input1, RangeType *input2, Int4Range (*callback)(Int4Range, Int4Range))
{   
    // assign typcache based on RangeType input
    Oid rangeTypeOID1;
    Oid rangeTypeOID2;
    TypeCacheEntry *typcache;
    Int4Range range1; 
    Int4Range range2;
    Int4Range result;
    RangeType *output;

    rangeTypeOID1 = RangeTypeGetOid(input1);
    rangeTypeOID2 = RangeTypeGetOid(input2);
    typcache = lookup_type_cache(rangeTypeOID1, TYPECACHE_RANGE_INFO);

    if (rangeTypeOID1 != rangeTypeOID2) {
        ereport(ERROR,
                (errcode(ERRCODE_DATATYPE_MISMATCH),
                errmsg("range type mismatch in arithmetic operation")));
    }

    range1 = deserialize_RangeType(input1, typcache);
    range2 = deserialize_RangeType(input2, typcache);
    
    // safety check. Should not be necessary bc postgres enforces this already
    if(!validRange(range1) || !validRange(range2)) {
        return make_empty_range(typcache);;
    }

    // implemented C function
    result = callback(range1, range2);

    // convert result into RangeType
    output = serialize_RangeType(result, typcache);
    return output;
}

/*
Generic Helper for arithmetic operations on ArrayType of RangeTypes.
Deserializes data, performs operation on data, serializes it back to native PG type.
* Parameters(3): 
    -input1 ArrayType: Int4RangeSet, NON-NULL
    -input2 ArrayType: Int4RangeSet, NON-NULL
    -function ptr callback: Int4RangeSet function()   
* Return(1):
    -ArrayType result
*/
ArrayType*
arithmetic_set_helper(ArrayType *input1, ArrayType *input2, Int4RangeSet (*callback)(Int4RangeSet, Int4RangeSet))
{   
    Oid rangeTypeOID1;
    Oid rangeTypeOID2;
    TypeCacheEntry *typcache;
    Int4RangeSet set1; 
    Int4RangeSet set2;
    Int4RangeSet result;
    ArrayType *output;
    
    // assign typcache based on RangeType input
    rangeTypeOID1 = ARR_ELEMTYPE(input1);
    rangeTypeOID2 = ARR_ELEMTYPE(input2);
    typcache = lookup_type_cache(rangeTypeOID1, TYPECACHE_RANGE_INFO);
    
    if (rangeTypeOID1 != rangeTypeOID2) {
        ereport(ERROR,
                (errcode(ERRCODE_DATATYPE_MISMATCH),
                errmsg("range type mismatch in arithmetic operation")));
    }

    // convert native PG representaion to our local representation
    set1 = deserialize_ArrayType(input1, typcache);
    set2 = deserialize_ArrayType(input2, typcache);

    if(set1.containsNull && set1.count == 1) {
        output = serialize_ArrayType(set2, typcache);
        return output;
    }
    else if(set2.containsNull && set2.count == 1) {
        output = serialize_ArrayType(set1, typcache);
        return output;
    }

    // callback function in this case is an arithmetic function with params: (Int4RangeSet a, Int4RangeSet b)
    result = callback(set1, set2);
    
    // convert result back to native PG representation
    output = serialize_ArrayType(result, typcache);

    // clean local representation
    pfree(set1.ranges);
    pfree(set2.ranges);
    pfree(result.ranges);

    return output;
}

/*
Generic Helper for logical operations on ArrayTypes of RangeTypes.
Deserializes data, performs operation on data, returns int (3VL boolean) result
* Parameters(3): 
    -input1 RangeType: Int4Range, NON-NULL
    -input2 RangeType: Int4Range, NON-NULL
    -function ptr callback: Int4Range function()   
* Return(1):
    -int result (3VL)
*/
int 
logical_range_helper(RangeType *input1, RangeType *input2, int (*callback)(Int4Range, Int4Range) )
{   
    Int4Range range1;
    Int4Range range2;
    TypeCacheEntry *typcache;
    int result;

    typcache = lookup_type_cache(input1->rangetypid, TYPECACHE_RANGE_INFO);
    if (input1->rangetypid != input2->rangetypid) {
        ereport(ERROR,
                (errcode(ERRCODE_DATATYPE_MISMATCH),
                errmsg("range type mismatch in arithmetic operation")));
    }

    // deserialize, operate, return integer
    range1 = deserialize_RangeType(input1, typcache);
    range2 = deserialize_RangeType(input2, typcache);
    result = callback(range1, range2);

    return result;
}

/*
Generic Helper for logical operations on ArrayTypes of RangeTypes.
Deserializes data, performs operation on data, returns int (3VL boolean) result
* Parameters(3): 
    -input1 RangeType: Int4Range, NON-NULL
    -input2 RangeType: Int4Range, NON-NULL
    -function ptr callback: Int4Range function()   
* Return(1):
    -int result (3VL)
*/
int 
logical_set_helper(ArrayType *input1, ArrayType *input2, int (*callback)(Int4RangeSet, Int4RangeSet) )
{   
    // assign typcache based on RangeType input
    Oid rangeTypeOID1;
    Oid rangeTypeOID2;
    TypeCacheEntry *typcache;
    Int4RangeSet set1;
    Int4RangeSet set2;
    int result;

    rangeTypeOID1 = ARR_ELEMTYPE(input1);
    rangeTypeOID2 = ARR_ELEMTYPE(input2);
    typcache = lookup_type_cache(rangeTypeOID1, TYPECACHE_RANGE_INFO);
    
    if (rangeTypeOID1 != rangeTypeOID2) {
        ereport(ERROR,
                (errcode(ERRCODE_DATATYPE_MISMATCH),
                errmsg("range type mismatch in arithmetic operation")));
    }

    // deserialize, operate, return integer
    set1 = deserialize_ArrayType(input1, typcache);
    set2 = deserialize_ArrayType(input2, typcache);
    result = callback(set1, set2);

    return result;
}

// /////////////////////
//  //   AGGREGATES
// /////////////////////

/*
// Returns naturalElement Set if multiplicity is 0, otherwise original Set. 
// naturalElement Set does not affect min/max calculation
*/
// FIXME will need to change the type of neutral element depending on what datatype the user is using
Int4Range
range_mult_combine_helper_sum(Int4Range set1, Int4Range mult, int neutralElement)
{
    // return neutral so doesn't affect the aggregate
    if(mult.lower == 0) {
        Int4Range result;
        result.isNull = false;
    
        // have to adjust UB + 2 or LB -2 based on if pos or neg
        if (neutralElement <= 0) {
            result.lower = 0;      //temp change to resolve crashing   
            result.upper = 1 ;
        }
        else {
            result.lower = neutralElement;      //temp change to resolve crashing   
            result.upper = neutralElement + 1;
        }
        return result;
    }

    return set1;
}

/*
// Returns naturalElement Set if multiplicity is 0, otherwise original Set. 
// naturalElement Set does not affect min/max calculation
*/
// FIXME will need to change the type of neutral element depending on what datatype the user is using
Int4RangeSet
set_mult_combine_helper_sum(Int4RangeSet set1, Int4Range mult, int neutralElement)
{
    // return neutral so doesn't affect the aggregate
    if(mult.lower == 0) {
        Int4RangeSet result;
        result.count = 1;
        result.containsNull = false;
        result.ranges = palloc(sizeof(Int4Range));
        result.ranges[0].isNull = false;
    
        // have to adjust UB + 2 or LB -2 based on if pos or neg
        if (neutralElement <= 0) {
            result.ranges[0].lower = 0;      //temp change to resolve crashing   
            result.ranges[0].upper = 1;
        }
        else {
            result.ranges[0].lower = neutralElement;      //temp change to resolve crashing   
            result.ranges[0].upper = neutralElement + 1;
        }
        return result;
    }

    return set1;
}

/*
// To be called inside a MAX aggregation call. This multiplies the Set and multiplicity together.
// neutral_element is the only difference between min/max implementation. This value is HARDCODED //FIXME
// Parameter: ArrayType (data col), RangeType (multiplicity)
// Returns: a ArrayType Datum as argument to MAX()
*/
Datum
combine_range_mult_sum(PG_FUNCTION_ARGS) 
{
    // inputs/ outputs
    RangeType *input; 
    RangeType *mult_input;
    RangeType *output;
    
    // working type
    Int4Range input_i4r;
    Int4Range result_i4r;
    Int4Range mult_i4r;

    int neutral_element;
    TypeCacheEntry *typcache;
    TypeCacheEntry *typcacheMult;
    
    CHECK_BINARY_PGARG_NULL_OR();
    
    input = PG_GETARG_RANGE_P(0);
    mult_input = PG_GETARG_RANGE_P(1);

    typcache = lookup_type_cache(input->rangetypid, TYPECACHE_RANGE_INFO);
    typcacheMult = lookup_type_cache(mult_input->rangetypid, TYPECACHE_RANGE_INFO);

    // hardcoded //FIXME
    neutral_element = 0;

    // deserialize, operate on, serialize, return
    input_i4r = deserialize_RangeType(input, typcache);
    mult_i4r = deserialize_RangeType(mult_input, typcacheMult);
    
    // check for mult LB = 0
    if (mult_i4r.lower == 0){;
        PG_RETURN_NULL();
    }

    result_i4r = range_mult_combine_helper_sum(input_i4r, mult_i4r, neutral_element);
    output = serialize_RangeType(result_i4r, typcache);

    PG_RETURN_ARRAYTYPE_P(output);
}

/*
// To be called inside a MAX aggregation call. This multiplies the Set and multiplicity together.
// neutral_element is the only difference between min/max implementation. This value is HARDCODED //FIXME
// Parameter: ArrayType (data col), RangeType (multiplicity)
// Returns: a ArrayType Datum as argument to MAX()
*/
Datum
combine_set_mult_sum(PG_FUNCTION_ARGS) 
{
    // inputs/ outputs
    ArrayType *set_input;
    RangeType *mult_input;
    ArrayType *output;
    
    // working type
    Int4Range mult;
    Int4RangeSet set1;
    Int4RangeSet result;
    
    int neutral_element;
    TypeCacheEntry *typcacheSet;
    TypeCacheEntry *typcacheMult;
    
    CHECK_BINARY_PGARG_NULL_OR();
    
    set_input = PG_GETARG_ARRAYTYPE_P(0);
    mult_input = PG_GETARG_RANGE_P(1);

    typcacheSet = lookup_type_cache(set_input->elemtype, TYPECACHE_RANGE_INFO);
    typcacheMult = lookup_type_cache(mult_input->rangetypid, TYPECACHE_RANGE_INFO);

    // check for empty array
    if (ArrayGetNItems(ARR_NDIM(set_input), ARR_DIMS(set_input)) == 0) {
        PG_RETURN_NULL();
    }
    // check for empty mult
    if RangeIsEmpty(mult_input){
        PG_RETURN_NULL();
    }

    // hardcoded //FIXME
    neutral_element = 0;

    // deserialize, operate on, serialize, return
    set1 = deserialize_ArrayType(set_input, typcacheSet);
    mult = deserialize_RangeType(mult_input, typcacheMult);

    // check for mult LB = 0
    if (mult.lower == 0){
        pfree(set1.ranges);
        PG_RETURN_NULL();
    }

    result = set_mult_combine_helper(set1, mult, neutral_element);
    output = serialize_ArrayType(result, typcacheSet);
    
    // clean
    pfree(set1.ranges);
    if (result.ranges != set1.ranges) {
        pfree(result.ranges);
    }

    PG_RETURN_ARRAYTYPE_P(output);
}

/*
*   transition function for sum(combine_set_mult_sum(data, mult), resizetrigger, sizelimit)
*   
* Parameters [4]:
*   - SumAggState: (internal type)
*   - ArrayType: current state (result of combine_set_mult_sum(data, mult))
*   - Integer: resize trigger
*   - Integer: size limit
*   
* Returns [1]:
*   - Datum (pointer to SumAggState) 
*/
Datum
agg_sum_set_transfunc(PG_FUNCTION_ARGS)
{
    MemoryContext aggcontext;
    MemoryContext oldcontext;
    SumAggState *state;
    ArrayType *currSet;
    TypeCacheEntry *typcache;
    Int4RangeSet inputSet, combined, reduced;
    
    if (!AggCheckCallContext(fcinfo, &aggcontext))
        elog(ERROR, "agg_sum_set_transfunc called in non-aggregate context");
    
    // first call, state is NULL
    if (PG_ARGISNULL(0)) {

        // check for NULL input param, or empty
        if (PG_ARGISNULL(1)) {
            PG_RETURN_NULL();
        }
        
        currSet = PG_GETARG_ARRAYTYPE_P(1);
        typcache = lookup_type_cache(ARR_ELEMTYPE(currSet), TYPECACHE_RANGE_INFO);
        
        // empty set, continue on until non empty
        if (ArrayGetNItems(ARR_NDIM(currSet), ARR_DIMS(currSet)) == 0) {
            PG_RETURN_NULL();
        }

        // switch to aggregate memory context for persistent allocations
        oldcontext = MemoryContextSwitchTo(aggcontext);
        
        // internal state init
        state = (SumAggState *) palloc0(sizeof(SumAggState));
        state->ranges = deserialize_ArrayType(currSet, typcache);
        state->resizeTrigger = PG_GETARG_INT32(2);
        state->sizeLimit = PG_GETARG_INT32(3);
        
        // need to return to callers context
        MemoryContextSwitchTo(oldcontext);
        
        PG_RETURN_POINTER(state);
    }
    
    // otherwise merge into existing state
    state = (SumAggState *) PG_GETARG_POINTER(0);

    if (!PG_ARGISNULL(1)) {
        currSet = PG_GETARG_ARRAYTYPE_P(1);
        typcache = lookup_type_cache(ARR_ELEMTYPE(currSet), TYPECACHE_RANGE_INFO);

        // empty check
        if (ArrayGetNItems(ARR_NDIM(currSet), ARR_DIMS(currSet)) == 0) {
            PG_RETURN_POINTER(state);
        }
        
        // deserialize input in current context (freed later)
        inputSet = deserialize_ArrayType(currSet, typcache);
        
        // agg context persists data
        oldcontext = MemoryContextSwitchTo(aggcontext);
        combined = range_set_add_internal(state->ranges, inputSet);
        
        // free old ranges
        if (state->ranges.ranges != NULL) {
            pfree(state->ranges.ranges);
        }
        
        // check reduce size
        if (combined.count >= state->resizeTrigger) {
            reduced = reduceSize(combined, state->sizeLimit);
            pfree(combined.ranges);
            state->ranges = reduced;
        }
        else {
            state->ranges = combined;
        }
        
        MemoryContextSwitchTo(oldcontext);
        
        // free previous memory context
        pfree(inputSet.ranges);
    }
    
    PG_RETURN_POINTER(state);
}

/*
    Reduce one last time if needed and Convert Internal type to ArrayType Datum.
*/
Datum
agg_sum_set_finalfunc(PG_FUNCTION_ARGS)
{
    SumAggState *state;
    ArrayType *result;
    TypeCacheEntry *typcache;
    Int4RangeSet reduced;
    Oid elemTypeOID;
    
    if (PG_ARGISNULL(0)) {
        PG_RETURN_NULL();
    }
    
    state = (SumAggState*) PG_GETARG_POINTER(0);
    // empty state
    if (state->ranges.count == 0) {
        elemTypeOID = TypenameGetTypid("int4range");
        PG_RETURN_ARRAYTYPE_P(construct_empty_array(elemTypeOID));
    }
    
    elemTypeOID = TypenameGetTypid("int4range");
    typcache = lookup_type_cache(elemTypeOID, TYPECACHE_RANGE_INFO);
    
    // reduce final time
    if (state->ranges.count >= state->resizeTrigger) {
        reduced = reduceSize(state->ranges, state->sizeLimit);
        result = serialize_ArrayType(reduced, typcache);
        pfree(reduced.ranges);
        PG_RETURN_ARRAYTYPE_P(result);
    }
    
    result = serialize_ArrayType(state->ranges, typcache);
    PG_RETURN_ARRAYTYPE_P(result);
}

// agg over empty == 0,0
/* arbitrary trigger size. doesnt use for now 
    first parameter is the state
    second parameter is the current val (I4RSet)
    third parameter is the multiplicity
 */
Datum
agg_sum_range_transfunc(PG_FUNCTION_ARGS)
{
    RangeType *state;
    RangeType *input;
    RangeType *result;
    
    // first call: use the first input as initial state, or non null
    if (PG_ARGISNULL(0)){
        if (PG_ARGISNULL(1)){
            PG_RETURN_NULL();
        }
        // othrwise value becomes the state
        PG_RETURN_RANGE_P(PG_GETARG_RANGE_P(1));
    }

    // NULL input: return current state unchanged
    if(PG_ARGISNULL(1)) {
        PG_RETURN_RANGE_P(PG_GETARG_RANGE_P(0));
    }
    
    // get arguments, call helper to get result, check to normalize after
    state = PG_GETARG_RANGE_P(0);
    input = PG_GETARG_RANGE_P(1);

    result = arithmetic_range_helper(state, input, range_add_internal);

    PG_RETURN_ARRAYTYPE_P(result);
}


/*
State Transition function for max aggregate
Returns the minimum LB and UB of all ranges in column.
Simply deserializes data, operates on it, and serializes 
    State = Int4Range = [a,b)
    Input = Int4Range = [c,d)
    Return RangeType: [min(a,c), min(b,d))
*/
Datum
agg_min_range_transfunc(PG_FUNCTION_ARGS)
{
    Int4Range state_i4r, input_i4r, result_i4r;
    RangeType *state, *input, *result;
    TypeCacheEntry *typcache;

    // first call: use the first input as initial state, or non null
    if (PG_ARGISNULL(0)) {
        if (PG_ARGISNULL(1)) {
            PG_RETURN_NULL();
        }
        // othrwise value becomes the state
        PG_RETURN_RANGE_P(PG_GETARG_RANGE_P(1));
    }
    
    // NULL input: return current state unchanged
    if (PG_ARGISNULL(1)) {
        PG_RETURN_RANGE_P(PG_GETARG_RANGE_P(0));
    }

    // compare existing min/state to the current input
    state = PG_GETARG_RANGE_P(0);
    input = PG_GETARG_RANGE_P(1);

    // return non empty
    if (RangeIsEmpty(state)) {
        PG_RETURN_POINTER(input);
    }
    if (RangeIsEmpty(input)) {
        PG_RETURN_POINTER(state);
    }
    
    typcache = lookup_type_cache(state->rangetypid, TYPECACHE_RANGE_INFO);
    
    // deserialize, compare, serialize, return
    state_i4r = deserialize_RangeType(state, typcache);
    input_i4r = deserialize_RangeType(input, typcache);
    result_i4r = min_range(state_i4r, input_i4r);
    result = serialize_RangeType(result_i4r, typcache);

    PG_RETURN_POINTER(result);
}

/*
State Transition function for max aggregate
Returns the maximum LB and UB of all ranges in column.
Simply deserializes data, operates on it, and serializes 
    State = Int4Range = [a,b)
    Input = Int4Range = [c,d)
    Return RangeType: [max(a,c), max(b,d))
*/
Datum
agg_max_range_transfunc(PG_FUNCTION_ARGS)
{
    Int4Range state_i4r, input_i4r, result_i4r;
    RangeType *state, *input, *result;
    TypeCacheEntry *typcache;

    // first call: use the first input as initial state, or non null
    if (PG_ARGISNULL(0)) {
        if (PG_ARGISNULL(1)) {
            PG_RETURN_NULL();
        }

        // othrwise value becomes the state
        PG_RETURN_POINTER(PG_GETARG_RANGE_P(1));
    }
    // NULL input: return current state unchanged
    if (PG_ARGISNULL(1)) {
        PG_RETURN_RANGE_P(PG_GETARG_RANGE_P(0));
    }
    
    state = PG_GETARG_RANGE_P(0);
    input = PG_GETARG_RANGE_P(1);
    
    // return non empty
    if (RangeIsEmpty(state)) {
        PG_RETURN_POINTER(input);
    }
    if (RangeIsEmpty(input)) {
        PG_RETURN_POINTER(state);
    }
    
    typcache = lookup_type_cache(input->rangetypid, TYPECACHE_RANGE_INFO);
    
    // deserialize, compare, serialize, return
    state_i4r = deserialize_RangeType(state, typcache);
    input_i4r = deserialize_RangeType(input, typcache);
    result_i4r = max_range(state_i4r, input_i4r);
    result = serialize_RangeType(result_i4r, typcache);

    PG_RETURN_POINTER(result);
}

/*
// Returns naturalElement Range if multiplicity is 0, otherwise original range. 
// naturalElement Range does not affect min/max calculation
*/
// FIXME will need to change the type of neutral element depending on what datatype the user is using
Int4Range
range_mult_combine_helper(Int4Range range, Int4Range mult, int neutralElement)
{
    // return neutral so doesn't affect the aggregate
    if(mult.lower == 0) {
        Int4Range result;
        result.isNull = true; //auto false, not using NULLs

        // have to adjust UB + 2 or LB -2 based on if pos or neg
        if (neutralElement <= 0) {
            result.lower = neutralElement;      //temp change to resolve crashing   
            result.upper = neutralElement + 2;
        }
        else {
            result.lower = neutralElement-2;      //temp change to resolve crashing   
            result.upper = neutralElement;
        }
        return result;
    }

    return range;
}

/*
// Returns naturalElement Set if multiplicity is 0, otherwise original Set. 
// naturalElement Set does not affect min/max calculation
*/
// FIXME will need to change the type of neutral element depending on what datatype the user is using
Int4RangeSet
set_mult_combine_helper(Int4RangeSet set1, Int4Range mult, int neutralElement)
{
    // return neutral so doesn't affect the aggregate
    if(mult.lower == 0) {
        Int4RangeSet result;
        result.count = 1;
        result.containsNull = false;
        result.ranges = palloc(sizeof(Int4Range));
        result.ranges[0].isNull = false;
    
        // have to adjust UB + 2 or LB -2 based on if pos or neg
        if (neutralElement <= 0) {
            result.ranges[0].lower = neutralElement + 1;      //temp change to resolve crashing   
            result.ranges[0].upper = neutralElement + 10;
        }
        else {
            result.ranges[0].lower = neutralElement-10;      //temp change to resolve crashing   
            result.ranges[0].upper = neutralElement -1;
        }
        return result;
    }

    return set1;
}


/*
// To be called inside a MIN aggregation call. This multiplies the range and multuplicity together.
// neutral_element is the only difference between min/max implementation. This value is HARDCODED //FIXME
// Returns: a RangeType Datum as argument to MIN()
*/
Datum
combine_range_mult_min(PG_FUNCTION_ARGS) 
{
    RangeType *range_input, *mult_input, *output;
    Int4Range range, mult, result;
    int neutral_element;
    TypeCacheEntry *typcacheRange, *typcacheMult;
    
    // need better handling of mult null
    CHECK_BINARY_PGARG_NULL_OR();

    range_input = PG_GETARG_RANGE_P(0);
    mult_input = PG_GETARG_RANGE_P(1);
    typcacheRange = lookup_type_cache(range_input->rangetypid, TYPECACHE_RANGE_INFO);
    typcacheMult = lookup_type_cache(mult_input->rangetypid, TYPECACHE_RANGE_INFO);
    
    // hardcoded //FIXME
    neutral_element = INT_MAX;

    range = deserialize_RangeType(range_input, typcacheRange);
    mult = deserialize_RangeType(mult_input, typcacheMult);

    result = range_mult_combine_helper(range, mult, neutral_element);    
    output = serialize_RangeType(result, typcacheRange);

    PG_RETURN_RANGE_P(output);
}

/*
// To be called inside a MAX aggregation call. This multiplies the range and multuplicity together.
// neutral_element is the only difference between min/max implementation. This value is HARDCODED //FIXME
// Returns: a RangeType Datum as argument to MAX()
*/
Datum
combine_range_mult_max(PG_FUNCTION_ARGS) 
{
    RangeType *range_input, *mult_input, *output;
    Int4Range range, mult, result;
    int neutral_element;
    TypeCacheEntry *typcacheRange, *typcacheMult;
    
    CHECK_BINARY_PGARG_NULL_OR();
    
    range_input = PG_GETARG_RANGE_P(0);
    mult_input = PG_GETARG_RANGE_P(1);
    typcacheRange = lookup_type_cache(range_input->rangetypid, TYPECACHE_RANGE_INFO);
    typcacheMult = lookup_type_cache(mult_input->rangetypid, TYPECACHE_RANGE_INFO);

    // hardcoded //FIXME
    neutral_element = INT_MIN;

    range = deserialize_RangeType(range_input, typcacheRange);
    mult = deserialize_RangeType(mult_input, typcacheMult);

    result = range_mult_combine_helper(range, mult, neutral_element);
    output = serialize_RangeType(result, typcacheRange);

    PG_RETURN_RANGE_P(output);
}

/*
// To be called inside a MIN aggregation call. This multiplies the Set and multiplicity together.
// neutral_element is the only difference between min/max implementation. This value is HARDCODED //FIXME
// Parameter: ArrayType (data col), RangeType (multiplicity)
// Returns: a ArrayType Datum as argument to MIN()
*/
Datum
combine_set_mult_min(PG_FUNCTION_ARGS) 
{
   // inputs/ outputs
    ArrayType *set_input, *output;
    RangeType *mult_input;
    
    // working type
    Int4Range mult;
    Int4RangeSet set1, result;

    int neutral_element;
    TypeCacheEntry *typcacheSet, *typcacheMult;
    
    CHECK_BINARY_PGARG_NULL_OR();
    
    set_input = PG_GETARG_ARRAYTYPE_P(0);
    mult_input = PG_GETARG_RANGE_P(1);

    typcacheSet = lookup_type_cache(set_input->elemtype, TYPECACHE_RANGE_INFO);
    typcacheMult = lookup_type_cache(mult_input->rangetypid, TYPECACHE_RANGE_INFO);

    // hardcoded //FIXME
    neutral_element = INT_MAX;

    // deserialize, operate on, serialize, return
    set1 = deserialize_ArrayType(set_input, typcacheSet);
    mult = deserialize_RangeType(mult_input, typcacheMult);

    result = set_mult_combine_helper(set1, mult, neutral_element);
    output = serialize_ArrayType(result, typcacheSet);

    PG_RETURN_ARRAYTYPE_P(output);
}

/*
// To be called inside a MAX aggregation call. This multiplies the Set and multiplicity together.
// neutral_element is the only difference between min/max implementation. This value is HARDCODED //FIXME
// Parameter: ArrayType (data col), RangeType (multiplicity)
// Returns: a ArrayType Datum as argument to MAX()
*/
Datum
combine_set_mult_max(PG_FUNCTION_ARGS) 
{
    // inputs/ outputs
    ArrayType *set_input, *output;
    RangeType *mult_input;
    
    // working type
    Int4Range mult;
    Int4RangeSet set1, result;

    int neutral_element;
    TypeCacheEntry *typcacheSet, *typcacheMult;
    
    CHECK_BINARY_PGARG_NULL_OR();
    
    set_input = PG_GETARG_ARRAYTYPE_P(0);
    mult_input = PG_GETARG_RANGE_P(1);

    typcacheSet = lookup_type_cache(set_input->elemtype, TYPECACHE_RANGE_INFO);
    typcacheMult = lookup_type_cache(mult_input->rangetypid, TYPECACHE_RANGE_INFO);

    // hardcoded //FIXME
    neutral_element = INT_MIN;

    // deserialize, operate on, serialize, return
    set1 = deserialize_ArrayType(set_input, typcacheSet);
    mult = deserialize_RangeType(mult_input, typcacheMult);

    result = set_mult_combine_helper(set1, mult, neutral_element);
    output = serialize_ArrayType(result, typcacheSet);

    PG_RETURN_ARRAYTYPE_P(output);
}

/*
// State Transition function for min aggregate
// Returns the minimum LB and UB of all ranges in column.
// Simply deserializes data, operates on it, and serializes back to ArrayType
//     State = ArrayType = {[a,b) ...}       //(implicit) 
//     Input = ArrayType = [c,d) ... }
// Return ArrayType: {[min(a,c), min(b,d)) for all ranges}
*/
Datum
agg_min_set_transfunc(PG_FUNCTION_ARGS)
{
    Int4RangeSet state_i4r, input_i4r, n_state_i4r, n_input_i4r, result_i4r;
    ArrayType *state, *input, *result;
    TypeCacheEntry *typcache;

    // first call: use the first input as initial state, or non null
    if (PG_ARGISNULL(0)){
        if (PG_ARGISNULL(1)){
            PG_RETURN_NULL();
        }
        // othrwise value becomes the state
        PG_RETURN_ARRAYTYPE_P(PG_GETARG_ARRAYTYPE_P(1));
    }

    // NULL input: return current state unchanged
    if(PG_ARGISNULL(1)) {
        PG_RETURN_ARRAYTYPE_P(PG_GETARG_ARRAYTYPE_P(0));
    }

    // compare existing min/state to the current input
    state = PG_GETARG_ARRAYTYPE_P(0);
    input = PG_GETARG_ARRAYTYPE_P(1);

    // handle empty array, or array with just null maybe
    // if (array_contains_nulls())

    typcache = lookup_type_cache(state->elemtype, TYPECACHE_RANGE_INFO);

    state_i4r = deserialize_ArrayType(state, typcache);
    input_i4r = deserialize_ArrayType(input, typcache);

    n_state_i4r = normalize(state_i4r);
    n_input_i4r = normalize(input_i4r);

    result_i4r = min_rangeSet(n_state_i4r, n_input_i4r);
    result = serialize_ArrayType(result_i4r, typcache);

    PG_RETURN_ARRAYTYPE_P(result);
}

/*
// State Transition function for max aggregate
// Returns the minimum LB and UB of all ranges in column.
// Simply deserializes data, operates on it, and serializes back to ArrayType
//     State = ArrayType = {[a,b) ...}       //(implicit) 
//     Input = ArrayType = [c,d) ... }
// Return ArrayType: {[max(a,c), max(b,d)) for all ranges}
*/
Datum
agg_max_set_transfunc(PG_FUNCTION_ARGS)
{
    Int4RangeSet state_i4r, input_i4r, n_state_i4r, n_input_i4r, result_i4r;
    ArrayType *state, *input, *result;
    TypeCacheEntry *typcache;

    // first call: use the first input as initial state, or non null
    if (PG_ARGISNULL(0)){
        if (PG_ARGISNULL(1)){
            PG_RETURN_NULL();
        }
        // othrwise value becomes the state
        PG_RETURN_ARRAYTYPE_P(PG_GETARG_ARRAYTYPE_P(1));
    }

    // NULL input: return current state unchanged
    if(PG_ARGISNULL(1)) {
        PG_RETURN_ARRAYTYPE_P(PG_GETARG_ARRAYTYPE_P(0));
    }

    // compare existing min/state to the current input
    state = PG_GETARG_ARRAYTYPE_P(0);
    input = PG_GETARG_ARRAYTYPE_P(1);

    // handle empty array, or array with just null maybe
    // if (array_contains_nulls())

    typcache = lookup_type_cache(state->elemtype, TYPECACHE_RANGE_INFO);

    state_i4r = deserialize_ArrayType(state, typcache);
    input_i4r = deserialize_ArrayType(input, typcache);

    n_state_i4r = normalize(state_i4r);
    n_input_i4r = normalize(input_i4r);
    
    result_i4r = max_rangeSet(n_state_i4r, n_input_i4r);
    result = serialize_ArrayType(result_i4r, typcache);
    
    PG_RETURN_ARRAYTYPE_P(result);
}

// Simply just normalizes the result. Compressing any ranges if possible
Datum 
agg_min_max_set_finalfunc(PG_FUNCTION_ARGS)
{
    ArrayType *inputArray;
    ArrayType *output;

    // check for NULLS. Diff from empty check
    if (PG_ARGISNULL(0)){
        PG_RETURN_NULL();
    }

    inputArray = PG_GETARG_ARRAYTYPE_P(0);
    output = helperFunctions_helper(inputArray, normalize);
    PG_RETURN_ARRAYTYPE_P(output);
}

/*
    Only necessary for multiplicty. Takes in mult as param and counts the total number of possible ranges
*/
Datum
agg_count_transfunc(PG_FUNCTION_ARGS)
{
    RangeType *state;
    RangeType *input;
    RangeType *result;
    
    // first call: use the first input as initial state, or non null
    if (PG_ARGISNULL(0)){
        if (PG_ARGISNULL(1)){
            PG_RETURN_NULL();
        }
        // othrwise value becomes the state
        PG_RETURN_RANGE_P(PG_GETARG_RANGE_P(1));
    }

    // NULL input: return current state unchanged
    if(PG_ARGISNULL(1)) {
        PG_RETURN_RANGE_P(PG_GETARG_RANGE_P(0));
    }
    
    // get arguments, call helper to get result, check to normalize after
    state = PG_GETARG_RANGE_P(0);
    input = PG_GETARG_RANGE_P(1);

    result = arithmetic_range_helper(state, input, range_add_internal);

    PG_RETURN_ARRAYTYPE_P(result);
}


Datum 
agg_avg_range_transfunc(PG_FUNCTION_ARGS)
{
    MemoryContext aggcontext;
    MemoryContext oldcontext;
    RangeType *data;
    RangeType *mult;
    Int4Range curr;
    Int4Range m;
    Int4Range combSum;
    rAvgAggState *state;
    TypeCacheEntry *typcache;

    if (!AggCheckCallContext(fcinfo, &aggcontext)) {
        elog(ERROR, "avg_range_transfunc called in non-aggregate context");
    }

    // ignore NULL rows
    if (PG_ARGISNULL(1) || PG_ARGISNULL(2))
    {
        if (PG_ARGISNULL(0)) {
            PG_RETURN_NULL();
        }
        PG_RETURN_POINTER(PG_GETARG_POINTER(0));
    }
    
    // get curr State values
    data = PG_GETARG_RANGE_P(1);
    mult = PG_GETARG_RANGE_P(2);
    typcache = lookup_type_cache(data->rangetypid, TYPECACHE_RANGE_INFO);

    // first call: use the first input as initial state, or non null
    if (PG_ARGISNULL(0)){    
        // switch to aggregate memory context for persistent allocations
        oldcontext = MemoryContextSwitchTo(aggcontext);

        state = (rAvgAggState *) palloc0(sizeof(rAvgAggState));
        state->sum = deserialize_RangeType(data, typcache);
        state->count = deserialize_RangeType(mult, typcache);
        
        // need to return to callers context
        MemoryContextSwitchTo(oldcontext);
        
        PG_RETURN_POINTER(state);
    }

    // otherwise merge into existing state
    state = (rAvgAggState *) PG_GETARG_POINTER(0);

    curr = deserialize_RangeType(data, typcache);
    m = deserialize_RangeType(mult, typcache);
    
    combSum = range_mult_combine_helper_sum(curr, m, 0);
    range_add_internal(state->sum, combSum);
    range_add_internal(state->count, m);

    PG_RETURN_POINTER(state);
}

Datum 
agg_avg_range_finalfunc(PG_FUNCTION_ARGS)
{
    rAvgAggState *state;
    TypeCacheEntry *typcache;
    Int4Range avg;
    RangeType *result;

    if (PG_ARGISNULL(0)) {
        PG_RETURN_NULL();
    }

    state = (rAvgAggState*) PG_GETARG_POINTER(0);
    avg = range_divide_internal(state->sum, state->count);
    
    typcache = lookup_type_cache(INT4RANGEOID, TYPECACHE_RANGE_INFO);

    result = serialize_RangeType(avg, typcache);
    
    PG_RETURN_RANGE_P(result);
}
/////////////////
//// set avg ////
/////////////////
// Datum 
// agg_avg_set_transfunc(PG_FUNCTION_ARGS)
// {
//     MemoryContext aggcontext;
//     MemoryContext oldcontext;
//     ArrayType *data;
//     RangeType *mult;
//     Int4RangeSet curr;
//     Int4Range m;
//     Int4RangeSet combSum;
//     sAvgAggState *state;
//     TypeCacheEntry *typcache;

//     if (!AggCheckCallContext(fcinfo, &aggcontext)) {
//         elog(ERROR, "avg_range_transfunc called in non-aggregate context");
//     }

//     // ignore NULL rows
//     if (PG_ARGISNULL(1) || PG_ARGISNULL(2))
//     {
//         if (PG_ARGISNULL(0)) {
//             PG_RETURN_NULL();
//         }
//         PG_RETURN_POINTER(PG_GETARG_POINTER(0));
//     }
    
//     // get curr State values
//     data = PG_GETARG_RANGE_P(1);
//     mult = PG_GETARG_RANGE_P(2);
//     typcache = lookup_type_cache(data->rangetypid, TYPECACHE_RANGE_INFO);

//     // first call: use the first input as initial state, or non null
//     if (PG_ARGISNULL(0)){    
//         // switch to aggregate memory context for persistent allocations
//         oldcontext = MemoryContextSwitchTo(aggcontext);

//         state = (rAvgAggState *) palloc0(sizeof(rAvgAggState));
//         state->sum = deserialize_RangeType(data, typcache);
//         state->count = deserialize_RangeType(mult, typcache);
        
//         // need to return to callers context
//         MemoryContextSwitchTo(oldcontext);
        
//         PG_RETURN_POINTER(state);
//     }

//     // otherwise merge into existing state
//     state = (rAvgAggState *) PG_GETARG_POINTER(0);

//     curr = deserialize_RangeType(data, typcache);
//     m = deserialize_RangeType(mult, typcache);
    
//     combSum = range_mult_combine_helper_sum(curr, m, 0);
//     range_add_internal(state->sum, combSum);
//     range_add_internal(state->count, m);

//     PG_RETURN_POINTER(state);
// }

// Datum 
// agg_avg_set_finalfunc(PG_FUNCTION_ARGS)
// {
//     rAvgAggState *state;
//     TypeCacheEntry *typcache;
//     Int4Range avg;
//     RangeType *result;

//     if (PG_ARGISNULL(0)) {
//         PG_RETURN_NULL();
//     }

//     state = (rAvgAggState*) PG_GETARG_POINTER(0);
//     avg = range_divide_internal(state->sum, state->count);
    
//     typcache = lookup_type_cache(INT4RANGEOID, TYPECACHE_RANGE_INFO);

//     result = serialize_RangeType(avg, typcache);
    
//     PG_RETURN_RANGE_P(result);
// }







/*
*   transition function for sum(combine_set_mult_sum(data, mult), resizetrigger, sizelimit)
*   
* Parameters [4]:
*   - SumAggStateTest: (internal type)
*   - ArrayType: current state (result of combine_set_mult_sum(data, mult))
*   - Integer: resize trigger
*   - Integer: size limit
*   
* Returns [1]:
*   - Datum (pointer to SumAggState) 
*/
Datum
agg_sum_set_transfuncTest(PG_FUNCTION_ARGS)
{
    MemoryContext aggcontext;
    MemoryContext oldcontext;
    SumAggStateTest *state;
    ArrayType *currSet;
    TypeCacheEntry *typcache;
    Int4RangeSet inputSet, combined, normalized, newState;
    long currentSpan, currentCount;
    
    if (!AggCheckCallContext(fcinfo, &aggcontext))
        elog(ERROR, "agg_sum_set_transfunc called in non-aggregate context");
    
    // first call, state is NULL
    if (PG_ARGISNULL(0)) {

        // check for NULL input param, or empty
        if (PG_ARGISNULL(1)) {
            PG_RETURN_NULL();
        }
        
        currSet = PG_GETARG_ARRAYTYPE_P(1);
        typcache = lookup_type_cache(ARR_ELEMTYPE(currSet), TYPECACHE_RANGE_INFO);
        
        // empty set, continue on until non empty
        if (ArrayGetNItems(ARR_NDIM(currSet), ARR_DIMS(currSet)) == 0) {
            PG_RETURN_NULL();
        }

        // switch to aggregate memory context for persistent allocations
        oldcontext = MemoryContextSwitchTo(aggcontext);
        
        // internal state init
        state = (SumAggStateTest *) palloc0(sizeof(SumAggStateTest));
        state->ranges = deserialize_ArrayType(currSet, typcache);
        state->resizeTrigger = PG_GETARG_INT32(2);
        state->sizeLimit = PG_GETARG_INT32(3);
        state->callNormalize = PG_GETARG_BOOL(4);
        
        state->reduceCalls = 0;
        state->combineCalls = 1;
        state->maxIntervalCount = state->ranges.count;
        state->totalIntervalCount = state->ranges.count;
        state->minEffectiveIntervalCount = 0;
        state->convergedToTotSize = 0;
        
        // need to return to callers context
        MemoryContextSwitchTo(oldcontext);
        
        PG_RETURN_POINTER(state);
    }
    
    // otherwise merge into existing state
    state = (SumAggStateTest *) PG_GETARG_POINTER(0);

    if (!PG_ARGISNULL(1)) {
        currSet = PG_GETARG_ARRAYTYPE_P(1);
        typcache = lookup_type_cache(ARR_ELEMTYPE(currSet), TYPECACHE_RANGE_INFO);

        // empty check
        if (ArrayGetNItems(ARR_NDIM(currSet), ARR_DIMS(currSet)) == 0) {
            PG_RETURN_POINTER(state);
        }
        
        // deserialize input in current context (freed later)
        inputSet = deserialize_ArrayType(currSet, typcache);
        
        // agg context persists data
        oldcontext = MemoryContextSwitchTo(aggcontext);
        combined = range_set_add_internal(state->ranges, inputSet);
        
        // metadata
        state->combineCalls++;
        state->totalIntervalCount += combined.count;
        if (combined.count > state->maxIntervalCount) {
            state->maxIntervalCount = combined.count;
        }

        // free old ranges
        if (state->ranges.ranges != NULL) {
            pfree(state->ranges.ranges);
        }
        
        // check reduce size
        if (combined.count >= state->resizeTrigger) {
            newState = reduceSize(combined, state->sizeLimit);
            pfree(combined.ranges);
            state->reduceCalls++;
        }
        else {
            newState = combined;
            // NOTE test normalize here and not here
        }

        if (state->callNormalize) {
            normalized = normalize(newState);
            if (newState.ranges != combined.ranges)
                pfree(newState.ranges);

            newState = normalized;
        }
        state->ranges = newState;
        
        // attemp to track when collapse occurs and min width/ num ranges
        currentSpan  = totalSpan(newState);
        currentCount = newState.count;
        
        
        // elog(INFO, "FINAL span = %ld, count = %ld",
        //     currentSpan,
        //     currentCount);
            
        // if (state->minEffectiveIntervalCount == 0) {
        //     state->minEffectiveIntervalCount = currentCount;
        //     state->convergedToTotSize = currentSpan;
        // }
        // else if (currentCount < state->minEffectiveIntervalCount) {
        //     state->minEffectiveIntervalCount = currentCount;
        //     state->convergedToTotSize = currentSpan;
        // }
        // else if (currentCount == state->minEffectiveIntervalCount &&
        //         currentSpan < state->convergedToTotSize) {
        //     state->convergedToTotSize = currentSpan;
        // }

        // elog(INFO, "FINAL span = %ld, count = %ld",
        //     currentSpan,
        //     currentCount);
        
        // free previous memory context
        MemoryContextSwitchTo(oldcontext);        
        pfree(inputSet.ranges);
    }
    
    PG_RETURN_POINTER(state);
    }

/*
    returns a composite type containing:
    * result
    * resize trigger
    * resize limit
    * number of calls to reduce
    * peak number of intervals seen
    * total intervals count
    * number of times merged new input = num rows in dataset
*/
Datum
agg_sum_set_finalfuncTest(PG_FUNCTION_ARGS)
{
    SumAggStateTest *state;
    Int4RangeSet normResult;
    Datum values[9];
    bool nulls[9] = {false,false,false,false,false,false,false,false,false};
    HeapTuple tuple;
    TupleDesc tupdesc;
    ArrayType *arr;
    Oid elemTypeOID;
    TypeCacheEntry *typcache;
    long currentSpan, currentCount;


    if (PG_ARGISNULL(0)) {
        PG_RETURN_NULL();
    }
    
    state = (SumAggStateTest*) PG_GETARG_POINTER(0);
    
    elemTypeOID = TypenameGetTypid("int4range");
    if (elemTypeOID == InvalidOid)
        elog(ERROR, "int4range type not found in catalog");

    typcache = lookup_type_cache(elemTypeOID, TYPECACHE_RANGE_INFO);

    normResult = normalize(state->ranges);

    // elog(INFO, "FINAL span = %ld, count = %ld",
    //  totalSpan(normResult),
    //  normResult.count);

    // attemp to track when collapse occurs and min width/ num ranges
    currentSpan  = totalSpan(normResult);
    currentCount = normResult.count;
    // if (state->minEffectiveIntervalCount == 0) {
    //     state->minEffectiveIntervalCount = currentCount;
    //     state->convergedToTotSize = currentSpan;
    // }
    // else if (currentCount < state->minEffectiveIntervalCount) {
    //     state->minEffectiveIntervalCount = currentCount;
    //     state->convergedToTotSize = currentSpan;
    // }
    // else if (currentCount == state->minEffectiveIntervalCount &&
    //         currentSpan < state->convergedToTotSize) {
    //     state->convergedToTotSize = currentSpan;
    // }
    state->minEffectiveIntervalCount = currentCount;
    state->convergedToTotSize = currentSpan;
    // elog(INFO, "FINAL span = %ld, count = %ld",
    //  totalSpan(normResult),
    //  normResult.count);

    arr = serialize_ArrayType(normResult, typcache);
    values[0] = PointerGetDatum(arr);
    values[1] = Int64GetDatum(state->resizeTrigger);
    values[2] = Int64GetDatum(state->sizeLimit);
    values[3] = Int64GetDatum(state->reduceCalls);
    values[4] = Int64GetDatum(state->maxIntervalCount);
    values[5] = Int64GetDatum(state->totalIntervalCount);
    values[6] = Int64GetDatum(state->combineCalls);
    values[7] = Int64GetDatum(state->minEffectiveIntervalCount);
    values[8] = Int64GetDatum(state->convergedToTotSize);

    // get the composite tuple descriptor
    if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
        elog(ERROR, "return type must be composite");
    BlessTupleDesc(tupdesc);

    tuple = heap_form_tuple(tupdesc, values, nulls);
    return HeapTupleGetDatum(tuple);
}



Datum
agg_sum_set_transfuncTestNN(PG_FUNCTION_ARGS)
{
    MemoryContext aggcontext;
    MemoryContext oldcontext;
    SumAggStateTest *state;
    ArrayType *currSet;
    TypeCacheEntry *typcache;
    Int4RangeSet inputSet, combined, normalized, newState;
    
    if (!AggCheckCallContext(fcinfo, &aggcontext))
        elog(ERROR, "agg_sum_set_transfunc called in non-aggregate context");
    
    // first call, state is NULL
    if (PG_ARGISNULL(0)) {

        // check for NULL input param, or empty
        if (PG_ARGISNULL(1)) {
            PG_RETURN_NULL();
        }
        
        currSet = PG_GETARG_ARRAYTYPE_P(1);
        typcache = lookup_type_cache(ARR_ELEMTYPE(currSet), TYPECACHE_RANGE_INFO);
        
        // empty set, continue on until non empty
        if (ArrayGetNItems(ARR_NDIM(currSet), ARR_DIMS(currSet)) == 0) {
            PG_RETURN_NULL();
        }

        // switch to aggregate memory context for persistent allocations
        oldcontext = MemoryContextSwitchTo(aggcontext);
        
        // internal state init
        state = (SumAggStateTest *) palloc0(sizeof(SumAggStateTest));
        state->ranges = deserialize_ArrayType(currSet, typcache);
        state->resizeTrigger = PG_GETARG_INT32(2);
        state->sizeLimit = PG_GETARG_INT32(3);
        state->callNormalize = PG_GETARG_BOOL(4);
        
        state->reduceCalls = 0;
        state->combineCalls = 1;
        state->maxIntervalCount = state->ranges.count;
        state->totalIntervalCount = state->ranges.count;
        
        // need to return to callers context
        MemoryContextSwitchTo(oldcontext);
        
        PG_RETURN_POINTER(state);
    }
    
    // otherwise merge into existing state
    state = (SumAggStateTest *) PG_GETARG_POINTER(0);

    if (!PG_ARGISNULL(1)) {
        currSet = PG_GETARG_ARRAYTYPE_P(1);
        typcache = lookup_type_cache(ARR_ELEMTYPE(currSet), TYPECACHE_RANGE_INFO);

        // empty check
        if (ArrayGetNItems(ARR_NDIM(currSet), ARR_DIMS(currSet)) == 0) {
            PG_RETURN_POINTER(state);
        }
        
        // deserialize input in current context (freed later)
        inputSet = deserialize_ArrayType(currSet, typcache);
        
        // agg context persists data
        oldcontext = MemoryContextSwitchTo(aggcontext);
        combined = range_set_add_internal(state->ranges, inputSet);
        
        state->combineCalls++;
        state->totalIntervalCount += combined.count;
        if (combined.count > state->maxIntervalCount) {
            state->maxIntervalCount = combined.count;
        }

        // free old ranges
        if (state->ranges.ranges != NULL) {
            pfree(state->ranges.ranges);
        }
        
        // check reduce size
        if (combined.count >= state->resizeTrigger) {
            newState = reduceSizeNN(combined, state->sizeLimit);
            pfree(combined.ranges);
            state->reduceCalls++;
        }
        else {
            newState = combined;
            // NOTE test normalize here and not here
        }

        if (state->callNormalize) {
            normalized = normalize(newState);
            if (newState.ranges != combined.ranges)
                pfree(newState.ranges);

            newState = normalized;
        }

        state->ranges = newState;

        
        MemoryContextSwitchTo(oldcontext);
        
        // free previous memory context
        pfree(inputSet.ranges);
    }
    
    PG_RETURN_POINTER(state);
}