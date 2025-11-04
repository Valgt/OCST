#!/usr/bin/env python3
"""
Phase 1.5 Validation: path_based_formulation vs path_based_formulation_original

This script validates the JSON migration by comparing:
- Objective values (tolerance: 1e-6, matching Gurobi MIP gap)
- Solution trees (edge-by-edge comparison)
- Runtime characteristics

Usage:
    python scripts/standardization/validate_path_based_migration.py
    python scripts/standardization/validate_path_based_migration.py --quick  # Only quick_check instances

Output:
    - experiments/validation/dual_run_YYYYMMDD_HHMMSS/comparison_report.csv
    - experiments/validation/dual_run_YYYYMMDD_HHMMSS/detailed_log.txt
"""

import argparse
import csv
import json
import os
import subprocess
import sys
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Tuple

# =============================================================================
# CONFIGURATION
# =============================================================================

NUMERICAL_TOLERANCE = 1e-6  # Match Gurobi MIP gap tolerance
TIME_LIMIT = 60  # seconds (increased for proper solve)
HEURISTICS = 0.5

EXECUTABLE_MIGRATING = Path("build/executables/path_based_formulation")
EXECUTABLE_CONTROL = Path("build/executables/path_based_formulation_original")

INPUT_DIR = Path("data/input")
OUTPUT_DIR = Path("experiments/validation")

# =============================================================================
# UTILITIES
# =============================================================================

def log(message: str, file_handle=None):
    """Print and optionally write to log file"""
    print(message)
    if file_handle:
        file_handle.write(message + "\n")
        file_handle.flush()

def get_quick_check_instances() -> List[Path]:
    """Get instances tagged with 'quick_check' from JSON metadata"""
    instances = []
    for json_file in INPUT_DIR.glob("*.json"):
        try:
            with open(json_file, 'r') as f:
                data = json.load(f)
                tags = data.get("tags", [])
                if "quick_check" in tags:
                    instances.append(json_file)
        except (json.JSONDecodeError, OSError):
            continue
    return sorted(instances)

def get_all_instances(exclude_big: bool = True) -> List[Path]:
    """Get all JSON instances, optionally excluding Big ones"""
    instances = []
    for json_file in INPUT_DIR.glob("*.json"):
        if exclude_big and ("Big" in json_file.name or "big" in json_file.name):
            continue
        instances.append(json_file)
    return sorted(instances)

# =============================================================================
# SOLVER EXECUTION
# =============================================================================

def run_migrating_solver(instance_path: Path, output_csv: Path, timeout: float) -> Tuple[bool, str, str]:
    """
    Run path_based_formulation (JSON input/output)
    
    Returns: (success, stdout, stderr)
    """
    cmd = [
        str(EXECUTABLE_MIGRATING),
        str(instance_path),
        str(output_csv),
        str(TIME_LIMIT),
        str(HEURISTICS)
    ]
    
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            cwd=Path.cwd()
        )
        return result.returncode == 0, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        return False, "", f"Timeout after {timeout}s"
    except Exception as e:
        return False, "", str(e)

def run_control_solver(instance_path: Path, output_csv: Path, timeout: float) -> Tuple[bool, str, str]:
    """
    Run path_based_formulation_original (legacy format)
    
    Expects legacy .ocstpin file at data/input/test_instances/{name}
    Returns: (success, stdout, stderr)
    """
    instance_name = instance_path.stem
    legacy_path = Path("data/input/test_instances") / instance_name
    
    if not legacy_path.exists():
        return False, "", f"Legacy instance not found: {legacy_path}"
    
    cmd = [
        str(EXECUTABLE_CONTROL),
        str(legacy_path),
        str(output_csv),
        str(TIME_LIMIT),
        str(HEURISTICS)
    ]
    
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            cwd=Path.cwd()
        )
        return result.returncode == 0, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        return False, "", f"Timeout after {timeout}s"
    except Exception as e:
        return False, "", str(e)

# =============================================================================
# RESULT PARSING
# =============================================================================

def parse_json_result(instance_name: str) -> Dict:
    """Parse JSON result from path_based_formulation"""
    json_path = Path(f"data/output/test_instances/{instance_name}.results.json")
    
    if not json_path.exists():
        return {"error": "JSON result file not found"}
    
    try:
        with open(json_path, 'r') as f:
            data = json.load(f)
        
        result = {
            "objective": data.get("results", {}).get("objective"),
            "status": data.get("optimization_status", {}).get("code"),
            "has_solution": data.get("optimization_status", {}).get("has_solution", False),
            "runtime": data.get("runtime", {}).get("wall_clock_seconds", 0),
            "edges": []
        }
        
        # Extract edges
        if "solution" in data and "tree_edges" in data["solution"]:
            for edge in data["solution"]["tree_edges"]:
                u, v = edge["source"], edge["destination"]
                result["edges"].append(tuple(sorted([u, v])))
        
        return result
        
    except (json.JSONDecodeError, OSError, KeyError) as e:
        return {"error": f"Failed to parse JSON: {e}"}

def parse_sol_result(instance_name: str) -> Dict:
    """
    Parse .sol result from path_based_formulation_original
    
    Format:
        Line 1: objective
        Line 2: num_nodes
        Line 3+: edges (one per line, format "u v")
    """
    sol_path = Path(f"data/output/test_instances/complete_{instance_name}.sol")
    
    if not sol_path.exists():
        return {"error": "SOL result file not found"}
    
    try:
        with open(sol_path, 'r') as f:
            lines = f.readlines()
        
        if len(lines) < 2:
            return {"error": "SOL file too short"}
        
        objective = float(lines[0].strip())
        num_nodes = int(lines[1].strip())  # Line 2 is num_nodes, not num_edges
        
        # Parse edges: one edge per line starting from line 3 (index 2)
        edges = []
        for i in range(2, len(lines)):
            edge_line = lines[i].strip()
            if not edge_line:  # Skip empty lines
                continue
            tokens = edge_line.split()
            if len(tokens) >= 2:
                u, v = int(tokens[0]), int(tokens[1])
                edges.append(tuple(sorted([u, v])))
        
        return {
            "objective": objective,
            "status": "OPTIMAL",  # Assume optimal if solved
            "has_solution": True,
            "num_nodes": num_nodes,
            "edges": edges
        }
        
    except (ValueError, OSError, IndexError) as e:
        return {"error": f"Failed to parse SOL: {e}"}

# =============================================================================
# COMPARISON
# =============================================================================

def compare_results(instance_name: str, migrating: Dict, control: Dict) -> Dict:
    """
    Compare results with strict numerical tolerance
    
    Returns dict with comparison status
    """
    comparison = {
        "instance": instance_name,
        "migrating_status": "ERROR",
        "control_status": "ERROR",
        "migrating_objective": None,
        "control_objective": None,
        "objective_diff": None,
        "objective_match": False,
        "migrating_edges": 0,
        "control_edges": 0,
        "edges_match": False,
        "edges_symmetric_diff": 0,
        "notes": []
    }
    
    # Check for errors
    if "error" in migrating:
        comparison["notes"].append(f"Migrating error: {migrating['error']}")
        return comparison
    
    if "error" in control:
        comparison["notes"].append(f"Control error: {control['error']}")
        return comparison
    
    # Extract values
    comparison["migrating_status"] = migrating.get("status", "UNKNOWN")
    comparison["control_status"] = control.get("status", "UNKNOWN")
    comparison["migrating_objective"] = migrating.get("objective")
    comparison["control_objective"] = control.get("objective")
    
    # Compare objectives with strict tolerance
    if comparison["migrating_objective"] is not None and comparison["control_objective"] is not None:
        obj_diff = abs(comparison["migrating_objective"] - comparison["control_objective"])
        comparison["objective_diff"] = obj_diff
        comparison["objective_match"] = obj_diff < NUMERICAL_TOLERANCE
        
        if not comparison["objective_match"]:
            comparison["notes"].append(f"OBJECTIVE MISMATCH: diff={obj_diff:.10e} > tolerance={NUMERICAL_TOLERANCE}")
    else:
        comparison["notes"].append("Missing objective in one or both results")
    
    # Compare edges
    migrating_edges = set(migrating.get("edges", []))
    control_edges = set(control.get("edges", []))
    
    comparison["migrating_edges"] = len(migrating_edges)
    comparison["control_edges"] = len(control_edges)
    
    symmetric_diff = migrating_edges.symmetric_difference(control_edges)
    comparison["edges_symmetric_diff"] = len(symmetric_diff)
    comparison["edges_match"] = len(symmetric_diff) == 0
    
    if not comparison["edges_match"]:
        in_migrating_only = migrating_edges - control_edges
        in_control_only = control_edges - migrating_edges
        comparison["notes"].append(f"EDGE MISMATCH: {len(in_migrating_only)} only in migrating, {len(in_control_only)} only in control")
        if len(in_migrating_only) <= 3:
            comparison["notes"].append(f"  Migrating only: {in_migrating_only}")
        if len(in_control_only) <= 3:
            comparison["notes"].append(f"  Control only: {in_control_only}")
    
    return comparison

# =============================================================================
# MAIN VALIDATION
# =============================================================================

def run_validation(instances: List[Path], output_dir: Path):
    """Run dual-solver validation on all instances"""
    
    # Setup output directory
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    run_dir = output_dir / f"dual_run_{timestamp}"
    run_dir.mkdir(parents=True, exist_ok=True)
    
    log_file = run_dir / "detailed_log.txt"
    csv_file = run_dir / "comparison_report.csv"
    
    with open(log_file, 'w') as log_handle:
        log("=" * 80, log_handle)
        log("Phase 1.5 Validation: path_based_formulation Migration", log_handle)
        log("=" * 80, log_handle)
        log(f"Timestamp: {timestamp}", log_handle)
        log(f"Numerical tolerance: {NUMERICAL_TOLERANCE}", log_handle)
        log(f"Time limit: {TIME_LIMIT}s", log_handle)
        log(f"Instances: {len(instances)}", log_handle)
        log(f"Output directory: {run_dir}", log_handle)
        log("", log_handle)
        
        # Check executables
        if not EXECUTABLE_MIGRATING.exists():
            log(f"❌ Migrating executable not found: {EXECUTABLE_MIGRATING}", log_handle)
            log("   Run: make path_based", log_handle)
            sys.exit(1)
        
        if not EXECUTABLE_CONTROL.exists():
            log(f"❌ Control executable not found: {EXECUTABLE_CONTROL}", log_handle)
            log("   Run: make path_based_original", log_handle)
            sys.exit(1)
        
        log("✓ Both executables found", log_handle)
        log("", log_handle)
        
        # Run comparisons
        comparisons = []
        
        for i, instance_path in enumerate(instances, 1):
            instance_name = instance_path.stem
            
            log("-" * 80, log_handle)
            log(f"[{i}/{len(instances)}] {instance_name}", log_handle)
            log("-" * 80, log_handle)
            
            # Run migrating solver
            log("  Running migrating solver (path_based_formulation)...", log_handle)
            output_csv_migrating = run_dir / f"{instance_name}_migrating.csv"
            success_m, stdout_m, stderr_m = run_migrating_solver(instance_path, output_csv_migrating, TIME_LIMIT + 30)
            
            if not success_m:
                log(f"    ❌ Failed: {stderr_m[:100]}", log_handle)
            else:
                log(f"    ✓ Completed", log_handle)
            
            # Run control solver
            log("  Running control solver (path_based_formulation_original)...", log_handle)
            output_csv_control = run_dir / f"{instance_name}_control.csv"
            success_c, stdout_c, stderr_c = run_control_solver(instance_path, output_csv_control, TIME_LIMIT + 30)
            
            if not success_c:
                log(f"    ❌ Failed: {stderr_c[:100]}", log_handle)
            else:
                log(f"    ✓ Completed", log_handle)
            
            # Parse results
            log("  Parsing results...", log_handle)
            migrating_result = parse_json_result(instance_name)
            control_result = parse_sol_result(instance_name)
            
            # Compare
            comparison = compare_results(instance_name, migrating_result, control_result)
            comparisons.append(comparison)
            
            # Log comparison
            if comparison["objective_match"]:
                log(f"    ✅ Objectives MATCH: {comparison['migrating_objective']:.10f}", log_handle)
            else:
                log(f"    ❌ Objectives DIFFER:", log_handle)
                log(f"       Migrating: {comparison['migrating_objective']}", log_handle)
                log(f"       Control:   {comparison['control_objective']}", log_handle)
                log(f"       Diff:      {comparison['objective_diff']:.10e}", log_handle)
            
            if comparison["edges_match"]:
                log(f"    ✅ Edges MATCH: {comparison['migrating_edges']} edges", log_handle)
            else:
                log(f"    ❌ Edges DIFFER:", log_handle)
                log(f"       Migrating: {comparison['migrating_edges']} edges", log_handle)
                log(f"       Control:   {comparison['control_edges']} edges", log_handle)
                log(f"       Symmetric diff: {comparison['edges_symmetric_diff']}", log_handle)
            
            for note in comparison["notes"]:
                log(f"    ⚠️  {note}", log_handle)
            
            log("", log_handle)
        
        # Summary
        log("=" * 80, log_handle)
        log("SUMMARY", log_handle)
        log("=" * 80, log_handle)
        
        total = len(comparisons)
        objective_matches = sum(1 for c in comparisons if c["objective_match"])
        edge_matches = sum(1 for c in comparisons if c["edges_match"])
        
        log(f"Total instances: {total}", log_handle)
        log(f"Objective matches: {objective_matches}/{total} ({100*objective_matches/total:.1f}%)", log_handle)
        log(f"Edge matches: {edge_matches}/{total} ({100*edge_matches/total:.1f}%)", log_handle)
        log("", log_handle)
        
        if objective_matches == total and edge_matches == total:
            log("✅ ✅ ✅  VALIDATION PASSED  ✅ ✅ ✅", log_handle)
            log("All results match within numerical tolerance!", log_handle)
            log(f"Migration ready for promotion to src/cpp/common/", log_handle)
        else:
            log("❌ ❌ ❌  VALIDATION FAILED  ❌ ❌ ❌", log_handle)
            log("Discrepancies found. Investigation required.", log_handle)
        
        log("=" * 80, log_handle)
        
        # Write CSV report
        with open(csv_file, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow([
                'instance',
                'migrating_status',
                'control_status',
                'migrating_objective',
                'control_objective',
                'objective_diff',
                'objective_match',
                'migrating_edges',
                'control_edges',
                'edges_match',
                'notes'
            ])
            
            for comp in comparisons:
                writer.writerow([
                    comp['instance'],
                    comp['migrating_status'],
                    comp['control_status'],
                    comp['migrating_objective'],
                    comp['control_objective'],
                    comp['objective_diff'],
                    comp['objective_match'],
                    comp['migrating_edges'],
                    comp['control_edges'],
                    comp['edges_match'],
                    '; '.join(comp['notes'])
                ])
        
        log(f"\n📊 CSV report: {csv_file}", log_handle)
        log(f"📝 Detailed log: {log_file}", log_handle)

# =============================================================================
# CLI
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Validate path_based_formulation migration (Phase 1.5)"
    )
    parser.add_argument(
        "--quick",
        action="store_true",
        help="Run only on quick_check instances"
    )
    parser.add_argument(
        "--exclude-big",
        action="store_true",
        default=True,
        help="Exclude Big instances (default: True)"
    )
    
    args = parser.parse_args()
    
    if args.quick:
        instances = get_quick_check_instances()
        print(f"Running validation on {len(instances)} quick_check instances")
    else:
        instances = get_all_instances(exclude_big=args.exclude_big)
        print(f"Running validation on {len(instances)} instances")
    
    if not instances:
        print("❌ No instances found!")
        sys.exit(1)
    
    run_validation(instances, OUTPUT_DIR)

if __name__ == "__main__":
    main()

