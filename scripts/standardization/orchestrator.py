#!/usr/bin/env python3
"""
OCST Experiment Orchestrator - Phase 1.5 Workstream 5

Unified experiment runner for all OCST formulations with standardized
JSON input/output, configuration management, and reproducibility tracking.
"""

import os
import sys
import json
import time
import argparse
import subprocess
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Any, Optional
import glob
import re


class OCSTOrchestrator:
    """Main orchestrator for OCST experiments."""

    def __init__(self):
        self.project_root = Path(__file__).parent.parent.parent
        self.data_dir = self.project_root / "data" / "input"
        self.experiments_dir = self.project_root / "experiments"
        self.build_dir = self.project_root / "build" / "executables"
        self.best_known_dir = self.project_root / "data" / "best known"

        # Ensure directories exist
        self.experiments_dir.mkdir(exist_ok=True)

        # Load best known optimal values
        self.best_known_values = self._load_best_known_values()

    def _load_best_known_values(self) -> Dict[str, float]:
        """Load optimal objective values from best known solutions."""
        best_known = {}
        if self.best_known_dir.exists():
            for sol_file in self.best_known_dir.glob("*.sol"):
                instance_name = sol_file.stem
                try:
                    with open(sol_file, 'r') as f:
                        best_known[instance_name] = float(f.read().strip())
                except (ValueError, OSError):
                    pass  # Skip malformed files
        return best_known

    def run_experiment(self, args: argparse.Namespace) -> None:
        """Run a complete experiment with multiple formulations."""

        # Validate formulations
        formulations = list(set(args.formulation))  # Remove duplicates while preserving order
        if not formulations:
            raise ValueError("At least one formulation must be specified")

        # Create experiment directory structure
        timestamp = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S")
        experiment_name = f"{args.tag}_{timestamp}"
        experiment_dir = self.experiments_dir / experiment_name

        experiment_dir.mkdir(parents=True, exist_ok=True)

        print(f"🚀 Starting experiment: {experiment_name}")
        print(f"📁 Output directory: {experiment_dir}")
        print(f"🏷️  Tag: {args.tag}")
        print(f"🔬 Formulations: {', '.join(formulations)}")
        print(f"🎲 Seed: {args.seed}")
        print()

        # Find instances by tag
        instances = self.find_instances_by_tag(args.tag)
        if not instances:
            raise ValueError(f"No instances found with tag '{args.tag}'")

        print(f"📊 Found {len(instances)} instances with tag '{args.tag}':")
        for instance in instances[:5]:  # Show first 5
            print(f"  - {instance['name']}")
        if len(instances) > 5:
            print(f"  ... and {len(instances) - 5} more")
        print()

        # Compile all formulations first
        for formulation in formulations:
            print(f"🔨 Compiling formulation: {formulation}")
            self.compile_formulation(formulation)
        print()

        # Prepare configuration
        config = self.prepare_config(args)

        # Execute experiments for each formulation
        all_results = {}
        formulation_summaries = {}

        total_instances = len(instances)
        total_formulations = len(formulations)

        print(f"⚡ Executing {total_instances} × {total_formulations} = {total_instances * total_formulations} experiments...")
        print("=" * 80)

        for form_idx, formulation in enumerate(formulations, 1):
            print(f"🔬 Formulation {form_idx}/{total_formulations}: {formulation}")
            print("-" * 80)

            # Create formulation-specific directories
            formulation_dir = experiment_dir / formulation
            results_dir = formulation_dir / "results"
            results_dir.mkdir(parents=True, exist_ok=True)

            # Execute all instances for this formulation
            results = []
            successful = 0
            failed = 0

            for i, instance in enumerate(instances, 1):
                instance_name = instance['name']
                print(f"[{form_idx}.{i:2d}/{total_formulations}.{total_instances}] Processing: {instance_name}")

                try:
                    result = self.run_single_instance(
                        formulation,
                        instance_name,
                        args.seed,
                        config,
                        results_dir
                    )
                    results.append(result)
                    successful += 1
                    status = "✓"
                except Exception as e:
                    error_result = {
                        "instance_name": instance_name,
                        "success": False,
                        "objective": None,
                        "optimization_status_description": "ERROR",
                        "stdout": "",
                        "stderr": str(e),
                        "timestamp": datetime.now(timezone.utc).isoformat(),
                        "error": str(e)
                    }
                    results.append(error_result)
                    failed += 1
                    status = "✗"
                    print(f"  Error: {e}")

                objective_str = f"{result.get('objective', 'ERROR'):.0f}" if status == "✓" else "FAILED"
                print(f"  {status} {instance_name}: {objective_str}")

            # Store results for this formulation
            all_results[formulation] = results

            # Generate formulation-specific summary
            formulation_summary = self.generate_formulation_summary(
                f"{experiment_name}_{formulation}",
                args,
                formulation,
                instances,
                results,
                successful,
                failed,
                config
            )
            formulation_summaries[formulation] = formulation_summary

            # Save formulation-specific files
            summary_file = formulation_dir / "experiment_summary.json"
            with open(summary_file, 'w') as f:
                json.dump(formulation_summary, f, indent=2)

            results_file = results_dir / "detailed_results.json"
            with open(results_file, 'w') as f:
                json.dump(results, f, indent=2)

            print(f"✅ Formulation {formulation}: {successful}/{total_instances} successful")
            print()

        # Generate comparative summary
        comparative_summary = self.generate_comparative_summary(
            experiment_name, args, instances, all_results, formulation_summaries
        )

        # Save comparative summary
        comparative_summary_file = experiment_dir / "comparative_summary.json"
        with open(comparative_summary_file, 'w') as f:
            json.dump(comparative_summary, f, indent=2)

        # Print final results
        self.print_experiment_summary(comparative_summary, formulations)

        print(f"📄 Comparative Summary: {comparative_summary_file}")
        print(f"📁 Individual results in: {experiment_dir}/{{formulation}}/")

    def find_instances_by_tag(self, tag: str) -> List[Dict[str, Any]]:
        """Find all instances with the specified tag."""
        instances = []

        for json_file in self.data_dir.glob("*.json"):
            try:
                with open(json_file, 'r') as f:
                    data = json.load(f)

                if tag in data.get('tags', []):
                    instances.append({
                        'name': data['name'],
                        'file': json_file,
                        'tags': data['tags']
                    })
            except (json.JSONDecodeError, KeyError) as e:
                print(f"Warning: Skipping {json_file.name} - {e}")

        return sorted(instances, key=lambda x: x['name'])

    def compile_formulation(self, formulation: str) -> None:
        """Compile the specified formulation."""
        # Map formulation names to make targets
        make_target_map = {
            "path_based": "path_based",
            "path_based_formulation_original": "path_based_original",
            "flow_based": "flow_based",
            "flow_based_relaxed": "flow_based_relaxed"
        }

        make_target = make_target_map.get(formulation, formulation)

        try:
            result = subprocess.run(
                ["make", make_target],
                cwd=self.project_root,
                capture_output=True,
                text=True,
                check=True
            )
            print("  ✓ Compilation successful")
            print(f"  Output: {result.stdout.strip()}")
        except subprocess.CalledProcessError as e:
            raise RuntimeError(f"Compilation failed: {e.stderr}")

    def create_temp_legacy_file(self, instance_name: str) -> str:
        """Create a temporary legacy format file from JSON for path_based_formulation_original."""
        json_file = self.data_dir / f"{instance_name}.json"

        # Read JSON instance
        with open(json_file, 'r') as f:
            data = json.load(f)

        # Create temporary file
        temp_fd, temp_path = tempfile.mkstemp(suffix='.ocstpin')
        try:
            with os.fdopen(temp_fd, 'w') as temp_file:
                # Write legacy format: n m p
                # header: n m p
                graph = data['graph']
                n = graph['nodes']
                m = len(graph['edges'])
                p = len(data.get('requirements', []))

                temp_file.write(f"{n} {m} {p}\n")

                # Write edges: m lines of "u v cost"
                for edge in graph['edges']:
                    temp_file.write(f"{edge['source']} {edge['destination']} {edge['cost']}\n")

                # Write requirements: p lines of "u v demand"
                for req in data.get('requirements', []):
                    temp_file.write(f"{req['origin']} {req['destination']} {req['weight']}\n")

            return temp_path
        except Exception:
            os.close(temp_fd)
            raise

    def create_temp_legacy_file_correct_format(self, instance_name: str) -> str:
        """Create a temporary legacy format file in the CORRECT format expected by path_based_formulation_original."""
        json_file = self.data_dir / f"{instance_name}.json"

        # Read JSON instance
        with open(json_file, 'r') as f:
            data = json.load(f)

        # Create temporary file
        temp_fd, temp_path = tempfile.mkstemp(suffix='.ocstpin')
        try:
            with os.fdopen(temp_fd, 'w') as temp_file:
                # CORRECT FORMAT for path_based_formulation_original:
                # Line 1: n m probability
                # Lines 2-m+1: edges (u v cost)
                # Line m+2: num_requirements
                # Lines m+3+: requirements (origin dest weight)

                graph = data['graph']
                requirements = data.get('requirements', [])
                n = graph['nodes']
                m = len(graph['edges'])
                # Probability can be at root level or in metadata
                probability = data.get('probability', data.get('metadata', {}).get('probability', 0.0))

                # Line 1: n m probability
                temp_file.write(f"{n} {m} {probability}\n")

                # Lines 2-m+1: edges
                for edge in graph['edges']:
                    temp_file.write(f"{edge['source']} {edge['destination']} {edge['cost']}\n")

                # Line m+2: number of requirements
                temp_file.write(f"{len(requirements)}\n")

                # Lines m+3+: requirements
                for req in requirements:
                    temp_file.write(f"{req['origin']} {req['destination']} {req['weight']}\n")

            return temp_path
        except Exception:
            os.close(temp_fd)
            raise

    def prepare_config(self, args: argparse.Namespace) -> Dict[str, Any]:
        """Prepare configuration from CLI args and config files."""
        config = {}

        # Parse CLI config overrides
        if args.config:
            for item in args.config:
                if '=' in item:
                    key, value = item.split('=', 1)
                    # Try to parse as number or boolean
                    try:
                        # Try int
                        config[key] = int(value)
                    except ValueError:
                        try:
                            # Try float
                            config[key] = float(value)
                        except ValueError:
                            # Try boolean
                            if value.lower() in ('true', 'false'):
                                config[key] = value.lower() == 'true'
                            else:
                                # Keep as string
                                config[key] = value

        # Load config file if specified
        if args.config_file:
            with open(args.config_file, 'r') as f:
                file_config = json.load(f)
                config.update(file_config)

        return config

    def run_single_instance(self, formulation: str, instance_name: str,
                          seed: int, config: Dict[str, Any],
                          results_dir: Path) -> Dict[str, Any]:
        """Run formulation on a single instance."""

        # Map formulation names to executable names
        executable_map = {
            "path_based": "path_based_formulation",
            "path_based_formulation_original": "path_based_formulation_original",
            "flow_based": "flow_based",
            "flow_based_relaxed": "flow_based_relaxed"
        }

        executable_name = executable_map.get(formulation, formulation)
        executable = self.build_dir / executable_name
        if not executable.exists():
            raise FileNotFoundError(f"Executable not found: {executable}")

        # Different command interfaces for different formulations
        temp_legacy_file = None
        if formulation == "path_based_formulation_original":
            # Legacy interface: <instance_file> [output_csv] [time_limit] [heuristics]
            # Use the original legacy file directly from test_instances/
            legacy_file_path = self.data_dir / "test_instances" / instance_name
            if not legacy_file_path.exists():
                raise FileNotFoundError(f"Legacy file not found: {legacy_file_path}")
            temp_legacy_file = str(legacy_file_path)
            time_limit = config.get('time_limit', 3600.0)

            cmd = [
                str(executable),
                temp_legacy_file,
                "",  # empty output_csv (not used in new format)
                str(time_limit),
                "0.5"  # heuristics (fixed for legacy)
            ]
        else:
            # Modern interface with --flags
            cmd = [
                str(executable),
                "--instance", instance_name,  # Instance name, not file path
                "--seed", str(seed),
                "--output-dir", str(results_dir)
            ]

            # Add config overrides
            for key, value in config.items():
                cmd.extend([f"--{key}", str(value)])

        # Execute
        start_time = time.time()
        try:
            env = os.environ.copy()
            gurobi_home = env.get("GUROBI_HOME")
            if gurobi_home:
                lib_path = os.path.join(gurobi_home, "lib")
                current_ld = env.get("LD_LIBRARY_PATH", "")
                if lib_path not in current_ld.split(":"):
                    env["LD_LIBRARY_PATH"] = f"{lib_path}:{current_ld}" if current_ld else lib_path

            result = subprocess.run(
                cmd,
                cwd=self.project_root,
                capture_output=True,
                text=True,
                env=env,
                timeout=3600,  # 1 hour timeout
                check=True
            )

            # Handle different result formats
            if formulation == "path_based_formulation_original":
                # Legacy formulation doesn't produce JSON, parse stdout
                # Look for "Objective value:" in stdout
                objective = None
                runtime_seconds = None
                nodes_explored = None
                mip_gap = None
                lazy_constraints = None
                cutting_planes = None

                for line in result.stdout.split('\n'):
                    if "Objective value:" in line:
                        try:
                            # Extract number from line like "Objective value: 1340.00"
                            parts = line.split(':')
                            if len(parts) > 1:
                                objective = float(parts[1].strip())
                        except ValueError:
                            pass
                    elif "Runtime:" in line and "seconds" in line:
                        m = re.search(r"Runtime:\s*([0-9.]+)", line)
                        if m:
                            runtime_seconds = float(m.group(1))
                    elif "Nodes explored:" in line:
                        m = re.search(r"Nodes explored:\s*([0-9.eE+-]+)", line)
                        if m:
                            try:
                                nodes_explored = float(m.group(1))
                            except ValueError:
                                nodes_explored = None
                    elif "MIP gap:" in line and "%" in line:
                        m = re.search(r"MIP gap:\s*([0-9.+-eE]+)", line)
                        if m:
                            try:
                                mip_gap = float(m.group(1))
                            except ValueError:
                                mip_gap = None
                    elif "Lazy constraints added" in line:
                        m = re.search(r"Lazy constraints added:\s*([0-9.+-eE]+)", line)
                        if m:
                            try:
                                lazy_constraints = float(m.group(1))
                            except ValueError:
                                lazy_constraints = None
                    elif "Cutting planes added" in line:
                        m = re.search(r"Cutting planes added:\s*([0-9.+-eE]+)", line)
                        if m:
                            try:
                                cutting_planes = float(m.group(1))
                            except ValueError:
                                cutting_planes = None

                # Cleanup temporary file (but not original legacy files from test_instances/)
                if temp_legacy_file and os.path.exists(temp_legacy_file):
                    # Don't delete files from test_instances/ directory - they are the original source files
                    if not temp_legacy_file.startswith(str(self.data_dir / "test_instances")):
                        os.unlink(temp_legacy_file)

                best_known_obj = self.best_known_values.get(instance_name)
                is_optimal = None
                optimality_status = "UNKNOWN"
                if best_known_obj is not None and objective is not None:
                    if abs(objective - best_known_obj) < 1e-6:
                        is_optimal = True
                        optimality_status = "OPTIMAL_VERIFIED"
                    elif abs(objective - best_known_obj) < 0.1:
                        is_optimal = True
                        optimality_status = "OPTIMAL_NUMERICAL"
                    else:
                        is_optimal = False
                        optimality_status = "SUBOPTIMAL"

                return {
                    "instance_name": instance_name,
                    "success": True,
                    "objective": objective,
                    "best_known_objective": best_known_obj,
                    "is_optimal": is_optimal,
                    "optimality_status": optimality_status,
                    "optimization_status_description": "COMPLETED",
                    "runtime_stats": {
                        "wall_clock_seconds": runtime_seconds if runtime_seconds not in (None, 0.0) else time.time() - start_time,
                        "solver_nodes": nodes_explored,
                        "solver_iterations": None
                    },
                    "lazy_constraints": lazy_constraints,
                    "cutting_planes": cutting_planes,
                    "warm_starts_tried": None,
                    "warm_start_used": None,
                    "gap_percent": mip_gap,
                    "stdout": result.stdout,
                    "stderr": result.stderr,
                    "timestamp": datetime.now(timezone.utc).isoformat()
                }
            else:
                # Modern formulation produces JSON
                result_file = results_dir / f"{instance_name}.results.json"
                if result_file.exists():
                    with open(result_file, 'r') as f:
                        result_data = json.load(f)

                    # Extract and flatten the relevant fields from the structured JSON
                    objective = result_data.get("results", {}).get("objective")
                    best_known_obj = self.best_known_values.get(instance_name)

                    # Validate against best known optimal value
                    is_optimal = None
                    optimality_status = "UNKNOWN"
                    if best_known_obj is not None and objective is not None:
                        if abs(objective - best_known_obj) < 1e-6:  # Exact match
                            is_optimal = True
                            optimality_status = "OPTIMAL_VERIFIED"
                        elif abs(objective - best_known_obj) < 0.1:  # Numerical precision
                            is_optimal = True
                            optimality_status = "OPTIMAL_NUMERICAL"
                        else:
                            is_optimal = False
                            optimality_status = "SUBOPTIMAL"

                    runtime_stats = result_data.get("runtime", {})
                    solver_metadata = result_data.get("solver_metadata", {})
                    def _parse_num(val):
                        try:
                            return int(val)
                        except (ValueError, TypeError):
                            try:
                                return float(val)
                            except (ValueError, TypeError):
                                return None
                    lazy_constraints = _parse_num(solver_metadata.get("lazy_constraints"))
                    cutting_planes = _parse_num(solver_metadata.get("cutting_planes"))
                    warm_start_used = solver_metadata.get("warm_start_used")
                    warm_starts_tried = solver_metadata.get("warm_starts_tried")

                    return {
                        "instance_name": result_data.get("instance", {}).get("name", instance_name),
                        "success": True,
                        "objective": objective,
                        "best_known_objective": best_known_obj,
                        "is_optimal": is_optimal,
                        "optimality_status": optimality_status,
                        "optimization_status_description": result_data.get("optimization_status", {}).get("code", "UNKNOWN"),
                        "runtime_stats": {
                            "wall_clock_seconds": runtime_stats.get("wall_clock_seconds"),
                            "cpu_seconds": runtime_stats.get("cpu_seconds"),
                            "solver_nodes": runtime_stats.get("solver_nodes"),
                            "solver_iterations": runtime_stats.get("solver_iterations")
                        },
                        "gap_percent": result_data.get("results", {}).get("gap_percent"),
                        "gap": result_data.get("results", {}).get("gap"),
                        "best_solution_time": result_data.get("results", {}).get("best_solution_time"),
                        "lazy_constraints": lazy_constraints,
                        "cutting_planes": cutting_planes,
                        "warm_starts_tried": warm_starts_tried,
                        "warm_start_used": warm_start_used,
                        "status": result_data.get("optimization_status", {}).get("code", "UNKNOWN"),
                        "error": None,
                        "raw_json": result_data  # Keep full JSON for debugging
                    }
                else:
                    # Fallback if no JSON result
                    return {
                        "instance_name": instance_name,
                        "success": True,
                        "stdout": result.stdout,
                        "stderr": result.stderr,
                        "timestamp": datetime.now(timezone.utc).isoformat()
                    }

        except subprocess.TimeoutExpired:
            # Cleanup temporary file on timeout
            if temp_legacy_file and os.path.exists(temp_legacy_file):
                os.unlink(temp_legacy_file)
            raise RuntimeError(f"Timeout after 3600 seconds")
        except subprocess.CalledProcessError as e:
            # Cleanup temporary file on error
            if temp_legacy_file and os.path.exists(temp_legacy_file):
                os.unlink(temp_legacy_file)
            raise RuntimeError(f"Execution failed: {e.stderr}")

    def generate_summary(self, experiment_name: str, args: argparse.Namespace,
                        instances: List[Dict], results: List[Dict],
                        successful: int, failed: int,
                        config: Dict[str, Any]) -> Dict[str, Any]:
        """Generate comprehensive experiment summary."""

        total_runtime = sum(
            r.get('runtime_stats', {}).get('wall_clock_seconds', 0)
            for r in results if r.get('success', False)
        )

        total_nodes = sum(
            r.get('runtime_stats', {}).get('solver_nodes', 0) or 0
            for r in results if r.get('success', False)
        )
        total_iterations = sum(
            r.get('runtime_stats', {}).get('solver_iterations', 0) or 0
            for r in results if r.get('success', False)
        )
        total_best_solution_time = sum(
            r.get('results', {}).get('best_solution_time', 0)
            if 'results' in r else r.get('best_solution_time', 0) or 0
            for r in results if r.get('success', False)
        )
        warm_used_set = set(
            r.get('warm_start_used')
            for r in results
            if r.get('warm_start_used') not in (None, "")
        )
        warm_tried_set = set(
            r.get('warm_starts_tried')
            for r in results
            if r.get('warm_starts_tried') not in (None, "")
        )

        # Calculate optimality statistics
        optimal_count = 0
        verified_optimal_count = 0
        suboptimal_count = 0
        unknown_optimality_count = 0

        for r in results:
            if r.get('success', False):
                is_optimal = r.get('is_optimal')
                if is_optimal is True:
                    optimal_count += 1
                    if r.get('optimality_status') == 'OPTIMAL_VERIFIED':
                        verified_optimal_count += 1
                elif is_optimal is False:
                    suboptimal_count += 1
                else:
                    unknown_optimality_count += 1

        return {
            "experiment_info": {
                "experiment_id": experiment_name,
                "tag": args.tag,
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "seed": args.seed,
                "formulation": args.formulation,
                "command_line": " ".join(sys.argv)
            },
            "execution_details": {
                "total_instances": len(instances),
                "instances_by_tag": {args.tag: len(instances)},
                "instance_names": [i['name'] for i in instances]
            },
            "formulation_config": {
                args.formulation: config
            },
            "results_summary": {
                "total_instances": len(instances),
                "successful": successful,
                "failed": failed,
                "success_rate": successful / len(instances) if instances else 0,
                "total_runtime_seconds": total_runtime,
                "avg_runtime_per_instance": total_runtime / len(instances) if instances else 0,
                "total_solver_nodes": total_nodes,
                "avg_solver_nodes": total_nodes / successful if successful else 0,
                "total_solver_iterations": total_iterations,
                "avg_solver_iterations": total_iterations / successful if successful else 0,
                "total_best_solution_time": total_best_solution_time,
                "avg_best_solution_time": total_best_solution_time / successful if successful else 0,
                "warm_starts_used": sorted(warm_used_set),
                "warm_starts_tried": sorted(warm_tried_set),
                "optimality_stats": {
                    "optimal_solutions": optimal_count,
                    "verified_optimal": verified_optimal_count,
                    "suboptimal_solutions": suboptimal_count,
                    "unknown_optimality": unknown_optimality_count,
                    "optimality_rate": optimal_count / successful if successful > 0 else 0
                }
            },
            "individual_results": [
                {
                    "instance_name": r.get("instance_name", "unknown"),
                    "success": r.get("success", False),
                    "objective": r.get("objective"),
                    "best_known_objective": r.get("best_known_objective"),
                    "is_optimal": r.get("is_optimal"),
                    "optimality_status": r.get("optimality_status", "UNKNOWN"),
                    "runtime_seconds": r.get("runtime_stats", {}).get("wall_clock_seconds"),
                    "solver_nodes": r.get("runtime_stats", {}).get("solver_nodes"),
                    "solver_iterations": r.get("runtime_stats", {}).get("solver_iterations"),
                    "lazy_constraints": r.get("lazy_constraints"),
                    "cutting_planes": r.get("cutting_planes"),
                    "warm_starts_tried": r.get("warm_starts_tried"),
                    "warm_start_used": r.get("warm_start_used"),
                    "status": r.get("optimization_status_description", "unknown"),
                    "gap_percent": r.get("gap_percent"),
                    "error": r.get("error")
                }
                for r in results
            ]
        }

    def generate_formulation_summary(self, experiment_name: str, args: argparse.Namespace,
                                    formulation: str, instances: List[Dict], results: List[Dict],
                                    successful: int, failed: int,
                                    config: Dict[str, Any]) -> Dict[str, Any]:
        """Generate summary for a specific formulation."""

        total_runtime = sum(
            r.get('runtime_stats', {}).get('wall_clock_seconds', 0)
            for r in results if r.get('success', False)
        )

        total_nodes = sum(
            r.get('runtime_stats', {}).get('solver_nodes', 0) or 0
            for r in results if r.get('success', False)
        )
        total_iterations = sum(
            r.get('runtime_stats', {}).get('solver_iterations', 0) or 0
            for r in results if r.get('success', False)
        )
        total_best_solution_time = sum(
            r.get('results', {}).get('best_solution_time', 0)
            if 'results' in r else r.get('best_solution_time', 0) or 0
            for r in results if r.get('success', False)
        )

        # Calculate optimality statistics
        optimal_count = 0
        verified_optimal_count = 0
        suboptimal_count = 0
        unknown_optimality_count = 0

        for r in results:
            if r.get('success', False):
                is_optimal = r.get('is_optimal')
                if is_optimal is True:
                    optimal_count += 1
                    if r.get('optimality_status') == 'OPTIMAL_VERIFIED':
                        verified_optimal_count += 1
                elif is_optimal is False:
                    suboptimal_count += 1
                else:
                    unknown_optimality_count += 1

        return {
            "experiment_info": {
                "experiment_id": experiment_name,
                "tag": args.tag,
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "seed": args.seed,
                "formulation": formulation,
                "command_line": " ".join(sys.argv)
            },
            "execution_details": {
                "total_instances": len(instances),
                "instances_by_tag": {args.tag: len(instances)},
                "instance_names": [i['name'] for i in instances]
            },
            "formulation_config": {
                formulation: config
            },
            "results_summary": {
                "total_instances": len(instances),
                "successful": successful,
                "failed": failed,
                "success_rate": successful / len(instances) if instances else 0,
                "total_runtime_seconds": total_runtime,
                "avg_runtime_per_instance": total_runtime / len(instances) if instances else 0,
                "total_solver_nodes": total_nodes,
                "avg_solver_nodes": total_nodes / successful if successful else 0,
                "total_solver_iterations": total_iterations,
                "avg_solver_iterations": total_iterations / successful if successful else 0,
                "total_best_solution_time": total_best_solution_time,
                "avg_best_solution_time": total_best_solution_time / successful if successful else 0,
                "optimality_stats": {
                    "optimal_solutions": optimal_count,
                    "verified_optimal": verified_optimal_count,
                    "suboptimal_solutions": suboptimal_count,
                    "unknown_optimality": unknown_optimality_count,
                    "optimality_rate": optimal_count / successful if successful > 0 else 0
                }
            },
            "individual_results": [
                {
                    "instance_name": r.get("instance_name", "unknown"),
                    "success": r.get("success", False),
                    "objective": r.get("objective"),
                    "best_known_objective": r.get("best_known_objective"),
                    "is_optimal": r.get("is_optimal"),
                    "optimality_status": r.get("optimality_status", "UNKNOWN"),
                    "runtime_seconds": r.get("runtime_stats", {}).get("wall_clock_seconds"),
                    "solver_nodes": r.get("runtime_stats", {}).get("solver_nodes"),
                    "solver_iterations": r.get("runtime_stats", {}).get("solver_iterations"),
                    "lazy_constraints": r.get("lazy_constraints"),
                    "cutting_planes": r.get("cutting_planes"),
                    "warm_starts_tried": r.get("warm_starts_tried"),
                    "warm_start_used": r.get("warm_start_used"),
                    "status": r.get("optimization_status_description", "unknown"),
                    "gap_percent": r.get("gap_percent"),
                    "error": r.get("error")
                }
                for r in results
            ]
        }

    def generate_comparative_summary(self, experiment_name: str, args: argparse.Namespace,
                                    instances: List[Dict], all_results: Dict[str, List[Dict]],
                                    formulation_summaries: Dict[str, Dict]) -> Dict[str, Any]:
        """Generate comparative summary across all formulations."""

        formulations = list(all_results.keys())

        # Aggregate statistics across all formulations
        total_instances = len(instances)
        total_experiments = total_instances * len(formulations)

        comparative_results = []
        formulation_stats = {}

        for formulation in formulations:
            results = all_results[formulation]
            summary = formulation_summaries[formulation]

            successful = summary["results_summary"]["successful"]
            failed = summary["results_summary"]["failed"]
            total_runtime = summary["results_summary"]["total_runtime_seconds"]
            total_nodes = summary["results_summary"].get("total_solver_nodes", 0)
            total_iterations = summary["results_summary"].get("total_solver_iterations", 0)
            optimality_stats = summary["results_summary"]["optimality_stats"]
            warm_used = summary["results_summary"].get("warm_starts_used", [])
            warm_tried = summary["results_summary"].get("warm_starts_tried", [])

            formulation_stats[formulation] = {
                "successful": successful,
                "failed": failed,
                "success_rate": successful / total_instances if total_instances > 0 else 0,
                "total_runtime": total_runtime,
                "avg_runtime_per_instance": total_runtime / total_instances if total_instances > 0 else 0,
                "total_solver_nodes": total_nodes,
                "avg_solver_nodes": total_nodes / total_instances if total_instances > 0 else 0,
                "total_solver_iterations": total_iterations,
                "avg_solver_iterations": total_iterations / total_instances if total_instances > 0 else 0,
                "warm_starts_used": warm_used,
                "warm_starts_tried": warm_tried,
                "optimality_stats": optimality_stats
            }

            # Add comparative results for each instance
            for result in results:
                comparative_results.append({
                    "instance_name": result.get("instance_name", "unknown"),
                    "formulation": formulation,
                    "success": result.get("success", False),
                    "objective": result.get("objective"),
                    "best_known_objective": result.get("best_known_objective"),
                    "is_optimal": result.get("is_optimal"),
                    "optimality_status": result.get("optimality_status", "UNKNOWN"),
                    "runtime_seconds": result.get("runtime_stats", {}).get("wall_clock_seconds"),
                    "solver_nodes": result.get("runtime_stats", {}).get("solver_nodes"),
                    "solver_iterations": result.get("runtime_stats", {}).get("solver_iterations"),
                    "lazy_constraints": result.get("lazy_constraints"),
                    "cutting_planes": result.get("cutting_planes"),
                    "warm_starts_tried": result.get("warm_starts_tried"),
                    "warm_start_used": result.get("warm_start_used"),
                    "status": result.get("optimization_status_description", "unknown"),
                    "gap_percent": result.get("gap_percent"),
                    "error": result.get("error")
                })

        return {
            "experiment_info": {
                "experiment_id": experiment_name,
                "tag": args.tag,
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "seed": args.seed,
                "formulations": formulations,
                "command_line": " ".join(sys.argv)
            },
            "execution_details": {
                "total_instances": total_instances,
                "total_formulations": len(formulations),
                "total_experiments": total_experiments,
                "instances_by_tag": {args.tag: total_instances},
                "instance_names": [i['name'] for i in instances]
            },
            "formulation_config": {
                formulation: formulation_summaries[formulation]["formulation_config"][formulation]
                for formulation in formulations
            },
            "formulation_summaries": formulation_stats,
            "comparative_results": comparative_results
        }

    def print_experiment_summary(self, comparative_summary: Dict[str, Any], formulations: List[str]) -> None:
        """Print a comprehensive summary of the comparative experiment."""

        print("=" * 80)
        print("🎉 MULTI-FORMULATION EXPERIMENT COMPLETED")
        print("=" * 80)

        exec_details = comparative_summary["execution_details"]
        form_summaries = comparative_summary["formulation_summaries"]

        print(f"📊 Instances: {exec_details['total_instances']}")
        print(f"🔬 Formulations: {len(formulations)} ({', '.join(formulations)})")
        print(f"⚡ Total Experiments: {exec_details['total_experiments']}")
        print()

        # Summary table for formulations
        print("FORMULATION COMPARISON:")
        print("-" * 80)
        print(f"{'Formulation':<25} {'Success':<8} {'Optimal':<8} {'Runtime':<10} {'Rate':<6}")
        print("-" * 80)

        for formulation in formulations:
            stats = form_summaries[formulation]
            success_rate = f"{stats['success_rate']:.1%}"
            optimal = stats['optimality_stats']['optimal_solutions']
            total_success = stats['successful']
            optimality_rate = f"{stats['optimality_stats']['optimality_rate']:.1%}" if total_success > 0 else "N/A"
            avg_runtime = f"{stats['avg_runtime_per_instance']:.1f}s"

            print(f"{formulation:<25} {stats['successful']}/{exec_details['total_instances']:<8} {optimal}/{total_success:<8} {avg_runtime:<10} {optimality_rate:<6}")

        print()

        # Overall statistics
        total_successful = sum(stats['successful'] for stats in form_summaries.values())
        total_failed = sum(stats['failed'] for stats in form_summaries.values())
        total_optimal = sum(stats['optimality_stats']['optimal_solutions'] for stats in form_summaries.values())

        print("OVERALL STATISTICS:")
        print(f"✅ Total Successful: {total_successful}/{exec_details['total_experiments']}")
        print(f"❌ Total Failed: {total_failed}/{exec_details['total_experiments']}")
        print(f"🎯 Total Optimal: {total_optimal}/{total_successful} ({total_optimal/total_successful:.1%})" if total_successful > 0 else "🎯 Total Optimal: N/A")


def main():
    parser = argparse.ArgumentParser(
        description="OCST Experiment Orchestrator - Phase 1.5 Workstream 5",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Single formulation
  python orchestrator.py --tag quick_check --formulation path_based

  # Multiple formulations (comparison)
  python orchestrator.py --tag quick_check --formulation path_based --formulation flow_based
  python orchestrator.py --tag beasley --formulation path_based --formulation path_based_formulation_original --seed 123

  # With configuration overrides
  python orchestrator.py --tag quick_check --formulation path_based --config time_limit=1800,mip_gap=0.001
  python orchestrator.py --tag quick_check --formulation path_based --formulation flow_based --config-file config.json
        """
    )

    parser.add_argument(
        "--tag",
        required=True,
        help="Tag to filter instances (e.g., quick_check, beasley)"
    )

    parser.add_argument(
        "--formulation",
        required=True,
        action="append",
        choices=["path_based", "path_based_formulation_original", "flow_based", "flow_based_relaxed"],
        help="Formulation(s) to execute (can be used multiple times for comparison)"
    )

    parser.add_argument(
        "--seed",
        type=int,
        default=42,
        help="Random seed for reproducibility (default: 42)"
    )

    parser.add_argument(
        "--config",
        action="append",
        help="Configuration overrides as key=value pairs (can be used multiple times)"
    )

    parser.add_argument(
        "--config-file",
        type=str,
        help="JSON file with additional configuration"
    )

    args = parser.parse_args()

    try:
        orchestrator = OCSTOrchestrator()
        orchestrator.run_experiment(args)
    except KeyboardInterrupt:
        print("\n⚠️  Experiment interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n❌ Error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
