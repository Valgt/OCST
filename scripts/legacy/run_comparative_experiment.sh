#!/bin/bash
# Convenience script to run Comparative Formulations experiments
# Usage: ./run_comparative_experiment.sh [time_limit] [heuristics_level]

set -e  # Exit on any error

# Default parameters
TIME_LIMIT=${1:-60}  # Default: 1 minute
HEURISTICS=${2:-0.5}   # Default: moderate heuristics

echo "🚀 Comparative Formulations Experiment"
echo "====================================="
echo "Time Limit: ${TIME_LIMIT} seconds"
echo "Heuristics Level: ${HEURISTICS}"
echo ""

# Check if executables exist
PATH_EXECUTABLE="build/executables/path_based_formulation"
FLOW_EXECUTABLE="build/executables/flow_based_formulation"

if [ ! -f "$PATH_EXECUTABLE" ]; then
    echo "❌ Error: Path-Based executable not found at $PATH_EXECUTABLE"
    echo "Please compile the algorithms first:"
    echo "  make all"
    exit 1
fi

if [ ! -f "$FLOW_EXECUTABLE" ]; then
    echo "❌ Error: Flow-Based executable not found at $FLOW_EXECUTABLE"
    echo "Please compile the algorithms first:"
    echo "  make all"
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
with open('experiments/config/comparative_experiment.yaml', 'r') as f:
    config = yaml.safe_load(f)

# Update parameters
config['execution']['time_limit'] = $TIME_LIMIT
config['execution']['heuristics_level'] = $HEURISTICS

# Update time limits by group
config['execution']['time_limits_by_group']['ocstpin'] = 30
config['execution']['time_limits_by_group']['orst'] = $TIME_LIMIT

# Save updated config
with open('experiments/config/comparative_experiment.yaml', 'w') as f:
    yaml.dump(config, f, default_flow_style=False)

print('✓ Configuration updated')
"

# Run the experiment
echo "🏃 Running comparative experiment..."
python3 experiments/scripts/run_comparative_experiment.py

echo ""
echo "✅ Comparative experiment completed!"
echo "📊 Check experiments/results/ for the CSV output"
echo "📈 Use the CSV to compare algorithm performance"
