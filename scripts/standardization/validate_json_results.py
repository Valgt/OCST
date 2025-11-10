#!/usr/bin/env python3
"""
Validate JSON Results Against Schema and Historical Data
=========================================================

Validates that generated JSON results conform to result.schema.v1.json
and match historical validation data.
"""

import json
import sys
from pathlib import Path
from typing import Dict, List, Tuple

def load_schema(schema_path: Path) -> Dict:
    """Load JSON schema."""
    with open(schema_path, 'r') as f:
        return json.load(f)

def validate_result_json(result_path: Path) -> Tuple[bool, List[str]]:
    """
    Validate a single result JSON file.
    
    Returns:
        (is_valid, list_of_errors)
    """
    errors = []
    
    try:
        with open(result_path, 'r') as f:
            data = json.load(f)
    except json.JSONDecodeError as e:
        return False, [f"Invalid JSON: {e}"]
    except Exception as e:
        return False, [f"Cannot read file: {e}"]
    
    # Check required top-level fields
    required_fields = [
        'schema_version', 'run_uuid', 'instance', 'solver',
        'config', 'optimization_status', 'results', 'runtime',
        'reproducibility'
    ]
    
    for field in required_fields:
        if field not in data:
            errors.append(f"Missing required field: {field}")
    
    # Validate schema version
    if data.get('schema_version') != '1.0':
        errors.append(f"Invalid schema_version: {data.get('schema_version')} (expected 1.0)")
    
    # Validate instance section
    instance = data.get('instance', {})
    if 'name' not in instance:
        errors.append("Missing instance.name")
    
    # Validate solver section
    solver = data.get('solver', {})
    for field in ['id', 'version', 'formulation']:
        if field not in solver:
            errors.append(f"Missing solver.{field}")
    
    # Validate optimization_status
    opt_status = data.get('optimization_status', {})
    if 'code' not in opt_status:
        errors.append("Missing optimization_status.code")
    if 'has_solution' not in opt_status:
        errors.append("Missing optimization_status.has_solution")
    
    # Validate results section
    results = data.get('results', {})
    for field in ['objective', 'primal_bound', 'dual_bound']:
        if field not in results:
            errors.append(f"Missing results.{field}")
    
    # Validate runtime section
    runtime = data.get('runtime', {})
    for field in ['wall_clock_seconds', 'solver_nodes']:
        if field not in runtime:
            errors.append(f"Missing runtime.{field}")
    
    # Validate solution if has_solution is True
    if opt_status.get('has_solution'):
        if 'solution' not in data:
            errors.append("has_solution is True but 'solution' field is missing")
        else:
            solution = data['solution']
            if 'tree_edges' not in solution:
                errors.append("Missing solution.tree_edges")
            if 'tree_cost' not in solution:
                errors.append("Missing solution.tree_cost")
    
    return len(errors) == 0, errors

def compare_with_historical(results_dir: Path, historical_csv: Path) -> Tuple[int, int, List[str]]:
    """
    Compare JSON results with historical CSV data.
    
    Returns:
        (matches, mismatches, mismatch_details)
    """
    # Load historical data
    historical = {}
    with open(historical_csv, 'r') as f:
        lines = f.readlines()
        header = lines[0].strip().split(',')
        for line in lines[1:]:
            parts = line.strip().split(',')
            if len(parts) >= 4:
                instance = parts[0]  # instance name
                obj = float(parts[3])  # migrating_objective
                historical[instance] = obj
    
    # Load current JSON results
    json_files = sorted(results_dir.glob('*.results.json'))
    
    matches = 0
    mismatches = 0
    mismatch_details = []
    
    for json_file in json_files:
        with open(json_file, 'r') as f:
            data = json.load(f)
        
        instance_name = data['instance']['name']
        current_obj = data['results']['objective']
        
        if instance_name in historical:
            hist_obj = historical[instance_name]
            diff = abs(current_obj - hist_obj)
            
            if diff < 1e-6:
                matches += 1
            else:
                mismatches += 1
                mismatch_details.append(
                    f"{instance_name}: Historical={hist_obj}, Current={current_obj}, Diff={diff}"
                )
    
    return matches, mismatches, mismatch_details

def main():
    results_dir = Path('data/output/test_instances')
    historical_csv = Path('experiments/validation/dual_run_20251103_225726/comparison_report.csv')
    
    print("="*80)
    print("JSON Results Validation")
    print("="*80)
    print()
    
    # Find all JSON result files
    json_files = sorted(results_dir.glob('*.results.json'))
    
    if not json_files:
        print(f"❌ No JSON files found in {results_dir}")
        sys.exit(1)
    
    print(f"Found {len(json_files)} JSON result files")
    print()
    
    # Validate each JSON
    print("Validating JSON Schema Compliance...")
    print("-" * 80)
    
    valid_count = 0
    invalid_count = 0
    
    for json_file in json_files:
        is_valid, errors = validate_result_json(json_file)
        
        if is_valid:
            valid_count += 1
            print(f"✓ {json_file.name}")
        else:
            invalid_count += 1
            print(f"✗ {json_file.name}")
            for error in errors:
                print(f"  - {error}")
    
    print("-" * 80)
    print(f"Schema Validation: {valid_count}/{len(json_files)} passed")
    print()
    
    if invalid_count > 0:
        print(f"❌ {invalid_count} files failed schema validation")
        sys.exit(1)
    
    # Compare with historical data
    if historical_csv.exists():
        print("Comparing with Historical Results...")
        print("-" * 80)
        
        matches, mismatches, details = compare_with_historical(results_dir, historical_csv)
        
        print(f"Matches: {matches}")
        print(f"Mismatches: {mismatches}")
        
        if mismatches > 0:
            print("\nMismatch Details:")
            for detail in details:
                print(f"  - {detail}")
            print()
            print(f"❌ {mismatches} results don't match historical data")
            sys.exit(1)
        else:
            print()
            print(f"✓ All {matches} results match historical data (100% parity)")
    else:
        print(f"⚠️  Historical data not found at {historical_csv}")
        print("Skipping historical comparison")
    
    print()
    print("="*80)
    print("✓✓✓ ALL VALIDATIONS PASSED ✓✓✓")
    print("="*80)
    sys.exit(0)

if __name__ == '__main__':
    main()

