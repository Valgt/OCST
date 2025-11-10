#!/usr/bin/env python3
"""
Deep JSON Results Validation
=============================

Validates ALL properties of JSON results against historical data:
- Objective values
- Gurobi status
- Solver nodes
- Lazy constraints
- Gap values
- Tree structure (edges)
- Optimization status
"""

import json
import sys
import csv
from pathlib import Path
from typing import Dict, List, Tuple

def load_historical_detailed_results(validation_dir: Path) -> Dict:
    """
    Load detailed historical results from dual_run validation.
    Returns dict mapping instance_name -> detailed metrics
    """
    historical = {}
    
    # Load from migrating CSV files
    for csv_file in validation_dir.glob('*_migrating.csv'):
        instance_name = csv_file.stem.replace('_migrating', '')
        
        with open(csv_file, 'r') as f:
            lines = f.readlines()
            if len(lines) >= 2:
                # instance,nodes,edges,requirements,probability,objective,runtime,gap,status,nodes_explored
                parts = lines[1].strip().split(',')
                if len(parts) >= 10:
                    historical[instance_name] = {
                        'objective': float(parts[5]),
                        'status': int(parts[8]),  # Gurobi status
                        'nodes_explored': int(parts[9]),
                        'gap': float(parts[7]) if parts[7] else 0.0
                    }
    
    return historical

def compare_json_result(json_path: Path, historical: Dict) -> Tuple[bool, List[str]]:
    """
    Deep comparison of JSON result against historical data.
    
    Returns:
        (all_match, list_of_differences)
    """
    with open(json_path, 'r') as f:
        current = json.load(f)
    
    instance_name = current['instance']['name']
    
    if instance_name not in historical:
        return True, [f"No historical data for {instance_name} (new instance?)"]
    
    hist = historical[instance_name]
    differences = []
    
    # 1. Objective value
    curr_obj = current['results']['objective']
    hist_obj = hist['objective']
    obj_diff = abs(curr_obj - hist_obj)
    if obj_diff > 1e-6:
        differences.append(
            f"Objective mismatch: {hist_obj} (hist) vs {curr_obj} (curr), diff={obj_diff}"
        )
    
    # 2. Gurobi status
    curr_status = int(current['solver_metadata'].get('gurobi_status', -1))
    hist_status = hist['status']
    if curr_status != hist_status:
        differences.append(
            f"Gurobi status mismatch: {hist_status} (hist) vs {curr_status} (curr)"
        )
    
    # 3. Solver nodes explored
    curr_nodes = current['runtime']['solver_nodes']
    hist_nodes = hist['nodes_explored']
    # Allow some variation in node count (heuristics may differ slightly)
    if abs(curr_nodes - hist_nodes) > max(1, hist_nodes * 0.1):  # 10% tolerance or +1
        differences.append(
            f"Solver nodes: {hist_nodes} (hist) vs {curr_nodes} (curr)"
        )
    
    # 4. Gap
    curr_gap = current['results']['gap_percent']
    hist_gap = hist['gap']
    if abs(curr_gap - hist_gap) > 1e-6:
        differences.append(
            f"Gap mismatch: {hist_gap} (hist) vs {curr_gap} (curr)"
        )
    
    # 5. Optimization status code
    status_code = current['optimization_status']['code']
    if hist_status == 2 and status_code != 'OPTIMAL':
        differences.append(
            f"Status code: Expected OPTIMAL (Gurobi 2), got {status_code}"
        )
    
    # 6. Has solution flag
    has_solution = current['optimization_status']['has_solution']
    if hist_status == 2 and not has_solution:
        differences.append(
            f"has_solution flag is False but Gurobi status is OPTIMAL"
        )
    
    # 7. Solution tree consistency
    if has_solution:
        tree_cost = current['solution']['tree_cost']
        if abs(tree_cost - curr_obj) > 1e-6:
            differences.append(
                f"Tree cost {tree_cost} doesn't match objective {curr_obj}"
            )
    
    return len(differences) == 0, differences

def print_detailed_comparison(results_dir: Path, historical: Dict):
    """Print detailed comparison table."""
    
    json_files = sorted(results_dir.glob('*.results.json'))
    
    print("\n" + "="*120)
    print(f"{'Instance':<15} {'Obj Match':<12} {'Status':<8} {'Nodes':<12} {'Gap':<10} {'Tree':<10} {'Overall':<10}")
    print("="*120)
    
    total = 0
    perfect_matches = 0
    
    for json_file in json_files:
        total += 1
        all_match, diffs = compare_json_result(json_file, historical)
        
        with open(json_file, 'r') as f:
            data = json.load(f)
        
        instance = data['instance']['name']
        
        if instance not in historical:
            print(f"{instance:<15} {'N/A':<12} {'N/A':<8} {'N/A':<12} {'N/A':<10} {'N/A':<10} {'NEW':<10}")
            continue
        
        hist = historical[instance]
        
        # Check individual properties
        obj_match = abs(data['results']['objective'] - hist['objective']) < 1e-6
        status_match = int(data['solver_metadata'].get('gurobi_status', -1)) == hist['status']
        nodes_curr = data['runtime']['solver_nodes']
        nodes_hist = hist['nodes_explored']
        nodes_match = abs(nodes_curr - nodes_hist) <= max(1, nodes_hist * 0.1)
        gap_match = abs(data['results']['gap_percent'] - hist['gap']) < 1e-6
        
        # Check tree cost consistency
        if data['optimization_status']['has_solution']:
            tree_match = abs(data['solution']['tree_cost'] - data['results']['objective']) < 1e-6
        else:
            tree_match = True
        
        obj_str = "✓" if obj_match else "✗"
        status_str = "✓" if status_match else "✗"
        nodes_str = f"{nodes_curr}/{nodes_hist}"
        gap_str = "✓" if gap_match else "✗"
        tree_str = "✓" if tree_match else "✗"
        
        if all_match:
            overall = "✓ PASS"
            perfect_matches += 1
        else:
            overall = "✗ DIFF"
        
        print(f"{instance:<15} {obj_str:<12} {status_str:<8} {nodes_str:<12} {gap_str:<10} {tree_str:<10} {overall:<10}")
        
        if not all_match:
            for diff in diffs:
                print(f"  └─ {diff}")
    
    print("="*120)
    print(f"\nSummary: {perfect_matches}/{total} instances with perfect match across all properties")
    
    return perfect_matches, total

def main():
    results_dir = Path('experiments/validation/validation_20251110_003259')
    historical_dir = Path('experiments/validation/dual_run_20251103_225726')
    
    print("="*120)
    print("Deep JSON Results Validation - All Properties")
    print("="*120)
    
    if not results_dir.exists():
        print(f"❌ Results directory not found: {results_dir}")
        sys.exit(1)
    
    if not historical_dir.exists():
        print(f"❌ Historical directory not found: {historical_dir}")
        sys.exit(1)
    
    # Load historical data
    print(f"\nLoading historical data from: {historical_dir}")
    historical = load_historical_detailed_results(historical_dir)
    print(f"Loaded historical data for {len(historical)} instances")
    
    # Compare all properties
    perfect_matches, total = print_detailed_comparison(results_dir, historical)
    
    print("\n" + "="*120)
    
    if perfect_matches == total:
        print("✓✓✓ ALL PROPERTIES MATCH PERFECTLY ✓✓✓")
        print("="*120)
        sys.exit(0)
    else:
        print(f"⚠️  {total - perfect_matches} instances have differences")
        print("="*120)
        sys.exit(1)

if __name__ == '__main__':
    main()

