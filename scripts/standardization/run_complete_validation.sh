#!/bin/bash

# Script to run complete validation experiment (all instances except big)
# Usage: ./run_complete_validation.sh

set -e

PROJECT_ROOT="/home/sergio/OCST"
SOLVER_BIN="$PROJECT_ROOT/build/executables/path_based_formulation"
INSTANCES_DIR="$PROJECT_ROOT/data/input"
OUTPUT_DIR="$PROJECT_ROOT/experiments/workstream_2/complete_cleanup_validation"

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Time limit per instance (in seconds)
TIME_LIMIT=300  # 5 minutes per instance

echo "========================================="
echo "Running Complete Validation Experiment"
echo "========================================="
echo "Solver: $SOLVER_BIN"
echo "Instances: $INSTANCES_DIR"
echo "Output: $OUTPUT_DIR"
echo "Time limit: ${TIME_LIMIT}s per instance"
echo "========================================="
echo ""

# Counter
total=0
success=0
failed=0

# Process all JSON instances except Big ones
for instance_file in "$INSTANCES_DIR"/*.json; do
    instance_name=$(basename "$instance_file" .json)
    
    # Skip Big instances
    if [[ "$instance_name" =~ Big ]]; then
        echo "⊗ Skipping $instance_name (Big instance)"
        continue
    fi
    
    total=$((total + 1))
    
    echo "[$total] Processing $instance_name..."
    
    # Output files
    sol_file="$OUTPUT_DIR/${instance_name}.sol"
    
    # Run solver
    if "$SOLVER_BIN" "$instance_file" "$sol_file" "$TIME_LIMIT" 0.5 > /dev/null 2>&1; then
        success=$((success + 1))
        echo "    ✓ Solved successfully"
    else
        failed=$((failed + 1))
        echo "    ✗ FAILED"
    fi
done

echo ""
echo "========================================="
echo "Summary:"
echo "  Total instances: $total"
echo "  Successful: $success"
echo "  Failed: $failed"
echo "========================================="

if [ $failed -eq 0 ]; then
    echo "✓✓✓ ALL INSTANCES SOLVED SUCCESSFULLY ✓✓✓"
    exit 0
else
    echo "⚠️ Some instances failed"
    exit 1
fi

