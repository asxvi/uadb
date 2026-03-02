from collections import defaultdict
from cliUtility import *
from DataTypes import *
from dataclasses import replace
from main import ExperimentGroup, format_datasize, format_name, ExperimentSuite

'''
experiments is a dict of {str: ExperimentGroup}. ALlows for many unrelated experiments to run from 1 file
Naming convention is "GroupName/ID": {ExperimentGroup of related experiments}
persists in namespace of caller program
'''
experiments = dict()

# dummy used to access members
template = ExperimentSettings(
    data_type=DataType.SET, 
    dataset_size=10_000, 
    uncertain_ratio=0.0, 
    mult_size_range=(1,5),
    interval_size_range=(1, 1000), 
    num_intervals=4, 
    mode="all",
    num_trials=3, 
    gap_size_range=(0,100), 
    name= "temp",
    reduce_triggerSz_sizeLim=(10, 5),
)

def num_intervals_sweep(max_ni: int = 4, n: int = 10_000, trigger_size: int = 10, reduce_to_size: int = 5):
    group = ExperimentGroup(f'ni{max_ni}_red{trigger_size}_{reduce_to_size}_sweep', 'num_intervals', None)
    
    # for ni in range(1, max_ni+1, 1):
    if 1 == 1:
        ni = 5
        experiment = replace(
            template,
            dataset_size             = n,
            num_trials               = 1,
            uncertain_ratio          = 0.0,
            independent_variable     = 'num_intervals',
            interval_size_range      = (1, 100_000),
            start_interval_range     = (1, 5),
            gap_size_range           = (2000, 2001),
            interval_width_range     = (5, 6),
            num_intervals            = ni,
            reduce_triggerSz_sizeLim = (trigger_size, reduce_to_size),
        )

        experiment.name = format_name(experiment)
        group.experiments[experiment.name] = experiment

    return group


# ================ #
def plot_all_ni_sweep(max_ni: int, n: int, suite_name: str = None):
    suite_name = suite_name if suite_name is not None else 'ni_sweeping'
    if suite_name not in experiments:
        experiments[suite_name] = ExperimentSuite(suite_name)
    
    suite = experiments[suite_name]

    # for i in range(1, 11, 1):
    # for trigger in [2, 5, 10, 20, 50, 100, 250, 500, 750, 1000, 2000]:
    #     suite.add(num_intervals_sweep(max_ni, n, trigger, 5))

        # suite.add(num_intervals_sweep(max_ni, n, 100, i))
    suite.add(num_intervals_sweep(max_ni, n, 100, 10))
    # suite.add(num_intervals_sweep(max_ni, n, 100, 10))
    # suite.add(num_intervals_sweep(max_ni, n, 100, 10))
    # suite.add(num_intervals_sweep(max_ni, n, 100, 40))
    # suite.add(num_intervals_sweep(max_ni, n, 100, 60))
    # suite.add(num_intervals_sweep(max_ni, n, 100, 80))
    # suite.add(num_intervals_sweep(max_ni, n, 100, 90))

## ============================== ##

plot_all_ni_sweep(10, 50, 'ni_sweepingn_50') 
plot_all_ni_sweep(10, 100, 'ni_sweepingn_100') 
plot_all_ni_sweep(10, 250, 'ni_sweepingn_250') 
plot_all_ni_sweep(10, 500, 'ni_sweepingn_500')



'''
parameter sweep
ni 
red trigger
red newSz

maybe n to see when the tbale starts converging 





-4 ,1000,1000,22,4725,79950,50)
-3 ,1000,1000,72,4925,158620,100) 
-2 ,1000,1000,222,4985,682870,250)
-1 ,1000,1000,392,5000,1512845,500) ("{""[1226,4013727)""}",

'''




