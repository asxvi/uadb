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
    for ni in range(1, max_ni+1, 1):
        experiment = replace(
            template,
            dataset_size             = n,
            num_trials               = 5,
            uncertain_ratio          = 0.0,
            independent_variable     = 'num_intervals',
            interval_size_range      = (1, 100_000),
            start_interval_range     = (1, 100),
            gap_size_range           = (10000, 15000),
            interval_width_range     = (100, 600),
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
    #     suite.add(num_intervals_sweep(max_ni, n, i+1, i))
    
    # suite.add(num_intervals_sweep(max_ni, n, 500, 10))
    # suite.add(num_intervals_sweep(max_ni, n, 500, 5))
    # suite.add(num_intervals_sweep(max_ni, n, 300, 10))
    # suite.add(num_intervals_sweep(max_ni, n, 300, 5))
    # suite.add(num_intervals_sweep(max_ni, n, 150, 10))
    # suite.add(num_intervals_sweep(max_ni, n, 150, 5))
    # suite.add(num_intervals_sweep(max_ni, n, 15, 10))
    # suite.add(num_intervals_sweep(max_ni, n, 10, 5))
    # suite.add(num_intervals_sweep(max_ni, n, 3, 2))
    suite.add(num_intervals_sweep(max_ni, n, 1, 1))


## ============================== ##

# plot_all_ni_n_sweep(10, [10000, 20000, 40000, 60000], 'ni_n_sweep')

plot_all_ni_sweep(15, 2_000, 'ni_sweeping10k')