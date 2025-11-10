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
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Any, Optional
import glob


class OCSTOrchestrator:
    """Main orchestrator for OCST experiments."""

    def __init__(self):
        self.project_root = Path(__file__).parent.parent.parent
        self.data_dir = self.project_root / "data" / "input"
        self.experiments_dir = self.project_root / "experiments"
        self.build_dir = self.project_root / "build" / "executables"

        # Ensure directories exist
        self.experiments_dir.mkdir(exist_ok=True)

    def run_experiment(self, args: argparse.Namespace) -> None:
        """Run a complete experiment with the specified parameters."""

        # Create experiment directory structure
        timestamp = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S")
        experiment_name = f"{args.tag}_{timestamp}"
        experiment_dir = self.experiments_dir / experiment_name
        formulation_dir = experiment_dir / args.formulation
        results_dir = formulation_dir / "results"

        results_dir.mkdir(parents=True, exist_ok=True)

        print(f"🚀 Starting experiment: {experiment_name}")
        print(f"📁 Output directory: {experiment_dir}")
        print(f"🏷️  Tag: {args.tag}")
        print(f"🔬 Formulation: {args.formulation}")
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

        # Compile formulation
        print(f"🔨 Compiling formulation: {args.formulation}")
        self.compile_formulation(args.formulation)

        # Prepare configuration
        config = self.prepare_config(args)

        # Execute experiments
        results = []
        successful = 0
        failed = 0

        print(f"⚡ Executing {len(instances)} experiments...")
        print("=" * 60)

        for i, instance in enumerate(instances, 1):
            instance_name = instance['name']
            print(f"[{i:2d}/{len(instances)}] Processing: {instance_name}")

            try:
                result = self.run_single_instance(
                    args.formulation,
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
                    "error": str(e),
                    "timestamp": datetime.now(timezone.utc).isoformat()
                }
                results.append(error_result)
                failed += 1
                status = "✗"
                print(f"  Error: {e}")

            print(f"  {status} {instance_name}")
            print()

        # Generate summary
        summary = self.generate_summary(
            experiment_name, args, instances, results,
            successful, failed, config
        )

        # Save summary
        summary_file = experiment_dir / "experiment_summary.json"
        with open(summary_file, 'w') as f:
            json.dump(summary, f, indent=2)

        # Save detailed results
        results_file = results_dir / "detailed_results.json"
        with open(results_file, 'w') as f:
            json.dump(results, f, indent=2)

        print("=" * 60)
        print("🎉 EXPERIMENT COMPLETED")
        print(f"✅ Successful: {successful}")
        print(f"❌ Failed: {failed}")
        print(f"📄 Summary: {summary_file}")
        print(f"📄 Results: {results_file}")

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
        try:
            result = subprocess.run(
                ["make", formulation],
                cwd=self.project_root,
                capture_output=True,
                text=True,
                check=True
            )
            print("  ✓ Compilation successful")
            print(f"  Output: {result.stdout.strip()}")
        except subprocess.CalledProcessError as e:
            raise RuntimeError(f"Compilation failed: {e.stderr}")

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

        executable = self.build_dir / formulation
        if not executable.exists():
            raise FileNotFoundError(f"Executable not found: {executable}")

        # Prepare command arguments
        cmd = [
            str(executable),
            f"--instance={instance_name}",
            f"--seed={seed}",
            f"--output-dir={results_dir}"
        ]

        # Add config overrides
        for key, value in config.items():
            cmd.append(f"--{key}={value}")

        # Execute
        try:
            result = subprocess.run(
                cmd,
                cwd=self.project_root,
                capture_output=True,
                text=True,
                timeout=3600,  # 1 hour timeout
                check=True
            )

            # Try to load the result JSON
            result_file = results_dir / f"{instance_name}.results.json"
            if result_file.exists():
                with open(result_file, 'r') as f:
                    result_data = json.load(f)
                    result_data['success'] = True
                    return result_data
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
            raise RuntimeError(f"Timeout after 3600 seconds")
        except subprocess.CalledProcessError as e:
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
                "avg_runtime_per_instance": total_runtime / len(instances) if instances else 0
            },
            "individual_results": [
                {
                    "instance_name": r.get("instance_name", "unknown"),
                    "success": r.get("success", False),
                    "objective": r.get("objective"),
                    "runtime_seconds": r.get("runtime_stats", {}).get("wall_clock_seconds"),
                    "status": r.get("optimization_status_description", "unknown"),
                    "gap_percent": r.get("gap_percent"),
                    "error": r.get("error")
                }
                for r in results
            ]
        }


def main():
    parser = argparse.ArgumentParser(
        description="OCST Experiment Orchestrator - Phase 1.5 Workstream 5",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python orchestrator.py --tag quick_check --formulation path_based
  python orchestrator.py --tag beasley --formulation flow_based --seed 123
  python orchestrator.py --tag quick_check --formulation path_based --config time_limit=1800,mip_gap=0.001
  python orchestrator.py --tag quick_check --formulation path_based --config-file config.json
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
        choices=["path_based", "path_based_formulation_original", "flow_based", "flow_based_relaxed"],
        help="Formulation to execute"
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
