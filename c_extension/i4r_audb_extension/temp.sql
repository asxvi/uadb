SELECT
    set_reduce_size(
        sum(combine_set_mult_sum(val, mult), 500, 100), 100
    )
FROM t_s_iv_n_9ece6d82ac;


SELECT
    sum(combine_set_mult_sum(val, mult), 500, 100)
FROM t_s_iv_n_9ece6d82ac;


SELECT
    set_normalize(
        sum(combine_set_mult_sum(val, mult), 500, 100)
    )
FROM (
    select * from t_s_iv_n_9ece6d82ac limit 10
) sub;



------------------------------------

-- normalized
SELECT
    set_normalize(sum(combine_set_mult_sum(val, mult), 500, 100))
FROM (
    select * from t_s_iv_n_9ece6d82ac limit 100
) sub;


SELECT
    sum(combine_set_mult_sum(val, mult), 500, 100)
FROM (
    select * from t_s_iv_n_9ece6d82ac limit 100
) sub;

-- vs sumtest

SELECT
    sumtest(combine_set_mult_sum(val, mult), 500, 100, false)
FROM (
    select * from t_s_iv_n_9ece6d82ac limit 100
) sub;
------------------------------------



SELECT
    sumtest(combine_set_mult_sum(val, mult), 500, 100), 100
FROM t_s_iv_n_9ece6d82ac;

SELECT 
    (result).result,
    (result).resizeTrigger,
    (result).sizeLimit,
    (result).reduceCalls,
    (result).maxIntervalCount,
    (result).totalIntervalCount,
    (result).combineCalls,
    (result).minEffectiveIntervalCount,
    (result).convergedToTotSize
FROM (
    (SELECT sumTest(combine_set_mult_sum(val, mult), 500, 100, true) as result
    FROM (SELECT * FROM t_s_iv_n_9ece6d82ac limit 10))) subq;




-- prune manual rewrite
-- select adn then agrgegation

-- tracking coverage and time vs non prune coverage and time

select sum(a) 
from r

select sum(a) 
from r
where a > 5

==

select prune_lt
select sum(a) 
from r
where a > 5

select sum(a) 
from r
where a > 5
having sum() > b

-- more carefully selected outcomes for reduce params
-- experiments with pruning for 