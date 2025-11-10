#!/usr/bin/env python3
"""
Benchmark script to measure overhead of config and logging system.

Compares performance with and without structured logging enabled.
Target: Overhead < 7% for the complete config + logging system.
"""

import subprocess
import json
import time
import statistics
import os
import sys
from pathlib import Path
from typing import List, Dict, Tuple

def run_benchmark(executable: str, input_file: str, config_file: str = None,
                  enable_logging: bool = False, runs: int = 5) -> List[float]:
    """Run benchmark and return list of execution times."""
    times = []

    for i in range(runs):
        # Warmup run (not counted)
        if i == 0:
            cmd = [executable, input_file]
            if config_file:
                cmd.extend(['--config', config_file])
            subprocess.run(cmd, capture_output=True, timeout=300)

        # Actual benchmark run
        start_time = time.time()
        cmd = [executable, input_file]
        if config_file:
            cmd.extend(['--config', config_file])
        if enable_logging:
            cmd.append('--enable-logging')

        try:
            result = subprocess.run(cmd, capture_output=True, timeout=300, text=True)
            if result.returncode == 0:
                end_time = time.time()
                execution_time = end_time - start_time
                times.append(execution_time)
                print(".3f")
            else:
                print(f"Run {i+1} failed with return code {result.returncode}")
                print(f"stderr: {result.stderr}")
        except subprocess.TimeoutExpired:
            print(f"Run {i+1} timed out")
        except Exception as e:
            print(f"Run {i+1} failed with exception: {e}")

    return times

def analyze_results(baseline_times: List[float], with_overhead_times: List[float]) -> Dict:
    """Analyze benchmark results and compute overhead statistics."""
    if not baseline_times or not with_overhead_times:
        return {"error": "Insufficient data for analysis"}

    baseline_mean = statistics.mean(baseline_times)
    baseline_std = statistics.stdev(baseline_times) if len(baseline_times) > 1 else 0

    overhead_mean = statistics.mean(with_overhead_times)
    overhead_std = statistics.stdev(with_overhead_times) if len(with_overhead_times) > 1 else 0

    overhead_percentage = ((overhead_mean - baseline_mean) / baseline_mean) * 100

    return {
        "baseline": {
            "mean": baseline_mean,
            "std": baseline_std,
            "runs": len(baseline_times)
        },
        "with_overhead": {
            "mean": overhead_mean,
            "std": overhead_std,
            "runs": len(with_overhead_times)
        },
        "overhead_percentage": overhead_percentage,
        "target_met": abs(overhead_percentage) < 7.0  # Allow small negative overhead
    }

def create_test_config() -> str:
    """Create a test configuration file."""
    config = {
        "time_limit": 300.0,
        "tolerance": 1e-6,
        "threads": 1,
        "presolve": True,
        "output_flag": False,
        "branching_strategy": "strong"
    }

    config_file = "/tmp/ocst_benchmark_config.json"
    with open(config_file, 'w') as f:
        json.dump(config, f, indent=2)

    return config_file

def main():
    if len(sys.argv) < 3:
        print("Usage: python benchmark_config_overhead.py <executable> <input_file>")
        sys.exit(1)

    executable = sys.argv[1]
    input_file = sys.argv[2]

    if not os.path.exists(executable):
        print(f"Error: Executable not found: {executable}")
        sys.exit(1)

    if not os.path.exists(input_file):
        print(f"Error: Input file not found: {input_file}")
        sys.exit(1)

    # Create test config
    config_file = create_test_config()
    print(f"Created test config: {config_file}")

    print("=== Config & Logging Overhead Benchmark ===")
    print(f"Executable: {executable}")
    print(f"Input: {input_file}")
    print()

    # Benchmark 1: Baseline (no config, no logging)
    print("Running baseline benchmark (no config, no logging)...")
    baseline_times = run_benchmark(executable, input_file, runs=3)
    print()

    # Benchmark 2: With config but no logging
    print("Running config benchmark (with config, no logging)...")
    config_times = run_benchmark(executable, input_file, config_file=config_file,
                                enable_logging=False, runs=3)
    print()

    # Benchmark 3: With config and logging
    print("Running full benchmark (with config + logging)...")
    full_times = run_benchmark(executable, input_file, config_file=config_file,
                              enable_logging=True, runs=3)
    print()

    # Analyze results
    print("=== Results Analysis ===")

    if baseline_times and config_times:
        config_analysis = analyze_results(baseline_times, config_times)
        config_overhead = config_analysis["overhead_percentage"]
        print(".3f"
              ".3f")

        config_target_met = config_analysis["target_met"]
        if config_target_met:
            print("✅ Config overhead: WITHIN TARGET (< 7%)")
        else:
            print("❌ Config overhead: EXCEEDS TARGET (>= 7%)")

    if config_times and full_times:
        logging_analysis = analyze_results(config_times, full_times)
        logging_overhead = logging_analysis["overhead_percentage"]
        print(".3f"
              ".3f")

        logging_target_met = logging_analysis["target_met"]
        if logging_target_met:
            print("✅ Logging overhead: WITHIN TARGET (< 7%)")
        else:
            print("❌ Logging overhead: EXCEEDS TARGET (>= 7%)")

    if baseline_times and full_times:
        total_analysis = analyze_results(baseline_times, full_times)
        total_overhead = total_analysis["overhead_percentage"]
        print(".3f"
              ".3f")

        total_target_met = total_analysis["target_met"]
        if total_target_met:
            print("✅ TOTAL overhead: WITHIN TARGET (< 7%)")
        else:
            print("❌ TOTAL overhead: EXCEEDS TARGET (>= 7%)")

    # Save detailed results
    results = {
        "timestamp": time.time(),
        "baseline_times": baseline_times,
        "config_times": config_times,
        "full_times": full_times,
        "config_analysis": config_analysis if 'config_analysis' in locals() else None,
        "logging_analysis": logging_analysis if 'logging_analysis' in locals() else None,
        "total_analysis": total_analysis if 'total_analysis' in locals() else None
    }

    results_file = "experiments/benchmarks/config_overhead_results.json"
    os.makedirs(os.path.dirname(results_file), exist_ok=True)

    with open(results_file, 'w') as f:
        json.dump(results, f, indent=2)

    print(f"\nDetailed results saved to: {results_file}")

    # Cleanup
    if os.path.exists(config_file):
        os.remove(config_file)

if __name__ == "__main__":
    main()
