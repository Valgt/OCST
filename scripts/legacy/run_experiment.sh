#!/bin/bash
# Convenience script to run Path-Based Formulation experiments
# Usage: ./run_experiment.sh [time_limit] [heuristics_level]

set -e  # Exit on any error

# Default parameters
TIME_LIMIT=${1:-300}  # Default: 5 minutes
HEURISTICS=${2:-0.5}   # Default: moderate heuristics

echo "🚀 Path-Based Formulation Experiment"
echo "====================================="
echo "Time Limit: ${TIME_LIMIT} seconds"
echo "Heuristics Level: ${HEURISTICS}"
echo ""

# Check if executable exists
EXECUTABLE="build/executables/path_based_formulation"
if [ ! -f "$EXECUTABLE" ]; then
    echo "❌ Error: Executable not found at $EXECUTABLE"
    echo "Please compile the algorithm first:"
    echo "  g++ -std=c++17 -Wall -Wextra -O2 -I\$GUROBI_HOME/include -L\$GUROBI_HOME/lib src/cpp/algorithms/path_based_formulation/algorithm.cpp -o $EXECUTABLE -lgurobi_c++ -lgurobi120"
    exit 1
fi

# Check if Python is available
if ! command -v python3 &> /dev/null; then
    echo "❌ Error: Python3 not found"
    echo "Please install Python3 to run experiments"
    exit 1
fi

# Check if PyYAML is available
if ! python3 -c "import yaml" &> /dev/null; then
    echo "❌ Error: PyYAML not found"
    echo "Please install PyYAML: pip install PyYAML"
    exit 1
fi

# Update configuration with command line parameters
echo "📝 Updating configuration..."
python3 -c "
import yaml
import sys

# Load config
with open('experiments/config/path_based_experiment.yaml', 'r') as f:
    config = yaml.safe_load(f)

# Update parameters
config['execution']['time_limit'] = $TIME_LIMIT
config['execution']['heuristics_level'] = $HEURISTICS

# Save updated config
with open('experiments/config/path_based_experiment.yaml', 'w') as f:
    yaml.dump(config, f, default_flow_style=False)

print('✓ Configuration updated')
"

# Run the experiment
echo "🏃 Running experiment..."
python3 experiments/scripts/run_path_based_experiment.py

echo ""
echo "✅ Experiment completed!"
echo "📊 Check experiments/results/ for the CSV output"
