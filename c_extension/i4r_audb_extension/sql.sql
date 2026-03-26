
-- select range_add(prune_a, lift_scalar(3)), *
-- from (select prune_range_lt(a,b,false) as prune_a, * from tx_r_prune_basic) subq
-- where range_lt(a,b) is not false;


select *, 
    prune_range_or( 
        prune_range_lt(a,b,true),
        prune_range_lt(a, lift_scalar(2), false)
    ) 
from tx_r_prune_basic;


select
     *, 
    prune_range_gt(a, lift_scalar(17), false), 
    prune_range_gt(a, lift_scalar(13), false) 
from tx_r_prune_basic;
-- where id = 1;