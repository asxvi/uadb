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
        self.iv = None
        self.n_range_str = None

    # ----------------------------------  
    # --- MAIN entrypoint ---
    # ----------------------------------
    def plot_experiment_suite(self, csv_results: list) -> None:
        ''' MAIN entrypoint '''

        df = self.load_all_csvs(csv_results)    
        self.set_n_range_str(df)
        iv = df['independent_variable'][0]
        self.iv = iv

        # plot stuff
        self.plot_time_coverage_by_reduce(df, iv)
        self.plot_reduction_heatmap(df, iv)
        self.plot_3_row_red_vs_TimeNCover(df, iv)
        self.plot_convergence_vs_n(df)
        
        print("Results saved in: ", self.resultFilepath)

    # Not work
    def plot_experiment_group(self, group_csv_results: list, independent_variable: str) -> None:
        df = pd.read_csv(group_csv_results)
        
        self.plot_timeNaccuracy_vs_iv(df, independent_variable)
        self.run_reduce_plot_suite(df, independent_variable)
    

    # ----------------------------------  
    # --- Plotting Code ---
    # ----------------------------------
    
    # def plot_pareto_front(self, df: pd.DataFrame)
    
    def plot_reduction_heatmap(self, df: pd.DataFrame) -> str:
        """generate heatmap for reduction parameter tuning"""
        
        # if self.iv != self.REDUCE_PARAM_NAME:
        #     return
        
        # parse tuple column
        parsed = df[self.REDUCE_PARAM_NAME].apply(
            lambda x: eval(x) if isinstance(x, str) else x
        )
        df['trigger_sz'] = parsed.apply(lambda x: x[0])
        df['reduce_to_sz'] = parsed.apply(lambda x: x[1])
        
        # pivot table input for heatmap 
        sum_pivot = df.pivot_table(values='sum_time_mean', index='reduce_to_sz', columns='trigger_sz')   
        
        fig, (ax) = plt.subplots(1, 1, figsize=(12, 5))
        param_str = (
            f' | iv={sorted(df["independent_variable"].unique())} | '
            f'n={self.n_range_str} | '
            f'gaps={sorted(df["gap_size_range"].unique())} | '
            f'widths={sorted(df["interval_width_range"].unique())} | '
            f'uncert={sorted(df["uncertain_ratio"].unique())} | '
            f'dataPath={self.resultFilepath} | '
            f'seed={self.master_seed} | '
        )
        fig.text(0.5, -0.02, param_str, ha='center', fontsize=7, color='black')

        # SUM heatmap, focused on reduction params
        sns.heatmap(sum_pivot, annot=True, fmt='.1f', cmap='RdYlGn_r', ax=ax, cbar_kws={'label': 'Time (ms)'})
        ax.set_title('SUM Time Heatmap', fontsize=14, fontweight='bold')
        ax.set_xlabel('Trigger Size', fontsize=12)
        ax.set_ylabel('Reduce To Size', fontsize=12)
        
        plt.tight_layout()
        outfile = f'heatmap{self.master_seed}'
        outpath = f"{self.resultFilepath}/{outfile}"
        plt.savefig(outpath, dpi=300, bbox_inches='tight')
        plt.close()
        return outpath
    
    def plot_time_coverage_by_reduce(self, df: pd.DataFrame, indep_variable: str) -> str:
        ''' plot time vs IV and coverage vs IV for'''
        # Sort dataframe by dataset size
        df_sorted = df.sort_values(indep_variable)

        # Group by reduction parameters
        dfg = df_sorted.groupby(self.REDUCE_PARAM_NAME)

        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 10), sharex=True)       
        param_str = (
            f' | iv={sorted(df["independent_variable"].unique())} | '
            f'n={self.n_range_str} | '
            f'gaps={sorted(df["gap_size_range"].unique())} | '
            f'widths={sorted(df["interval_width_range"].unique())} | '
            f'uncert={sorted(df["uncertain_ratio"].unique())} | '
            f'dataPath={self.resultFilepath} | '
            f'seed={self.master_seed} | '
        )
        fig.text(0.5, -0.02, param_str, ha='center', fontsize=7, color='black')
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
        ax1.legend(title='Reduce Params', loc='upper left')
        # ax1.xaxis.set_ticks(x_labels)
        # ax1.set_xticklabels(x_labels, rotation=45, ha='right')
        ax1.tick_params(axis='x', rotation=45)

        # COVERAGE axis labels
        ax2.set_ylabel('Coverage')
        ax2.set_xlabel(indep_variable)
        ax2.set_title(f'Coverage vs {indep_variable} by Reduction Parameters')
        ax2.grid(True, alpha=0.3)
        ax2.legend(title='Reduce Params', loc='upper left')
        ax2.tick_params(axis='x', rotation=45)

        plt.tight_layout()
        outfile = f'time_accuracy_sd{self.master_seed}'
        outpath = f"{self.resultFilepath}/{outfile}"
        plt.savefig(outpath, dpi=300, bbox_inches='tight')
        plt.close()
        return outpath

    def plot_3_row_red_vs_TimeNCover(self, df, iv) -> str:
        ''' calculates time/cover/distance vs redution params for each ni'''
        
        if iv != 'num_intervals':
            return

        reduce_params = sorted(df['reduce_triggerSz_sizeLim_tuple'].unique())
        x_labels = [str(r) for r in reduce_params]
        n = len(df['num_intervals'].unique())

        # 0: Time vs red params for each ni
        # 1: Cover vs red params for each ni
        # 2: Distance = sqrt(time^2 + cover^2)
        # fig, axes=  plt.subplots(3, n, figsize=(6*n,12))
        fig, axes=  plt.subplots(2, n, figsize=(6*n,12))
        param_str = (
            f' | iv={sorted(df["independent_variable"].unique())} | '
            f'n={self.n_range_str} | '
            f'gaps={sorted(df["gap_size_range"].unique())} | '
            f'widths={sorted(df["interval_width_range"].unique())} | '
            f'uncert={sorted(df["uncertain_ratio"].unique())} | '
            f'dataPath={self.resultFilepath} | '
            f'seed={self.master_seed} | '
        )
        fig.text(0.5, -0.02, param_str, ha='center', fontsize=7, color='black')

        for i, ni in enumerate(sorted(df['num_intervals'].unique())):
            # plot for each ni
            df_ni = df[df['num_intervals'] == ni].copy()

            # PLot 1: time
            ax = axes[0, i]
            y = df_ni['sum_time_mean']
            ax.plot(x_labels, y, marker='o')
            ax.set_title(f'ni={ni}')
            ax.set_xlabel('red')
            ax.tick_params(axis='x', rotation=45)
            ax.grid(True)

            # Plot 2: coverage
            ax = axes[1, i]
            y = df_ni['result_coverage_mean']
            ax.plot(x_labels, y, marker='o')
            ax.set_title(f'ni={ni}')
            ax.set_xlabel('red')
            ax.tick_params(axis='x', rotation=45)
            ax.grid(True)

            # # Plot 3: distance of both time and coverage
            # # normalize time and coverage to equal weight (x-min)/(max-min)
            # df_ni['time_norm'] = self.safe_normalize(df_ni['sum_time_mean'])
            # df_ni['cov_norm'] = self.safe_normalize(df_ni['result_coverage_mean'])
            # scores = (df_ni['time_norm']**2 + df_ni['cov_norm']**2)**0.5
            
            # ax = axes[2, i]
            # ax.plot(x_labels, scores, marker='o')
            # ax.set_title(f'ni={ni}')
            # ax.set_xlabel('red')
            # ax.tick_params(axis='x', rotation=45)
            # ax.grid(True)
            
        axes[0][0].set_ylabel('Time (ms)')
        axes[1][0].set_ylabel('Coverage (smaller=better)')
        # axes[2][0].set_ylabel('Distance (smaller=bettwe)')
        
        plt.tight_layout()
        outfile = f'distance_vs_NI_and_red{self.master_seed}'
        outpath = f"{self.resultFilepath}/{outfile}"
        plt.savefig(outpath, dpi=300, bbox_inches='tight')
        plt.close()
        return outpath
    
    def plot_convergence_vs_n(self, df: pd.DataFrame) -> str:
        triggers = df['resizeTrigger'].unique()
        plt.figure(figsize=(10, 6)) 
        for trigger in triggers:
            subdf = df[df['resizeTrigger'] == trigger]
            plt.plot(subdf['dataset_size'], subdf['minEffectiveIntervalCountMean'], marker='o', label=f'trigger={trigger}')
        
        param_str = (
            f' | iv={sorted(df["independent_variable"].unique())} | '
            f"n={self.n_range_str} | "
            f'gaps={sorted(df["gap_size_range"].unique())} | '
            f'widths={sorted(df["interval_width_range"].unique())} | '
            f'uncert={sorted(df["uncertain_ratio"].unique())} | '
            f'dataPath={self.resultFilepath} | '
            f'seed={self.master_seed} | '
        )
        plt.text(0.5, -1.50, param_str, ha='center', fontsize=7, color='black')
        plt.xlabel('Dataset Size')
        plt.ylabel('Interval Count')
        plt.title('Convergence for different triggers')
        plt.legend()
        plt.grid(True)
        # plt.show()
        outfile = f'convergence_vs_n_{self.master_seed}'
        outpath = f"{self.resultFilepath}/{outfile}"
        plt.savefig(outpath, dpi=300, bbox_inches='tight')
        plt.close()
        return outpath



    # ----------------------------------  
    # --- helpers ---
    # ----------------------------------
    def get_dataset_size_bounds(self, df: pd.DataFrame) -> str:
        sizes = sorted(df['dataset_size'].unique())
        min_n = sizes[0]
        max_n = sizes[-1]
        if len(sizes) > 1:
            step_n = sizes[1] - sizes[0]
        else:
            step_n = 0
        
        return f"{min_n}..{max_n} step {step_n}"

    def set_n_range_str (self, df):
            self.n_range_str = self.get_dataset_size_bounds(df)

    def load_all_csvs(self, csv_paths: List[str]) -> pd.DataFrame:
        """Load and combine multiple experiment CSVs. Do simple data processing for later convenience"""
        dfs = []
        for path in csv_paths:
            df = pd.read_csv(path, index_col=0)
            df['source_file'] = Path(path).parent.name  # track which experiment it came from
            dfs.append(df)
        
        combined = pd.concat(dfs, ignore_index=True)
        
        # add in diff representations of red params for convenience
        combined[['resizeTrigger', 'sizeLimit']] = combined['reduce_triggerSz_sizeLim'].str.strip('()').str.split(',', expand=True).astype(int)
        combined['gap_size_range_tuple'] = combined['gap_size_range'].apply(ast.literal_eval)
        combined['reduce_triggerSz_sizeLim_tuple'] = combined['reduce_triggerSz_sizeLim'].apply(ast.literal_eval)
        return combined