#!/usr/bin/env python3
"""
CSV Post-Processor for OCST Results
====================================

Converts JSON result files (result.schema.v1.json) to CSV format for quick analysis.

JSON remains the source of truth; CSV is generated on-demand for convenience.

Usage:
    python results_to_csv.py <results_dir> [--output OUTPUT] [--aggregate]
    
Examples:
    # Single formulation results
    python results_to_csv.py experiments/path_based_formulation/standardized/
    
    # Aggregate multiple formulations
    python results_to_csv.py experiments/ --aggregate
"""

import argparse
import csv
import json
import sys
from pathlib import Path
from typing import List, Dict, Any
from datetime import datetime


def load_result_json(filepath: Path) -> Dict[str, Any]:
    """Load and validate a single result JSON file."""
    try:
        with open(filepath, 'r') as f:
            data = json.load(f)
        
        # Basic validation
        if data.get('schema_version') != '1.0':
            print(f"Warning: {filepath} has unexpected schema version: {data.get('schema_version')}")
        
        return data
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON in {filepath}: {e}", file=sys.stderr)
        return None
    except Exception as e:
        print(f"Error: Cannot read {filepath}: {e}", file=sys.stderr)
        return None


def extract_csv_fields(result: Dict[str, Any]) -> Dict[str, Any]:
    """Extract relevant fields from result JSON for CSV export."""
    
    # Extract nested fields safely
    instance_name = result.get('instance', {}).get('name', 'unknown')
    instance_tags = ','.join(result.get('instance', {}).get('tags', []))
    
    solver_id = result.get('solver', {}).get('id', 'unknown')
    solver_version = result.get('solver', {}).get('version', 'unknown')
    formulation = result.get('solver', {}).get('formulation', 'unknown')
    
    opt_status = result.get('optimization_status', {})
    status_code = opt_status.get('code', 'UNKNOWN')
    has_solution = opt_status.get('has_solution', False)
    has_bound = opt_status.get('has_bound', False)
    
    results_section = result.get('results', {})
    objective = results_section.get('objective', None)
    primal_bound = results_section.get('primal_bound', None)
    dual_bound = results_section.get('dual_bound', None)
    gap_percent = results_section.get('gap_percent', None)
    best_solution_time = results_section.get('best_solution_time', None)
    
    runtime = result.get('runtime', {})
    wall_clock = runtime.get('wall_clock_seconds', None)
    cpu_seconds = runtime.get('cpu_seconds', None)
    solver_nodes = runtime.get('solver_nodes', None)
    termination_reason = runtime.get('termination_reason', '')
    
    repro = result.get('reproducibility', {})
    git_commit = repro.get('git_commit', 'unknown')
    git_dirty = repro.get('git_dirty', False)
    seed = repro.get('seed', None)
    timestamp = repro.get('timestamp', '')
    
    run_uuid = result.get('run_uuid', 'unknown')
    
    return {
        'run_uuid': run_uuid,
        'instance_name': instance_name,
        'instance_tags': instance_tags,
        'solver_id': solver_id,
        'solver_version': solver_version,
        'formulation': formulation,
        'status': status_code,
        'has_solution': has_solution,
        'has_bound': has_bound,
        'objective': objective,
        'primal_bound': primal_bound,
        'dual_bound': dual_bound,
        'gap_percent': gap_percent,
        'best_solution_time': best_solution_time,
        'wall_clock_seconds': wall_clock,
        'cpu_seconds': cpu_seconds,
        'solver_nodes': solver_nodes,
        'termination_reason': termination_reason,
        'git_commit': git_commit,
        'git_dirty': git_dirty,
        'seed': seed,
        'timestamp': timestamp,
    }


def collect_results(results_dir: Path, aggregate: bool = False) -> List[Dict[str, Any]]:
    """
    Collect all result JSON files from directory.
    
    Args:
        results_dir: Directory to search for .results.json files
        aggregate: If True, search recursively; if False, only direct children
        
    Returns:
        List of extracted CSV field dictionaries
    """
    if aggregate:
        json_files = sorted(results_dir.rglob('*.results.json'))
    else:
        json_files = sorted(results_dir.glob('*.results.json'))
    
    if not json_files:
        print(f"Warning: No *.results.json files found in {results_dir}", file=sys.stderr)
        return []
    
    print(f"Found {len(json_files)} result file(s)")
    
    rows = []
    for filepath in json_files:
        result_data = load_result_json(filepath)
        if result_data:
            row = extract_csv_fields(result_data)
            row['source_file'] = str(filepath)
            rows.append(row)
    
    print(f"Successfully loaded {len(rows)} result(s)")
    return rows


def write_csv(rows: List[Dict[str, Any]], output_path: Path):
    """Write results to CSV file."""
    if not rows:
        print("No data to write", file=sys.stderr)
        return
    
    # CSV column order
    fieldnames = [
        'run_uuid',
        'instance_name',
        'instance_tags',
        'solver_id',
        'solver_version',
        'formulation',
        'status',
        'has_solution',
        'has_bound',
        'objective',
        'primal_bound',
        'dual_bound',
        'gap_percent',
        'best_solution_time',
        'wall_clock_seconds',
        'cpu_seconds',
        'solver_nodes',
        'termination_reason',
        'git_commit',
        'git_dirty',
        'seed',
        'timestamp',
        'source_file',
    ]
    
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(output_path, 'w', newline='') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)
    
    print(f"✓ CSV written to {output_path} ({len(rows)} row(s))")


def main():
    parser = argparse.ArgumentParser(
        description='Convert OCST JSON results to CSV format',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    
    parser.add_argument(
        'results_dir',
        type=Path,
        help='Directory containing *.results.json files'
    )
    
    parser.add_argument(
        '--output', '-o',
        type=Path,
        default=None,
        help='Output CSV file path (default: <results_dir>/summary.csv)'
    )
    
    parser.add_argument(
        '--aggregate',
        action='store_true',
        help='Recursively aggregate all *.results.json files from subdirectories'
    )
    
    args = parser.parse_args()
    
    # Validate input directory
    if not args.results_dir.exists():
        print(f"Error: Directory not found: {args.results_dir}", file=sys.stderr)
        sys.exit(1)
    
    if not args.results_dir.is_dir():
        print(f"Error: Not a directory: {args.results_dir}", file=sys.stderr)
        sys.exit(1)
    
    # Determine output path
    if args.output is None:
        args.output = args.results_dir / 'summary.csv'
    
    print(f"Reading results from: {args.results_dir}")
    print(f"Aggregate mode: {'ON (recursive)' if args.aggregate else 'OFF (single directory)'}")
    print()
    
    # Collect and convert
    rows = collect_results(args.results_dir, aggregate=args.aggregate)
    
    if not rows:
        print("No results to export", file=sys.stderr)
        sys.exit(1)
    
    write_csv(rows, args.output)
    print()
    print(f"✓ Export complete")


if __name__ == '__main__':
    main()

