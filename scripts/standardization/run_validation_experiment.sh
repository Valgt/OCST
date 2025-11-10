#!/bin/bash
# Validation Experiment - Path Based Formulation with Unit Test Changes
# Executes all quick_check instances and compares with historical results

set -e

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUTPUT_DIR="experiments/validation/validation_${TIMESTAMP}"
SOLVER="./build/executables/path_based_formulation"

echo "========================================"
echo "OCST Validation Experiment"
echo "========================================"
echo "Timestamp: ${TIMESTAMP}"
echo "Output: ${OUTPUT_DIR}"
echo "Solver: ${SOLVER}"
echo ""

# Create output directory
mkdir -p "${OUTPUT_DIR}"

# CSV header
echo "instance,status,objective,edges,runtime_seconds,nodes_explored,lazy_constraints" > "${OUTPUT_DIR}/results.csv"

# Get all quick_check instances (excluding orstBig)
INSTANCES=$(find data/input -name "*.json" | grep -E "(ocstpin|orst[0-9]\.json)" | sort)
INSTANCE_COUNT=$(echo "$INSTANCES" | wc -l)

echo "Found ${INSTANCE_COUNT} instances to test"
echo ""

PASSED=0
FAILED=0

for instance in $INSTANCES; do
    instance_name=$(basename "$instance" .json)
    echo "Testing: ${instance_name}"
    
    sol_file="${OUTPUT_DIR}/${instance_name}.sol"
    
    # Run solver and capture output
    if timeout 600 ${SOLVER} "$instance" "$sol_file" > "${OUTPUT_DIR}/${instance_name}.log" 2>&1; then
        # Extract results from log
        status=$(grep "^Status:" "${OUTPUT_DIR}/${instance_name}.log" | awk '{print $2}')
        objective=$(grep "^Objective value:" "${OUTPUT_DIR}/${instance_name}.log" | awk '{print $3}')
        edges=$(grep "^Selected edges:" "${OUTPUT_DIR}/${instance_name}.log" | awk '{print $3}')
        runtime=$(grep "^Runtime:" "${OUTPUT_DIR}/${instance_name}.log" | awk '{print $2}')
        nodes=$(grep "^Nodes explored:" "${OUTPUT_DIR}/${instance_name}.log" | awk '{print $3}')
        lazy=$(grep "^Lazy constraints added:" "${OUTPUT_DIR}/${instance_name}.log" | awk '{print $4}')
        
        echo "${instance_name},${status},${objective},${edges},${runtime},${nodes},${lazy}" >> "${OUTPUT_DIR}/results.csv"
        
        echo "  ✓ ${status} - Objective: ${objective}, Edges: ${edges}, Time: ${runtime}s"
        PASSED=$((PASSED + 1))
    else
        echo "  ✗ FAILED or TIMEOUT"
        echo "${instance_name},FAILED,NA,NA,NA,NA,NA" >> "${OUTPUT_DIR}/results.csv"
        FAILED=$((FAILED + 1))
    fi
    echo ""
done

echo "========================================"
echo "Experiment Complete"
echo "========================================"
echo "Total instances: ${INSTANCE_COUNT}"
echo "Passed: ${PASSED}"
echo "Failed: ${FAILED}"
echo ""
echo "Results saved to: ${OUTPUT_DIR}/results.csv"
echo ""

# Compare with historical results if available
HISTORICAL="experiments/validation/dual_run_20251103_225726/comparison_report.csv"
if [ -f "$HISTORICAL" ]; then
    echo "Comparing with historical results..."
    python3 - <<EOF
import csv
import sys

# Load historical results
historical = {}
with open('${HISTORICAL}', 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        instance = row['instance']
        historical[instance] = {
            'objective': float(row['migrating_objective']),
            'edges': int(row['migrating_edges'])
        }

# Load current results
current = {}
with open('${OUTPUT_DIR}/results.csv', 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        if row['status'] != 'FAILED':
            current[row['instance']] = {
                'objective': float(row['objective']),
                'edges': int(row['edges'])
            }

# Compare
matches = 0
mismatches = 0
new_instances = 0

print("\nComparison Report:")
print("=" * 80)
print(f"{'Instance':<15} {'Historical':<15} {'Current':<15} {'Match':<10}")
print("=" * 80)

for instance in sorted(current.keys()):
    if instance in historical:
        hist_obj = historical[instance]['objective']
        curr_obj = current[instance]['objective']
        
        match = abs(hist_obj - curr_obj) < 1e-6
        match_str = "✓ PASS" if match else "✗ FAIL"
        
        if match:
            matches += 1
        else:
            mismatches += 1
        
        print(f"{instance:<15} {hist_obj:<15.0f} {curr_obj:<15.0f} {match_str:<10}")
    else:
        new_instances += 1
        print(f"{instance:<15} {'N/A':<15} {current[instance]['objective']:<15.0f} {'NEW':<10}")

print("=" * 80)
print(f"\nSummary:")
print(f"  Matches: {matches}")
print(f"  Mismatches: {mismatches}")
print(f"  New instances: {new_instances}")

if mismatches > 0:
    print("\n⚠️  WARNING: Some results don't match historical data!")
    sys.exit(1)
else:
    print("\n✓ All results match historical data (100% parity)")
    sys.exit(0)
EOF
    
    COMPARISON_EXIT=$?
    if [ $COMPARISON_EXIT -eq 0 ]; then
        echo ""
        echo "✓✓✓ VALIDATION SUCCESSFUL ✓✓✓"
    else
        echo ""
        echo "⚠️⚠️⚠️ VALIDATION FAILED ⚠️⚠️⚠️"
        exit 1
    fi
else
    echo "Historical results not found at: ${HISTORICAL}"
    echo "Skipping comparison."
fi

echo ""
echo "Experiment log saved to: ${OUTPUT_DIR}/"

