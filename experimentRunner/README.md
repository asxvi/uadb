# Overview
**Entrypoint:** `main.py`

**OuputPath** `/data/results/<SEED>/<SuiteName>/`

### User workflow
1. Create an experiment config file  
   - `.py` (recommended, most flexible)  
   - `.yaml` (simpler for static configs)
2. Run the script using CLI flags

### internal workflow:
generate data -> insert into DB -> run queries -> collect data -> write to csv-> (eventually make optional) create plots (StatisticsPlotter.py)


### 3 main classes:
- ExperimentSuite - container containing many releted groups of experiments
- ExperimentGroup - contains experiments 
- ExperimentSettings - individual representation of experiment. Where all the IV's are.

Assuming the CLI -py mode is used and an experiment procedure/config format similar to any of the \*.py examples in experimentRunner/experiments/active, the main data structure is a nested dict: 'ExperimentSuiteName': ExperiementSuite.

# Quickstart
### Python file (recommended)
python main.py --py experiments/active/gap_sweeping.py 

### YAML file
python main.py --yaml experiments/active/yourExperiment.yaml

### Clean leftover tables before run (recommended)
python main.py --code ... --cb "t_%"


# Improvements
- Statistics side of things. Struggle on multidimensional analysis
