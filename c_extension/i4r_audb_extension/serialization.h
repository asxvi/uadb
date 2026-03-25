#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include "helperFunctions.h"    // logic for helpers

// source files
#include "postgres.h"           // root
#include "fmgr.h"               // "..must be included by all Postgres modules"
#include "utils/rangetypes.h"   // RangeType
#include "utils/array.h"        // ArrayType
#include "utils/typcache.h"     // data type cache
#include "utils/lsyscache.h"    // "Convenience routines for common queries in the system catalog cache."
#include "catalog/pg_type_d.h"  // pg_type oid macros
#include "catalog/namespace.h"  // type helpers

Int4Range deserialize_RangeType(RangeType *rng, TypeCacheEntry *typcache);
RangeType* serialize_RangeType(Int4Range range, TypeCacheEntry *typcache);
RangeBound make_range_bound(int32 val, bool is_lower, bool inclusive);
Int4RangeSet deserialize_ArrayType(ArrayType *arr, TypeCacheEntry *typcache);
ArrayType* serialize_ArrayType(Int4RangeSet set, TypeCacheEntry *typcache);
ArrayType* serialize_ArrayType2(Int4RangeSet set, Oid rangeTypeOid, TypeCacheEntry *typcache);
#endif
