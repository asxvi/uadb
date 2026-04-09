-- (1) when referring to prune_op(...), 
-- we mean calling prune_set_and(prune_op_left, prune_op_right)

-- need to handle 3 cases:
-- prune pre aggregation == WHERE
-- prune post aggregation == having
-- prune pre and post


------------------------------------------------------------
-- PRE AGGREGATE: SELECT sum(a) FROM r WHERE a < 5;
-- baseline
SELECT 
    sum(combine_set_mult_sum(val, mult), 500, 100) 
FROM r
WHERE val < cond;

-- pruned
SELECT 
    sum(combine_set_mult_sum(
        prune_set_lt(val, cond, direction),
         mult), 500, 100) 
FROM r;

-- or is it. union of left and right, right?
SELECT 
    sum(combine_set_mult_sum(
        prune_set_and(
            prune_set_lt(val, cond, false),
            prune_set_lt(val, cond, true),
        ),
        mult),500, 100)
FROM r;

------------------------------------------------------------
-- POST AGGREGATE: SELECT sum(a) FROM r HAVING sum(a) < 5;
-- baseline
SELECT 
    prune_set_lt(
        sum(combine_set_mult_sum(val, mult), 500, 100),
        cond, direction)
FROM r

SELECT 
    sum(combine_set_mult_sum(val, mult), 500, 100) res
FROM r
HAVING prune_set_lt(res, cond, direction)



------------------------------------------------------------
-- PRE AND POST AGRGEGATE: SELECT sum(a) FROM r WHERE a < 5 HAVING sum(a) > 2;
-- baseline
SELECT     
    sum(combine_set_mult_sum(val, mult), 500, 100) as res
FROM r
WHERE prune_set_lt(val, cond, direction)
HAVING prune_set_gt(res, cond direction)

-- rewrite
SELECT 
    prune_set_gt(
        sum(combine_set_mult_sum(
            prune_set_lt(val, cond_high, false),
            mult), 500, 100),
        cond_low, false)
FROM r;