import ast
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
from typing import List

class StatisticsPlotter:
    """ handles all statistical analysis and visualization of experiment results"""

    REDUCE_PARAM_NAME = 'reduce_triggerSz_sizeLim'

    def __init__(self, resultFilepath: str, seed: str):
        self.resultFilepath = resultFilepath
        self.master_seed = seed
    

    def parse_reduce_tuple(self, val):
        """Parse reduce_triggerSz_sizeLim column."""
        try:
            return ast.literal_eval(str(val))
        except:
            return None

    def load_all_csvs(self, csv_paths: List[str]) -> pd.DataFrame:
        """Load and combine multiple experiment CSVs."""
        dfs = []
        for path in csv_paths:
            df = pd.read_csv(path, index_col=0)
            df['source_file'] = Path(path).parent.name  # track which experiment it came from
            dfs.append(df)
        
        combined = pd.concat(dfs, ignore_index=True)
                
        return combined

    def plot_experiment_group(self, group_csv_results: list, independent_variable: str) -> None:
        df = pd.read_csv(group_csv_results)
        
        self.plot_timeNaccuracy_vs_iv(df, independent_variable)
        self.run_reduce_plot_suite(df, independent_variable)
    

    def plot_experiment_suite(self, csv_results: list) -> None:
        ''' MAIN entrypoint '''
        
        df = self.load_all_csvs(csv_results)    
        
        iv = df['independent_variable'][0]
        path = self.plot_time_coverage_by_reduce(df, iv)
        
        print("Results saved in: ", path)
    
    def plot_reduction_heatmap(self, df: pd.DataFrame, indep_variable: str) -> str:
        """generate heatmap for reduction parameter tuning"""
        
        if indep_variable != self.REDUCE_PARAM_NAME:
            return
        
        # parse tuple column
        parsed = df[self.REDUCE_PARAM_NAME].apply(
            lambda x: eval(x) if isinstance(x, str) else x
        )
        df['trigger_sz'] = parsed.apply(lambda x: x[0])
        df['reduce_to_sz'] = parsed.apply(lambda x: x[1])
        
        # pivot table input for heatmap 
        sum_pivot = df.pivot_table(values='sum_time_mean', index='reduce_to_sz', columns='trigger_sz')   
        
        fig, (ax) = plt.subplots(1, 1, figsize=(12, 5))

        # SUM heatmap, focused on reduction params
        sns.heatmap(sum_pivot, annot=True, fmt='.1f', cmap='RdYlGn_r', ax=ax, cbar_kws={'label': 'Time (ms)'})
        ax.set_title('SUM Time Heatmap', fontsize=14, fontweight='bold')
        ax.set_xlabel('Trigger Size', fontsize=12)
        ax.set_ylabel('Reduce To Size', fontsize=12)
        
        plt.tight_layout()
        outfile = f'heatmap{self.master_seed}'
        outpath = f"{self.resultFilepath}/{outfile}"
        plt.savefig(outpath, dpi=300, bbox_inches='tight')
        return outpath
    
    # def plot_pareto_front(self, df: pd.DataFrame)


    def plot_time_coverage_by_reduce(self, df: pd.DataFrame, indep_variable: str) -> str:
        
        ''' plot time vs IV and coverage vs IV for'''
        # Sort dataframe by dataset size
        df_sorted = df.sort_values(indep_variable)

        # Group by reduction parameters
        dfg = df_sorted.groupby(self.REDUCE_PARAM_NAME)

        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 10), sharex=True)

        for p, group in dfg:
            # Make sure each group is sorted by dataset_size
            group = group.sort_values(indep_variable)
            x = group[indep_variable]
            y_time = group['sum_time_mean']
            y_time_err = group['sum_time_std']
            y_coverage = group['result_coverage_mean']

            # Plot time with error bars
            ax1.errorbar(x, y_time, yerr=y_time_err, fmt='o-', capsize=5, label=str(p))
            # Plot coverage
            ax2.errorbar(x, y_coverage, fmt='o-', capsize=5, label=str(p))

        # TIME axis labels
        ax1.set_ylabel('Time (ms)')
        ax1.set_title(f'Time vs {indep_variable} by Reduction Parameters')
        ax1.grid(True, alpha=0.3)
        ax1.legend(title='Reduce Params')

        # COVERAGE axis labels
        ax2.set_ylabel('Coverage')
        ax2.set_xlabel(indep_variable)
        ax2.set_title(f'Coverage vs {indep_variable} by Reduction Parameters')
        ax2.grid(True, alpha=0.3)
        ax2.legend(title='Reduce Params')

        plt.tight_layout()
        outfile = f'time_accuracy_sd{self.master_seed}'
        outpath = f"{self.resultFilepath}/{outfile}"
        plt.savefig(outpath, dpi=300, bbox_inches='tight')
        return outpath