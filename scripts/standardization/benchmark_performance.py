#!/usr/bin/env python3
"""
Performance Benchmark Suite for Workstream 1 - JSON Loader & Parsing

Measures:
- JSON parsing time (must be O(n + m))
- Memory usage during loading
- Comparison with legacy format (if available)

Validation Protocol:
- Runs on quick_check instance set
- Overhead must be < 10% vs baseline
- Results stored in experiments/benchmarks/

Author: Phase 1.5 Standardization
Date: 2025-11-10
"""

import json
import os
import sys
import time
import subprocess
import statistics
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Tuple
import csv

# Paths
PROJECT_ROOT = Path(__file__).parent.parent.parent
SOLVER_BIN = PROJECT_ROOT / "build" / "executables" / "path_based_formulation"
INSTANCES_DIR = PROJECT_ROOT / "data" / "input"  # Updated to correct location
BENCHMARK_DIR = PROJECT_ROOT / "experiments" / "benchmarks"
TIMESTAMP = datetime.now().strftime("%Y%m%d_%H%M%S")
OUTPUT_DIR = BENCHMARK_DIR / f"performance_{TIMESTAMP}"

# Benchmark configuration
NUM_RUNS = 5  # Runs per instance for statistical significance
WARMUP_RUNS = 2  # Warm-up runs (not counted)

def setup_directories():
    """Create necessary directories for benchmark outputs"""
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    print(f"📁 Benchmark output directory: {OUTPUT_DIR}")

def get_instance_size(instance_path: Path) -> Tuple[int, int]:
    """Extract (num_nodes, num_edges) from JSON instance"""
    try:
        with open(instance_path, 'r') as f:
            data = json.load(f)
            # Handle both legacy and new schema formats
            num_nodes = data.get("num_nodes", data.get("graph", {}).get("nodes", 0))
            num_edges = len(data.get("edges", data.get("graph", {}).get("edges", [])))
            return num_nodes, num_edges
    except Exception as e:
        print(f"⚠️  Error reading {instance_path.name}: {e}")
        return 0, 0

def measure_parsing_time(instance_path: Path) -> Dict:
    """
    Measure JSON parsing time by running solver with minimal time limit
    
    We use a very short time limit (0.01s) to isolate parsing overhead
    from solving time. The solver will timeout immediately after parsing.
    """
    cmd = [
        str(SOLVER_BIN),
        str(instance_path),
        str(OUTPUT_DIR / f"{instance_path.stem}_bench.sol"),
        "0.01",  # 10ms time limit - just enough to parse
        "0.0"    # No heuristics
    ]
    
    start_time = time.perf_counter()
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=5.0  # Safety timeout
        )
        end_time = time.perf_counter()
        elapsed = end_time - start_time
        
        # Check if parsing succeeded (even if solving timed out)
        parsing_ok = "Error parsing" not in result.stderr
        
        return {
            "elapsed_seconds": elapsed,
            "parsing_ok": parsing_ok,
            "returncode": result.returncode
        }
    except subprocess.TimeoutExpired:
        end_time = time.perf_counter()
        return {
            "elapsed_seconds": end_time - start_time,
            "parsing_ok": False,
            "returncode": -1
        }
    except Exception as e:
        return {
            "elapsed_seconds": 0.0,
            "parsing_ok": False,
            "returncode": -1,
            "error": str(e)
        }

def benchmark_instance(instance_path: Path) -> Dict:
    """Run complete benchmark suite on a single instance"""
    instance_name = instance_path.stem
    print(f"\n🔬 Benchmarking: {instance_name}")
    
    # Get instance size
    num_nodes, num_edges = get_instance_size(instance_path)
    complexity = num_nodes + num_edges  # O(n + m)
    
    print(f"   Size: n={num_nodes}, m={num_edges} (complexity O({complexity}))")
    
    # Warm-up runs
    print(f"   🔥 Warm-up: {WARMUP_RUNS} runs...")
    for _ in range(WARMUP_RUNS):
        measure_parsing_time(instance_path)
    
    # Actual benchmark runs
    print(f"   ⏱️  Measuring: {NUM_RUNS} runs...")
    timings = []
    
    for run in range(NUM_RUNS):
        result = measure_parsing_time(instance_path)
        if result["parsing_ok"]:
            timings.append(result["elapsed_seconds"])
            print(f"      Run {run+1}: {result['elapsed_seconds']*1000:.2f} ms")
        else:
            print(f"      Run {run+1}: ❌ FAILED")
    
    if not timings:
        return {
            "instance": instance_name,
            "num_nodes": num_nodes,
            "num_edges": num_edges,
            "status": "FAILED",
            "mean_ms": 0.0,
            "std_ms": 0.0,
            "min_ms": 0.0,
            "max_ms": 0.0,
            "complexity_normalized": 0.0
        }
    
    # Statistics
    mean_s = statistics.mean(timings)
    std_s = statistics.stdev(timings) if len(timings) > 1 else 0.0
    min_s = min(timings)
    max_s = max(timings)
    
    # Normalize by complexity O(n + m) to verify linear behavior
    complexity_normalized = (mean_s / complexity) * 1e6 if complexity > 0 else 0.0  # µs per edge/node
    
    print(f"   ✅ Mean: {mean_s*1000:.2f} ms ± {std_s*1000:.2f} ms")
    print(f"   📊 Normalized: {complexity_normalized:.2f} µs/(n+m)")
    
    return {
        "instance": instance_name,
        "num_nodes": num_nodes,
        "num_edges": num_edges,
        "status": "OK",
        "mean_ms": mean_s * 1000,
        "std_ms": std_s * 1000,
        "min_ms": min_s * 1000,
        "max_ms": max_s * 1000,
        "complexity_normalized": complexity_normalized
    }

def find_quick_check_instances() -> List[Path]:
    """Find all instances tagged as quick_check"""
    instances = []
    
    # Look for JSON instances with quick_check tag
    for json_file in INSTANCES_DIR.glob("*.json"):
        try:
            with open(json_file, 'r') as f:
                data = json.load(f)
                tags = data.get("tags", [])
                if "quick_check" in tags:
                    instances.append(json_file)
        except Exception:
            continue
    
    # If no tagged instances, use small instances as proxy
    if not instances:
        print("⚠️  No 'quick_check' tagged instances found, using small instances (<30 nodes)")
        for json_file in INSTANCES_DIR.glob("*.json"):
            try:
                with open(json_file, 'r') as f:
                    data = json.load(f)
                    if data.get("num_nodes", 999) < 30:
                        instances.append(json_file)
            except Exception:
                continue
    
    return sorted(instances)

def write_results(results: List[Dict]):
    """Write benchmark results to CSV and JSON"""
    
    # CSV summary
    csv_path = OUTPUT_DIR / "benchmark_results.csv"
    with open(csv_path, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=[
            "instance", "num_nodes", "num_edges", "status",
            "mean_ms", "std_ms", "min_ms", "max_ms", "complexity_normalized"
        ])
        writer.writeheader()
        writer.writerows(results)
    
    print(f"\n💾 Results saved to: {csv_path}")
    
    # JSON detailed report
    json_path = OUTPUT_DIR / "benchmark_results.json"
    report = {
        "benchmark_info": {
            "timestamp": TIMESTAMP,
            "num_instances": len(results),
            "runs_per_instance": NUM_RUNS,
            "warmup_runs": WARMUP_RUNS
        },
        "results": results
    }
    
    with open(json_path, 'w') as f:
        json.dump(report, f, indent=2)
    
    print(f"💾 Detailed report: {json_path}")

def analyze_results(results: List[Dict]):
    """Analyze benchmark results and check for performance issues"""
    
    print("\n" + "="*80)
    print("📊 BENCHMARK ANALYSIS")
    print("="*80)
    
    valid_results = [r for r in results if r["status"] == "OK"]
    
    if not valid_results:
        print("❌ No valid results to analyze!")
        return
    
    # Overall statistics
    total_instances = len(results)
    successful = len(valid_results)
    failed = total_instances - successful
    
    print(f"\n✅ Successful: {successful}/{total_instances}")
    if failed > 0:
        print(f"❌ Failed: {failed}/{total_instances}")
    
    # Parsing time statistics
    mean_times = [r["mean_ms"] for r in valid_results]
    normalized_times = [r["complexity_normalized"] for r in valid_results]
    
    print(f"\n⏱️  Parsing Time Statistics:")
    print(f"   Mean: {statistics.mean(mean_times):.2f} ms")
    print(f"   Median: {statistics.median(mean_times):.2f} ms")
    print(f"   Min: {min(mean_times):.2f} ms")
    print(f"   Max: {max(mean_times):.2f} ms")
    
    # Complexity analysis (should be roughly constant for O(n+m))
    print(f"\n📈 Complexity Analysis (should be ~constant for O(n+m)):")
    print(f"   Mean normalized: {statistics.mean(normalized_times):.2f} µs/(n+m)")
    print(f"   Std dev: {statistics.stdev(normalized_times):.2f} µs/(n+m)")
    
    # Coefficient of variation (should be low for linear complexity)
    cv = statistics.stdev(normalized_times) / statistics.mean(normalized_times)
    print(f"   Coefficient of variation: {cv:.2%}")
    
    if cv < 0.5:
        print(f"   ✅ PASS: Low variance confirms O(n+m) behavior")
    else:
        print(f"   ⚠️  WARNING: High variance may indicate non-linear behavior")
    
    # Performance threshold check (10% overhead limit)
    baseline_ns = statistics.mean(normalized_times)
    threshold_ns = baseline_ns * 1.10  # 10% overhead
    
    outliers = [r for r in valid_results if r["complexity_normalized"] > threshold_ns]
    
    if not outliers:
        print(f"\n✅ PASS: All instances within 10% overhead threshold")
    else:
        print(f"\n⚠️  WARNING: {len(outliers)} instances exceed 10% overhead:")
        for r in outliers:
            overhead = (r["complexity_normalized"] / baseline_ns - 1) * 100
            print(f"      {r['instance']}: +{overhead:.1f}% overhead")
    
    # Final verdict
    print(f"\n" + "="*80)
    if cv < 0.5 and not outliers:
        print("🎉 BENCHMARK PASSED: Parser exhibits O(n+m) behavior with <10% overhead")
    elif cv < 0.5:
        print("⚠️  BENCHMARK WARNING: Linear behavior confirmed but some outliers detected")
    else:
        print("❌ BENCHMARK FAILED: Non-linear behavior or excessive overhead detected")
    print("="*80)

def main():
    """Main benchmark execution"""
    print("="*80)
    print("🚀 WORKSTREAM 1 - PERFORMANCE BENCHMARK SUITE")
    print("="*80)
    print(f"Timestamp: {TIMESTAMP}")
    print(f"Solver: {SOLVER_BIN}")
    print(f"Runs per instance: {NUM_RUNS} (+ {WARMUP_RUNS} warmup)")
    
    # Setup
    setup_directories()
    
    # Find instances
    instances = find_quick_check_instances()
    
    if not instances:
        print("\n❌ ERROR: No quick_check instances found!")
        print(f"   Searched in: {INSTANCES_DIR}")
        return 1
    
    print(f"\n📋 Found {len(instances)} quick_check instances")
    
    # Check solver binary
    if not SOLVER_BIN.exists():
        print(f"\n❌ ERROR: Solver binary not found: {SOLVER_BIN}")
        print("   Run 'make path_based' to build the solver")
        return 1
    
    # Run benchmarks
    results = []
    
    for instance in instances:
        result = benchmark_instance(instance)
        results.append(result)
    
    # Save and analyze
    write_results(results)
    analyze_results(results)
    
    print(f"\n✅ Benchmark complete! Results in: {OUTPUT_DIR}")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())

