# standard
import pathlib
import sys
import random
import time
import os
import json
import hashlib
from dataclasses import dataclass

# 3rd party
from numerize import numerize
import pandas as pd
import psycopg2, psycopg2.extras
import numpy as np

# local
from cliUtility import *
from DataTypes import *
from StatisticsPlotter import *

@dataclass
class ExperimentGroup:
    '''Container class that stores groups of single IV experiments (Dicts of ExperimentSettings)'''
    name: str
    independent_variable: str
    experiments: dict = None
    
    def __post_init__(self):
        if self.experiments is None:
            self.experiments = {}

@dataclass
class ExperimentSuite:
    name: str
    groups: dict[str, ExperimentGroup] = None

    def __post_init__(self):
        if self.groups is None:
            self.groups = {}

    def add(self, group: ExperimentGroup):
        if group.name in self.groups:
            raise ValueError(f"Duplicate group name: {group.name}")
        self.groups[group.name] = group

@dataclass
class ExperimentSettings:
    '''
        Class contains the modifiable settings of a test
        if num_intervals is used, num_intervals_range shouldn't be used (left default=NULL)
        if gap_size is used, gap_size_range shouldn't be used (left default=NULL)

        - name:                   required. consider pairing with format_name(self) after setting attiributes
        - data_type:              always Set or Range
        - curr_trial:             internal
        - experiment_id:          unique string name that identifies specific experiment
        - dataset_size:           modifiable. number of rows to produce
        - uncertain_ratio:        modifiable. uncertainty in data
        - interval_size_range:     

        WIP

    '''
    name: str                                   # required 
    data_type: DataType                         # always Set or Range
    curr_trial: int = 0                         # keep track locally 
    experiment_id: str = None                   # unique string name that identifies specific experiment
    num_trials: int = 1                         # always fixed
    dataset_size: int = 100                     # always fixed
    uncertain_ratio: float = 0.00               # uncertainty ratio is split 50% in data, 50% in multiplicity columns. Uncert in data == NULL, uncert in mult = [0,N]
    interval_size_range: tuple = (1, 1000)      # the size of each interval
    mult_size_range: tuple = (1,5)              # required 
    independent_variable: str = None            # flag for what var we test. Used internally
    start_interval_range: tuple = (interval_size_range[0], interval_size_range[1])   
    reduce_triggerSz_sizeLim: tuple = (10,5)    # test this. need to figure out how to encode different techniques.
    
    # use these value if not None, otherwise use tuple if not None, both none = error
    interval_width: int = None
    interval_width_range: tuple = None
    num_intervals: int = None       
    gap_size: int = None
    num_intervals_range: tuple = None
    gap_size_range: tuple = None    
    
    mode: str = None                # NOT USED YET what modes of test suite to execute
    save_ddl:bool = False           # store ddl code to make tables 
    save_csv: bool = True           # store csv with statistics and results of test

    # shortened abbreviation of atributes
    iv_map = {
        "dataset_size": "n",
        "uncertain_ratio": "unc",
        "interval_size_range": "isr",
        "mult_size_range": "msr",
        "num_intervals": "ni_nir",
        "num_intervals_range": "ni_nir",
        "gap_size": "gs_gsr",
        "gap_size_range": "gs_gsr",
        "reduce_triggerSz_sizeLim": "red_sz"
    }

    def to_dict(self) -> dict:
        ''' convenience function converting class to dict''' 

        dt = 'range' if self.data_type == DataType.RANGE else 'set'
        return {
            'name': self.name,
            'data_type': dt,
            'curr_trial': self.curr_trial,
            'experiment_id': self.experiment_id,
            'num_trials': self.num_trials,
            'dataset_size': self.dataset_size,
            'uncertain_ratio': self.uncertain_ratio,
            "interval_size_range": self.interval_size_range,
            'mult_size_range': self.mult_size_range,
            'independent_variable': self.independent_variable,
            'start_interval_range': self.start_interval_range,
            'reduce_triggerSz_sizeLim': self.reduce_triggerSz_sizeLim,
            'interval_width': self.interval_width,
            'interval_width_range': self.interval_width_range,
            'num_intervals': self.num_intervals,
            'gap_size': self.gap_size,
            'num_intervals_range': self.num_intervals_range,
            'gap_size_range': self.gap_size_range
        }
    
class ExperimentRunner:
    '''
        ExperimentRunner runs entire or parts of a test (gen_data, insert_db)
    '''

    NORMALIZE = True
    
    DATA_TYPE_CONFIG = {
        DataType.RANGE: {
            "combine_sum": "combine_range_mult_sum",
            "combine_min": "combine_range_mult_min",
            "combine_max": "combine_range_mult_max",
        },
        DataType.SET: {
            "combine_sum": "combine_set_mult_sum",
            "combine_min": "combine_set_mult_min",
            "combine_max": "combine_set_mult_max",
        },
    }

    def __init__(self, db_config, seed):
        self.db_config = db_config
        self.results = []
        self.master_seed = seed
        self.trial_seed = None
        self.resultFilepath: str = None
        self.name = None
        self.groupName = None
        self.csv_paths = []

    def run_experiment(self, experiment: ExperimentSettings) -> list:
        '''an experiement has N trials. generate data for each trial, run queries//benchmark results, and append to results'''    
        # generate data for each trial. Insert ddl to file optinally. After inserting to DB, run tests
        experiment_results = []
        
        for trial in range(experiment.num_trials):
            # create a trial seed dependent on the master seed, and specific trial number
            experiment.curr_trial = trial+1
            self.trial_seed = (self.master_seed + experiment.curr_trial) % (2**32)
            np.random.seed(self.trial_seed)
            experiment.experiment_id = self.__generate_name(experiment)
            
            # get randomly generated data for curr seed
            db_data_format, file_data_format = self.generate_data(experiment)

            # DOES NOT WORK properly # save in ddl compatible format. Insert into DB regardless... need to run tests.
            if experiment.save_ddl:
                self.__save_ddl_file(experiment, file_data_format)
            
            self.__insert_data_db(experiment, db_data_format)

            # run queries and benchmark
            trial_results = self.run_queries(experiment)
            experiment_results.append(trial_results)

        aggregated_results = self.__calc_aggregate_results(experiment, experiment_results)
        self.results.append(aggregated_results)

        return experiment_results
        
    def generate_data(self, experiment :ExperimentSettings):
        '''
            Generates pseudorandom data based on user specified experiment settings. 
            * NOTE Specfic data serialization for different formats (i.e file and db ddl differs)
        '''
        db_formatted_rows = []
        file_formatted_rows = []
        
        for i in range(experiment.dataset_size):
            if experiment.data_type == DataType.RANGE:
                obj = self.__generate_range(experiment)
                val = str(obj) if not obj.isNone else None
            elif experiment.data_type == DataType.SET:
                obj = self.__generate_set(experiment)
                val = str(obj) if (obj.rset and not getattr(obj, 'isNone', False)) else None

            mult_obj = self.__generate_mult(experiment)
            mult = str(mult_obj)
            db_formatted_rows.append((val, mult))
            
            # save in ddl preffered format if requested
            if experiment.save_ddl:
                val = obj.str_ddl()
                mult = mult_obj.str_ddl()
                file_formatted_rows.append((val, mult))
                
        return db_formatted_rows, file_formatted_rows

    def run_queries(self, experiment: ExperimentSettings):
        '''Run aggregation tests and collect metrics.'''
        
        results = {
            'row_count' : 0,
            'min_time' : None,
            'max_time' : None,
            'sum_time' : None,
            'sumtest_time': None,
            'sum_test_result' : None,
            'reduce_calls' : None,
            'max_interval_count': None,
            'total_interval_count': None,
            'combine_calls': None,
            'result_size': None,
            'result_coverage': None,

        }
        table = experiment.experiment_id
        config = self.DATA_TYPE_CONFIG[experiment.data_type]

        try:
            with self.__connect_db() as conn:
                with conn.cursor() as cur:
                    # count 
                    cur.execute(f"SELECT COUNT(*) FROM {table};")
                    results['row_count'] = cur.fetchone()[0]
                    
                    # aggreate metrics
                    results['sum_time'] = self.__run_aggregate(cur, table, 'SUM', config['combine_sum'], experiment.reduce_triggerSz_sizeLim[0], experiment.reduce_triggerSz_sizeLim[1])
                    results['min_time'] = self.__run_aggregate(cur, table, 'MIN', config['combine_min'])
                    results['max_time'] = self.__run_aggregate(cur, table, 'MAX', config['combine_max'])
                    results['sumtest_time'] = self.__run_aggregate(cur, table, 'SUMTEST', config['combine_sum'], experiment.reduce_triggerSz_sizeLim[0], experiment.reduce_triggerSz_sizeLim[1], not self.NORMALIZE)
                    
                    # get additional tests for sumtest. Run experiment and time profile once each
                    metrics = self.__get_sumtest_metrics(cur, table, config['combine_sum'], experiment.reduce_triggerSz_sizeLim[0], experiment.reduce_triggerSz_sizeLim[1], not self.NORMALIZE)
                    
                    if metrics: 
                        results['sum_test_result'] = metrics['result']
                        results['reduce_calls'] = metrics['reduce_calls']
                        results['max_interval_count'] = metrics['max_interval_count']
                        results['total_interval_count'] = metrics['total_interval_count']
                        results['combine_calls'] = metrics['combine_calls']
                        results['result_size'] = metrics['result_size']
                        results['result_coverage'] = self.__calculate_coverage(metrics['result'])

        except Exception as e:
            print(f"Error running queries for {experiment.experiment_id}: {e}")
            raise
        
        return results
    
    def save_results(self, experiment: ExperimentSettings) -> str:
        ''' saves and returns CSV path of results experiment.resultFilepath'''
        
        if not self.results or not experiment.save_csv:
            return
        
        csv_name = f"results_{experiment.name}_sd{self.master_seed}.csv"
        csv_path = os.path.join(self.resultFilepath, csv_name)

        df = pd.DataFrame(self.results)
        df.to_csv(csv_path, index=True)         
        
        self.csv_paths.append(csv_path)
        print(f"  CSV saved: {csv_path}")

        return csv_path
    
    def clean_tables(self, find_trigger="t_%", batch_size=200):
        '''drop all tables with wildcard match {find_trigger}'''
        
        print(f"\nCleaning/ Dropping all Tables starting with '{find_trigger}'")
        with self.__connect_db() as conn:
            with conn.cursor() as cur:
                try:
                    cur.execute(f"""
                        SELECT tablename 
                        FROM pg_tables 
                        WHERE schemaname = 'public' 
                            AND tablename LIKE '{find_trigger}'
                        ORDER BY tablename;
                    """)
                    tables = [row[0] for row in cur.fetchall()]

                    if not tables:
                        print(f"  No tables found matching: {find_trigger}\n")
                        return

                    dropped = 0
                    for i in range(0, len(tables), batch_size):
                        batch = tables[i: i+batch_size]
                        
                        # with self.__connect_db() as tempConn:
                        for table in batch:
                            try:
                                cur.execute(f'DROP TABLE IF EXISTS "{table}" CASCADE;')
                                dropped+=1
                                # print(f"  Dropping Table {table[0]}")
                            except Exception as e:
                                print(f"    Failed to drop '{table}': {e}")
                                raise
                        conn.commit()
                        # print(f"  Committed batch {dropped}/{len(tables)} dropped so far")
                except Exception as e:
                    print(f"    Error cleaning tables: {e}")
        print(f"\nDropped {dropped} tables\n")

    def set_file_path(self, suite_name: str, group_name: str, experiment_name:str) -> None:
        """creates experiment folder path based on group and experiment name.
        if experiment_name is None, creates a folder for the entire group.

        Format:
        - with experiment: ./data/results/<group>/<experiment_name>_sd<seed>
        - group-only:    ./data/results/<group>/sd<seed>"""
        
        if experiment_name:
            # folder_name = f"{experiment_name}_sd{self.master_seed}"
            folder_name = f"{experiment_name}"
        else:
            folder_name = ""

        if group_name and suite_name:
            self.resultFilepath = os.path.join("data", "results", str(self.master_seed), suite_name, group_name, folder_name)
        else:
            self.resultFilepath = os.path.join("data", "results", str(self.master_seed), folder_name)

        os.makedirs(self.resultFilepath, exist_ok=True)
    
    # ----------------------------------  
    # --- Internal Helpers (Private) ---
    # ----------------------------------
    def __generate_range(self, experiment:ExperimentSettings) -> RangeType:
        # uncertain ratio. maybe should account for half nulls, half mult 0
        if np.random.random() < experiment.uncertain_ratio * 0.5:  
            return RangeType(0, 0, True)

        lb = np.random.randint(*experiment.interval_size_range)
        ub = np.random.randint(lb+1, experiment.interval_size_range[1]+1)
        return RangeType(lb, ub)
    
    def __generate_set(self, experiment:ExperimentSettings) -> RangeSetType:
        
        # if experiment.num_intervals is not None then use, otherwise if experiment.num_intervals_range then use. otherwise raise error
        if experiment.num_intervals is not None:
            num_intervals = experiment.num_intervals
        elif experiment.num_intervals_range is not None:
            num_intervals = np.random.randint(*experiment.num_intervals_range)
        else:
            raise ValueError("Either num_intervals or num_intervals_range must be specified")
        
        # entire set is unknown
        if np.random.random() < experiment.uncertain_ratio * 0.5:  
            return RangeSetType([], cu=False)
        
        rset = []

        # set the first starting point
        if experiment.start_interval_range is not None:
            start = np.random.randint(*experiment.start_interval_range)
        else:
            start = experiment.interval_size_range[0]

        # for each interval
        for i in range(num_intervals):    
            if np.random.random() < experiment.uncertain_ratio * 0.5:  
                continue
            
            # get the interval width
            if experiment.interval_width is not None:
                interval_width = experiment.interval_width
            elif experiment.interval_width_range is not None:
                interval_width = np.random.randint(*experiment.interval_width_range)
            else:
                raise ValueError("Either interval_width or interval_width_range must be specified")
            interval_end = start + interval_width

            rset.append(RangeType(start, interval_end, False))

            # find next gap if not last
            if i < num_intervals -1:
                if experiment.gap_size is not None:
                    gap = experiment.gap_size
                elif experiment.gap_size_range is not None:
                    gap = np.random.randint(*experiment.gap_size_range)
                else:
                    gap = 0  
                
                start = interval_end + gap

                # next next start exceeds bounds, we can't add more intervals
                if start >= experiment.interval_size_range[1]:
                    break

        return RangeSetType(rset, cu=False)

    def __generate_mult(self, experiment:ExperimentSettings) -> RangeType:
        # uncertain ratio. maybe should account for half nulls, half mult 0
        if np.random.random() < experiment.uncertain_ratio * 0.5:  
            return RangeType(0, 0, True)
        
        lb = np.random.randint(*experiment.mult_size_range)
        ub = np.random.randint(lb+1, experiment.mult_size_range[1]+1)
        return RangeType(lb, ub)
    
    def __insert_data_db(self, experiment: ExperimentSettings, data):
        '''Insert data into database specified in config file'''
        with self.__connect_db() as conn:
            with conn.cursor() as cur:
                table_name = experiment.experiment_id
                cur.execute(f"DROP TABLE IF EXISTS {table_name};")

                if experiment.data_type == DataType.RANGE:
                    cur.execute(f"CREATE TABLE {table_name} (id INT GENERATED ALWAYS AS IDENTITY, val int4range, mult int4range);")                
                    template = "(%s::int4range, %s::int4range)"
                elif experiment.data_type == DataType.SET:
                    cur.execute(f"CREATE TABLE {table_name} (id INT GENERATED ALWAYS AS IDENTITY, val int4range[], mult int4range);")
                    template = "(%s::int4range[], %s::int4range)"
                
                sql = f"INSERT INTO {table_name} (val, mult) VALUES %s"
                psycopg2.extras.execute_values(cur, sql, data, template)
                conn.commit()
    
    def __run_aggregate(self, cur, table, agg_name, combine_func, *agg_params):
        '''General aggregate runner with no WHERE clause'''

        params_sql = ",".join(str(param) for param in agg_params)
        sql = f"""EXPLAIN (analyze, format json)
            SELECT {agg_name} ({combine_func}(val, mult) {',' if params_sql else ''}{params_sql})
            FROM {table};"""
        cur.execute(sql)
        
        print(f"DEBUG SQL: {table}") 
        results = cur.fetchone()[0]
        plan_root = results[0]
        plan = plan_root["Plan"]
        agg_time = plan["Actual Total Time"]
        
        return agg_time
    
    def __get_aggregate_result(self, cur, table, agg_name, combine_func, *agg_params):
        '''get actual aggregate result value (no timing)'''
        
        params_sql = ",".join(str(param) for param in agg_params)
        sql = f"""
            SELECT {agg_name} ({combine_func}(val, mult) {',' if params_sql else ''}{params_sql})
            FROM {table};"""
        
        cur.execute(sql)
        result = cur.fetchone()
        
        if result is None:
            return None
        
        result_value = result[0]
        
        return result_value
    
    def __get_sumtest_metrics(self, cur, table, combine_func, trigger_sz, size_lim, normalize: bool):
        '''get SUMTEST metrics from composite type result using field accessors'''
        
        sql = f"""
            SELECT 
                (result).result,
                (result).resizeTrigger,
                (result).sizeLimit,
                (result).reduceCalls,
                (result).maxIntervalCount,
                (result).totalIntervalCount,
                (result).combineCalls
            FROM (
                SELECT sumTest({combine_func}(val, mult), {trigger_sz}, {size_lim}, {normalize}) as result
                FROM {table}) subq;"""
        
        cur.execute(sql)
        result = cur.fetchone()     
        if result is None:
            return None
        
        result_array = result[0] 
        resize_trigger = result[1]
        size_limit = result[2]
        reduce_calls = result[3]
        max_interval_count = result[4]
        total_interval_count = result[5]
        combine_calls = result[6]

        metrics = {
            'result': result_array,             # list of NumericRange objects
            'resize_trigger': resize_trigger,
            'size_limit': size_limit,
            'reduce_calls': reduce_calls,
            'max_interval_count': max_interval_count,
            'total_interval_count': total_interval_count,
            'combine_calls': combine_calls,
            'result_size': len(result_array) if result_array else 0,
        }
    
        return metrics
           
    def __calculate_coverage(self, interval_set):
        '''adds all values contained within every interval in set'''
        cover = 0
        for interval in interval_set:
            cover += interval.upper - interval.lower
        return cover

    def __calc_aggregate_results(self, experiment: ExperimentSettings, trial_results: dict) -> dict:
        ''' combines all result data, and returns dict of all experiment metadata leter used to convert to CSV'''

        def extract(key):
            return [r[key] for r in trial_results if r.get(key) is not None]

        min_times = extract('min_time')
        max_times = extract('max_time')
        sum_times = extract('sum_time')
        sumtest_times = extract('sumtest_time')
        reduce_calls = extract('reduce_calls')
        max_intervals = extract('max_interval_count')
        total_intervals = extract('total_interval_count')
        combine_calls = extract('combine_calls')
        result_sizes = extract('result_size')
        result_coverages = extract('result_coverage')

        aggregated = {
            # experiment metadata
            'uid' : self.__generate_name(experiment, True),
            'master_seed': self.master_seed,
            'data_type' : 'range' if experiment.data_type == DataType.RANGE else 'set',
            'num_trials': experiment.num_trials,
            'dataset_size' : experiment.dataset_size,
            'uncertain_ratio': experiment.uncertain_ratio,
            'interval_size_range':experiment.interval_size_range,
            'mult_size_range': experiment.mult_size_range,
            'num_intervals': experiment.num_intervals,
            'gap_size': experiment.gap_size,
            'interval_width': experiment.interval_width,
            'num_intervals_range': experiment.num_intervals_range,
            'gap_size_range': experiment.gap_size_range,
            'interval_width_range': experiment.interval_width_range,
            'reduce_triggerSz_sizeLim': experiment.reduce_triggerSz_sizeLim,
            'independent_variable': experiment.independent_variable,

            # MIN stats
            'min_time_mean': np.mean(min_times) if min_times else None,
            'min_time_std': np.std(min_times) if min_times else None,    
            # MAX stats
            'max_time_mean': np.mean(max_times) if max_times else None,
            'max_time_std': np.std(max_times) if max_times else None,
            # SUM stats 
            'sum_time_mean': np.mean(sum_times) if sum_times else None,
            'sum_time_std': np.std(sum_times) if sum_times else None,
            'sumtest_time_mean': np.mean(sumtest_times) if sumtest_times else None,
            'sumtest_time_std': np.std(sumtest_times) if sumtest_times else None,
            
            # reduction stats
            'reduce_calls_mean': np.mean(reduce_calls) if reduce_calls else None,
            'max_interval_count_mean': np.mean(max_intervals) if max_intervals else None,
            'total_interval_count_mean': np.mean(total_intervals) if total_intervals else None,
            'combine_calls_mean': np.mean(combine_calls) if combine_calls else None,
            'result_size_mean': np.mean(result_sizes) if result_sizes else None,
            'result_coverage_mean': np.mean(result_coverages) if result_coverages else None,
            # ^^ v3
        }
        
        return aggregated

    def __connect_db(self):
        return psycopg2.connect(**self.db_config)
    
    def __generate_name(self, experiment: ExperimentSettings, generalName: bool = False) -> str:
        '''
            generates postgres safe name (< 63 chars). old name was being cut.
            if generalName param is set, then trial number will be emit from result

                format:     t_{dtype}_{iv_abbrev}_{10 char dictHashOfExperiment}_t{trialNum}
        '''
        dtype = 'r' if experiment.data_type == DataType.RANGE else 's'
        iv_abbrev = experiment.iv_map.get(experiment.independent_variable if experiment.independent_variable else 'iv')
        param_str = json.dumps(experiment.to_dict(), sort_keys=True, default=str)
        
        hashed = hashlib.sha1(param_str.encode()).hexdigest()[:10]
    
        if generalName:
            return f"t_{dtype}_iv_{iv_abbrev}_{hashed}"
        
        return f"t_{dtype}_iv_{iv_abbrev}_{hashed}_t{experiment.curr_trial}"
        
    def __save_ddl_file(self, experiment: ExperimentSettings, data):
        ''' write data to DDL file for later loading 
            #NOTE broken. Need way to store final group and apppend all DDL to proper directory
        '''
        raise NotImplementedError("Broken. Will fix if ever actually used. NOTE- Need way to store final group and append all DDL to proper directory. Currently it stores in group dirctory, but not the specific Experiment within this group.")
        experiment_folder_path = f'data/results/{self.groupName}/ddl'
        timestamp = time.strftime("d%d_m%m_y%Y")
        out_file = f'{timestamp}_{experiment.name}_sd{self.master_seed}'
        ddl_path = f'{experiment_folder_path}/{out_file}.sql'    

        os.makedirs(experiment_folder_path, exist_ok=True)           

        table_name = experiment.experiment_id
            
        with open(ddl_path, 'w') as file:
            if experiment.data_type == DataType.RANGE:
                file.write(f"CREATE TABLE {table_name} (id INT GENERATED ALWAYS AS IDENTITY, val int4range, mult int4range);\n\n")
            elif experiment.data_type == DataType.SET:
                file.write(f"CREATE TABLE {table_name} (id INT GENERATED ALWAYS AS IDENTITY, val int4range[], mult int4range);\n\n")
            
            batch_size = 250
            for i in range(0, len(data), batch_size):
                batch = data[i: i + batch_size]
                file.write(f"INSERT INTO {table_name} (val, mult) VALUES \n")

                values = []
                for val, mult in batch:                    
                    values.append(f"    ({val}, {mult})")

                file.write(',\n'.join(values))
                file.write(';\n\n')

        print(f"  DDL saved: {ddl_path}")
    
def generate_seed(in_seed=None):
    '''genrate the master seed of this programs run. (can be included in runner or settings class)'''
    if in_seed is not None:
        seed = in_seed
    else:
        seed = int(time.time() * 1000) % (2**32)
        
    random.seed(seed)
    np.random.seed(seed)
    
    return seed

def format_datasize(size):
        if size >= 1_000_000: 
            return str.replace(numerize.numerize(size, 2), '.', '_')
        return numerize.numerize(size, 0)

def format_name(experiment: ExperimentSettings):
    """ generate unique name for an experiment based on its parameters.
        auto-includes dataset size, reduction config, and optional seed."""

    # base type: s = set, r = range
    dtype = 's' if getattr(experiment, 'data_type', None) == DataType.SET else 'r'
    
    # dataset size
    sz = f"n{getattr(experiment, 'dataset_size', 0)}"
    
    # reduction trigger/limit
    red = ""
    red_cfg = getattr(experiment, 'reduce_triggerSz_sizeLim', None)
    if red_cfg:
        red = f"red{red_cfg[0]}_{red_cfg[1]}"
    
    # shortened independent variable
    iv = getattr(experiment, 'independent_variable', 'iv')
    iv_val = getattr(experiment, iv, None)
    if iv and iv_val is not None:
        iv = f"iv_{experiment.iv_map[iv]}{iv_val}"
    
    seed = f"s{getattr(experiment, 'seed', '')}" if getattr(experiment, 'seed', None) else ""
    
    name = f"{dtype}_{sz}_{red}_{iv}{seed}"
    return name

def run_all():
    '''
        Main entrypoint to running experiments. 
        Parses args, starts runner engine, runs experiments, and processes results
    '''

    ### Parse args and config
    args = parse_args()
    master_seed = generate_seed(args.seed)
    print("Unique Master seed: ", master_seed)

    try:
        db_config = load_config(args.dbconfig)    
    except Exception as e:
        print(f"Error loading config: {e}")
        exit(1)

    ### Start engine
    runner = ExperimentRunner(db_config, master_seed)

    ### Clean before
    if args.clean_before:
        runner.clean_tables(args.clean_before)

    ### Load Experiments
    experiments = _load_experiments(args, runner, db_config)

    ### Run every experiment Suite and save results
    for suite in experiments.values():
        suite_results = []
        for group in suite.groups.values():
            results = _run_experiment_group(runner, suite.name, group)
            print(f'    Group results saved in: {runner.resultFilepath}')  
            suite_results.append(results)
        
        print(suite_results)

        # plot aggregate results for suite
        _plot_experiment_suite(runner, suite_results)

    ### Clean after
    if args.clean_after:
        runner.clean_tables(args.clean_after)

    print("\nUnique Master seed: ", master_seed)

def _load_experiments(args, runner, db_config):
    '''Load experiment configuration from various sources. (CLI, YAML, Python Script)'''

    if args.quick:
        return create_quick_experiment(args)
    elif args.yaml_experiments_file:
        return load_experiments_from_file(args.yaml_experiments_file)
    elif args.code:
        namespace = {'runner': runner, 'db_config': db_config}
        exec(open(args.code).read(), namespace)
        return namespace.get('experiments', {})
    else:
        sys.exit("No experiment source specified (use --help for examples)")

def _run_experiment_group(runner: ExperimentRunner, suite_name: str, group: ExperimentGroup):
    '''Run all experiments in a group and generate results.'''

    print(f"\nRunning experiment group: [{suite_name}]- {group.name}")
    
    # reset runner metadata to current experiment group
    runner.results = []
    
    runner.name = suite_name
    runner.groupName = group.name

    runner.set_file_path(suite_name, group.name, None)
        
    # for every experiment within group, run it
    for experiment in group.experiments.values():
        runner.run_experiment(experiment)

    os.makedirs(runner.resultFilepath, exist_ok=True)
    group_csv_path = f"{runner.resultFilepath}/results_sd{runner.master_seed}.csv"
    
    df = pd.DataFrame(runner.results)
    df.to_csv(group_csv_path, index=False)
    
    return group_csv_path

def _plot_experiment_suite(runner: ExperimentRunner, csv_paths: list) -> None:
    if csv_paths is None:
            raise ValueError('No list of csv results for suite')

    # create a suite-level folder (one level above groups)
    last_group_csv = Path(runner.resultFilepath)
    suite_folder = last_group_csv.parent  # parent of group folder → suite folder
    os.makedirs(suite_folder, exist_ok=True)
    runner.resultFilepath = suite_folder

    plotter = StatisticsPlotter(runner.resultFilepath, runner.master_seed)
    plotter.plot_experiment_suite(csv_paths)

if __name__ == '__main__':
    start = time.perf_counter()
    run_all()
    end = time.perf_counter()

    print(f"Tests took {end-start:.3f} s")

    # example cli runs (not recommended)
        # python3 main.py --quick -dt r -nt 5 -sz 2 -ur .0 -ca        
        # python3 main.py -xf tests_config.yaml -cb   