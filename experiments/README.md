# Path-Based Formulation Experiments

This directory contains the experimental framework for running comprehensive tests of the Path-Based Formulation algorithm on all available instances.

## Quick Start

### Option 1: Using the convenience script (Recommended)
```bash
# Run with default parameters (5 minutes, heuristics=0.5)
./run_experiment.sh

# Run with custom time limit (10 minutes)
./run_experiment.sh 600

# Run with custom time limit and heuristics level
./run_experiment.sh 600 1.0
```

### Option 2: Using Python directly
```bash
# Run with default configuration
python3 experiments/scripts/run_path_based_experiment.py

# Run with custom configuration file
python3 experiments/scripts/run_path_based_experiment.py experiments/config/custom_config.yaml
```

## Configuration

The experiment configuration is stored in `experiments/config/path_based_experiment.yaml`. You can modify:

- **Time limit**: How long to run each instance
- **Heuristics level**: Gurobi heuristics parameter (0.0-2.0)
- **Instance groups**: Which instances to test
- **Output format**: CSV columns and filename template

## Output

The experiment generates:

1. **CSV Results**: `experiments/results/experimento_path_based_formulation_TIMESTAMP.csv`
   - Contains detailed results for each instance
   - Includes: instance name, optimal value, best found, runtime, nodes explored, etc.

2. **Log File**: `experiments/results/experiment_TIMESTAMP.log`
   - Detailed execution log
   - Error messages and warnings

3. **Console Summary**: Real-time progress and final statistics

## CSV Columns

The output CSV includes these columns:

- `instance_name`: Name of the instance file
- `instance_group`: Group (ocstpin, orst, orstBig)
- `num_nodes`, `num_edges`, `num_requirements`: Instance characteristics
- `probability`: Instance probability parameter
- `optimal_known`: Known optimal solution (if available)
- `best_found`: Best solution found by algorithm
- `is_optimal`: Whether the solution is optimal
- `runtime_seconds`: Execution time
- `nodes_explored`: Branch-and-bound nodes explored
- `mip_gap_percent`: MIP gap percentage
- `gurobi_status`: Gurobi solver status
- `time_limit`, `heuristics_level`: Experiment parameters
- `algorithm_version`: Algorithm version

## Instance Groups

The experiment tests three groups of instances:

1. **ocstpin**: Small-medium OCST instances (ocstpin0-ocstpin14)
2. **orst**: Medium ORST instances (orst0-orst9)
3. **orstBig**: Large ORST instances (orstBig0-orstBig2)

## Requirements

- Python 3.7+
- PyYAML (`pip install PyYAML`)
- Compiled path-based formulation executable
- Gurobi license

## Example Usage

```bash
# Quick test with 1-minute limit
./run_experiment.sh 60

# Comprehensive test with 10-minute limit and aggressive heuristics
./run_experiment.sh 600 2.0

# Check results
ls experiments/results/
cat experiments/results/experimento_path_based_formulation_*.csv
```

## Troubleshooting

### Executable not found
```bash
# Compile the algorithm first
g++ -std=c++17 -Wall -Wextra -O2 -I$GUROBI_HOME/include -L$GUROBI_HOME/lib \
    src/cpp/algorithms/path_based_formulation/algorithm.cpp \
    -o build/executables/path_based_formulation -lgurobi_c++ -lgurobi120
```

### PyYAML not found
```bash
pip install PyYAML
```

### Permission denied
```bash
chmod +x run_experiment.sh
chmod +x experiments/scripts/run_path_based_experiment.py
```
