#!/usr/bin/env python3
"""
Comparison Script: path_based_formulation vs path_based_formulation_original

This script runs both versions on ALL instances (excluding Big) to validate that
the JSON parser migration doesn't affect solver results.

Usage:
    python compare_path_based_versions.py
"""

import os
import sys
import subprocess
import csv
import glob
from pathlib import Path
from datetime import datetime

# Configuration
EXECUTABLE_JSON = "build/executables/path_based_formulation"
EXECUTABLE_LEGACY = "build/executables/path_based_formulation_original"
TIME_LIMIT = 30  # seconds
HEURISTICS = 0.5

def find_all_instances():
    """Find all JSON instances, excluding Big ones"""
    json_instances = []
    legacy_instances = []
    
    # Find all JSON instances
    json_files = glob.glob("data/input/*.json")
    json_files.sort()
    
    for json_file in json_files:
        # Skip Big instances
        if "Big" in json_file or "big" in json_file:
            continue
        
        json_instances.append(json_file)
        
        # Find corresponding legacy instance
        instance_name = Path(json_file).stem
        legacy_path = f"data/input/test_instances/{instance_name}"
        if os.path.exists(legacy_path):
            legacy_instances.append(legacy_path)
    
    return json_instances, legacy_instances

def run_solver(executable, instance_file, output_file):
    """Run solver and capture output"""
    try:
        cmd = [executable, instance_file, output_file, str(TIME_LIMIT), str(HEURISTICS)]
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=TIME_LIMIT + 10  # Add buffer
        )
        return result.returncode == 0, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        return False, "", "Timeout"
    except Exception as e:
        return False, "", str(e)

def parse_solution_file(solution_file):
    """Parse the .sol file to extract objective value"""
    try:
        with open(solution_file, 'r') as f:
            lines = f.readlines()
            if len(lines) >= 2:
                # Format: first line is objective, second line is edges
                objective = float(lines[0].strip())
                edges = lines[1].strip().split() if len(lines) > 1 else []
                return objective, len(edges) // 2  # Each edge is two numbers
            return None, None
    except Exception as e:
        return None, str(e)

def compare_results(results_json, results_legacy):
    """Compare results from both versions"""
    matches = []
    differences = []
    
    for instance in results_json:
        if instance in results_legacy:
            json_obj = results_json[instance].get('objective')
            legacy_obj = results_legacy[instance].get('objective')
            
            if json_obj is not None and legacy_obj is not None:
                diff = abs(json_obj - legacy_obj)
                if diff < 0.01:  # Numerical tolerance
                    matches.append({
                        'instance': instance,
                        'objective': json_obj,
                        'status': 'MATCH'
                    })
                else:
                    differences.append({
                        'instance': instance,
                        'json_objective': json_obj,
                        'legacy_objective': legacy_obj,
                        'difference': diff,
                        'status': 'DIFFERENT'
                    })
            else:
                differences.append({
                    'instance': instance,
                    'json_objective': json_obj,
                    'legacy_objective': legacy_obj,
                    'status': 'ERROR'
                })
    
    return matches, differences

def main():
    print("=" * 70)
    print("Path-Based Formulation Comparison")
    print("JSON version vs Original Legacy version")
    print("=" * 70)
    print()
    
    # Check executables exist
    if not os.path.exists(EXECUTABLE_JSON):
        print(f"❌ JSON executable not found: {EXECUTABLE_JSON}")
        print("   Run: make path_based_formulation")
        sys.exit(1)
    
    if not os.path.exists(EXECUTABLE_LEGACY):
        print(f"❌ Legacy executable not found: {EXECUTABLE_LEGACY}")
        print("   Run: make path_based_original")
        sys.exit(1)
    
    print("✓ Both executables found")
    print()
    
    # Find all instances
    INSTANCES, LEGACY_INSTANCES = find_all_instances()
    
    print(f"Found {len(INSTANCES)} JSON instances to test")
    print(f"Found {len(LEGACY_INSTANCES)} corresponding legacy instances")
    print()
    
    if len(INSTANCES) == 0:
        print("❌ No instances found!")
        sys.exit(1)
    
    results_json = {}
    results_legacy = {}
    
    # Run JSON version
    print("Running JSON version (path_based_formulation)...")
    print("-" * 70)
    for instance in INSTANCES:
        if not os.path.exists(instance):
            print(f"⚠ Skipping {instance} (not found)")
            continue
        
        instance_name = Path(instance).stem
        output_csv = f"experiments/results/tmp_json_{instance_name}.csv"
        solution_file = f"data/output/test_instances/complete_{instance_name}.sol"
        
        print(f"  Processing: {instance_name}...", end=" ", flush=True)
        success, stdout, stderr = run_solver(EXECUTABLE_JSON, instance, output_csv)
        
        if success:
            objective, num_edges = parse_solution_file(solution_file)
            if objective is not None:
                print(f"✓ Objective: {objective:.2f}")
                results_json[instance_name] = {
                    'objective': objective,
                    'num_edges': num_edges,
                    'stdout': stdout
                }
            else:
                print(f"✗ Could not parse solution file")
                results_json[instance_name] = {'objective': None, 'error': num_edges}
        else:
            print(f"✗ Failed: {stderr[:50]}")
            results_json[instance_name] = {'objective': None, 'error': stderr[:100]}
    
    print()
    
    # Run Legacy version
    print("Running Legacy version (path_based_formulation_original)...")
    print("-" * 70)
    for instance in LEGACY_INSTANCES:
        if not os.path.exists(instance):
            print(f"⚠ Skipping {instance} (not found)")
            continue
        
        instance_name = Path(instance).name
        output_csv = f"experiments/results/tmp_legacy_{instance_name}.csv"
        solution_file = f"data/output/test_instances/complete_{instance_name}.sol"
        
        print(f"  Processing: {instance_name}...", end=" ", flush=True)
        success, stdout, stderr = run_solver(EXECUTABLE_LEGACY, instance, output_csv)
        
        if success:
            objective, num_edges = parse_solution_file(solution_file)
            if objective is not None:
                print(f"✓ Objective: {objective:.2f}")
                results_legacy[instance_name] = {
                    'objective': objective,
                    'num_edges': num_edges,
                    'stdout': stdout
                }
            else:
                print(f"✗ Could not parse solution file")
                results_legacy[instance_name] = {'objective': None, 'error': num_edges}
        else:
            print(f"✗ Failed: {stderr[:50]}")
            results_legacy[instance_name] = {'objective': None, 'error': stderr[:100]}
    
    print()
    
    # Compare results
    print("=" * 70)
    print("Comparison Results")
    print("=" * 70)
    print()
    
    matches, differences = compare_results(results_json, results_legacy)
    
    print(f"✓ Matches: {len(matches)}")
    for match in matches:
        print(f"  - {match['instance']}: Objective = {match['objective']:.2f}")
    
    print()
    print(f"{'✗' if differences else '✓'} Differences: {len(differences)}")
    for diff in differences:
        print(f"  - {diff['instance']}:")
        print(f"      JSON:   {diff.get('json_objective', 'N/A')}")
        print(f"      Legacy: {diff.get('legacy_objective', 'N/A')}")
        if 'difference' in diff:
            print(f"      Diff:   {diff['difference']:.6f}")
    
    print()
    print("=" * 70)
    if not differences:
        print("✅ SUCCESS: All results match! JSON parser migration validated.")
    else:
        print("⚠️  WARNING: Differences found. Investigate further.")
    print("=" * 70)
    
    # Save comparison report
    report_file = f"experiments/results/comparison_report_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
    with open(report_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['instance', 'json_objective', 'legacy_objective', 'difference', 'status'])
        for match in matches:
            writer.writerow([match['instance'], match['objective'], match['objective'], 0, 'MATCH'])
        for diff in differences:
            writer.writerow([
                diff['instance'],
                diff.get('json_objective', ''),
                diff.get('legacy_objective', ''),
                diff.get('difference', ''),
                diff['status']
            ])
    
    print(f"\n📊 Report saved to: {report_file}")

if __name__ == "__main__":
    main()
