from collections import defaultdict
from cliUtility import *
from DataTypes import *
from dataclasses import replace
from main import ExperimentGroup, format_name, ExperimentSuite

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

def gap_size_sweep(max_gs: int = 1000, step:int =100,  n: int = 10_000, trigger_size: int = 10, reduce_to_size: int = 5):
    
    group = ExperimentGroup(f'gs{max_gs}_red{trigger_size}_{reduce_to_size}_sweep', 'gap_size_range', None)
    
    for gap in range(1, max_gs+1, step):
        experiment = replace(
            template,
            dataset_size             = n,
            num_trials               = 5,
            uncertain_ratio          = 0.0,
            independent_variable     = 'gap_size_range',
            interval_size_range      = (1, 10_000),
            start_interval_range     = (1, 2),
            gap_size_range           = (1000, 5000),
            interval_width_range     = (2, 15),
            num_intervals            = 4,
            reduce_triggerSz_sizeLim = (trigger_size, reduce_to_size),
        )

        experiment.name = format_name(experiment)
        group.experiments[experiment.name] = experiment

    return group

def plot_all_gap_sweep(max_gs: int, n: int, suite_name: str = None):
    
    suite_name = suite_name if suite_name is not None else 'gap_size_sweeping'
    if suite_name not in experiments:
        experiments[suite_name] = ExperimentSuite(suite_name)
    
    suite = experiments[suite_name]
    
    # suite.add(gap_size_sweep(max_gs, n, 15, 10))
    # suite.add(gap_size_sweep(max_gs, n, 10, 5))
    # suite.add(gap_size_sweep(max_gs, n, 4, 2))
    # suite.add(gap_size_sweep(max_gs, n, 9, 3))
    # suite.add(gap_size_sweep(max_gs, n, 5, 2))
    suite.add(gap_size_sweep(max_gs, n, 3, 1))
    suite.add(gap_size_sweep(max_gs, n, 1, 1))

# plot_all_gap_sweep(10, 10_000, 'gap_size_sweeping10k')



def wide_gap_sweep(max_gs: int = 1000, step:int =100,  n: int = 10_000, trigger_size: int = 10, reduce_to_size: int = 5):
    
    
    group = ExperimentGroup(f'gs{max_gs}_red{trigger_size}_{reduce_to_size}_sweep', 'gap_size_range', None)
    
    for gap in range(1, max_gs+1, step):
        experiment = replace(
            template,
            dataset_size             = n,
            num_trials               = 5,
            uncertain_ratio          = 0.0,
            independent_variable     = 'gap_size_range',
            interval_size_range      = (1, 10_000),
            start_interval_range     = (1, 2),
            gap_size_range           = (1000, 5000),
            interval_width_range     = (2, 15),
            num_intervals            = 4,
            reduce_triggerSz_sizeLim = (trigger_size, reduce_to_size),
        )

        experiment.name = format_name(experiment)
        group.experiments[experiment.name] = experiment

    return group