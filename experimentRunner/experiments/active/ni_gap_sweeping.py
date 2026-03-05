from dataclasses import replace

from cliUtility import *
from DataTypes import *
from main import ExperimentGroup, format_datasize, format_name, ExperimentSuite, make_log_sweep

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

def ni_gap_sweep2(gap_sizes: list, max_ni: int = 10, n_list: list = None, trigger_size: int = 10, reduce_to_size: int = 5, gap_width: int = 1):

    if n_list is None:
        n_list = []

    group = ExperimentGroup(f'ni_gap_red{trigger_size}_{reduce_to_size}_sweep', 'gap_size_range', None)
    
    for n in n_list:
        for g in gap_sizes:
            # for ni in range(1, max_ni+1):
                ni = 5
                experiment = replace(
                    template,
                    dataset_size             = n,
                    num_trials               = 1,
                    uncertain_ratio          = 0.0,
                    independent_variable     = 'gap_size_range',
                    interval_size_range      = (1, 100_000),
                    start_interval_range     = (1, 500),
                    gap_size_range           = (g, g + gap_width),  # fix the gap size
                    interval_width_range     = (5, 6),
                    num_intervals            = ni,
                    reduce_triggerSz_sizeLim = (trigger_size, reduce_to_size),
                )
                # experiment.name = f"{format_name(experiment)}_g{g}"  # make sure gap_size is in name to avoid overwrite
                experiment.name = f"{format_name(experiment)}_g{g}_ni{ni}_n{n}_r{trigger_size}_{reduce_to_size}"
                group.experiments[experiment.name] = experiment
        
    return group

def plot_ni_gap_sweep2(max_ni: int, n_list: list, suite_name: str = None):
    suite_name = suite_name if suite_name is not None else f'ni_gap_sweeping{format_datasize(n_list[-1])}'
    if suite_name not in experiments:
        experiments[suite_name] = ExperimentSuite(suite_name)
    
    gap_sizes = [10, 50, 100, 250, 500, 1000, 2000, 6000, 10000]
    # red_params = [(15, 10), (10, 5), (4, 2), (9, 3), (5, 2), (1, 1), (3,1)]

    
    experiments[suite_name].add(ni_gap_sweep2(gap_sizes, max_ni, n_list, 150, 100, 1000))
    experiments[suite_name].add(ni_gap_sweep2(gap_sizes, max_ni, n_list, 150, 10, 1000))
    experiments[suite_name].add(ni_gap_sweep2(gap_sizes, max_ni, n_list, 70, 50, 1000))
    experiments[suite_name].add(ni_gap_sweep2(gap_sizes, max_ni, n_list, 70, 10, 1000))
    experiments[suite_name].add(ni_gap_sweep2(gap_sizes, max_ni, n_list, 15, 10, 1000))
    experiments[suite_name].add(ni_gap_sweep2(gap_sizes, max_ni, n_list, 10, 5, 1000))
    experiments[suite_name].add(ni_gap_sweep2(gap_sizes, max_ni, n_list, 4, 2, 1000))
    experiments[suite_name].add(ni_gap_sweep2(gap_sizes, max_ni, n_list, 9, 3, 1000))
    experiments[suite_name].add(ni_gap_sweep2(gap_sizes, max_ni, n_list, 5, 2, 1000))
    experiments[suite_name].add(ni_gap_sweep2(gap_sizes, max_ni, n_list, 3, 1, 1000))
    # experiments[suite_name].add(ni_gap_sweep(gap_sizes, max_ni, n_list, 1, 1, 1000))

n_list = make_log_sweep(1, 2000, 40)
plot_ni_gap_sweep2(5, n_list)




# bigger gaps
# diff start points [gap, 10*gap, ]
# smaller n -> gb agg
# runtimes



