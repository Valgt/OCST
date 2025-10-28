#!/usr/bin/env python3
"""
OCST Experiment Analysis Tool

Single comprehensive script for analyzing comparative experiment results.
Takes a CSV file as input and generates detailed statistical analysis.

Usage:
    python analyze_experiment.py <csv_file>
    python analyze_experiment.py experiments/results/experimento_comparative_formulations_20251027_232256.csv
"""

import csv
import sys
from pathlib import Path
from typing import Dict, List, Any
from collections import defaultdict
from dataclasses import dataclass, field
from datetime import datetime


@dataclass
class InstanceResult:
    """Single instance execution result."""
    instance_name: str
    algorithm: str
    n: int
    m: int
    requirements: int
    probability: float
    objective: float
    nodes: int
    runtime: float
    gap: float
    lazy_constraints: int
    cutting_planes: int
    relaxation: float
    lower_bound: float
    upper_bound: float
    is_optimal: bool
    status: str


@dataclass
class AlgorithmStats:
    """Statistical summary for an algorithm."""
    name: str
    total_instances: int = 0
    optimal_count: int = 0
    optimal_percentage: float = 0.0
    
    # Runtime statistics
    avg_runtime: float = 0.0
    median_runtime: float = 0.0
    min_runtime: float = float('inf')
    max_runtime: float = 0.0
    total_runtime: float = 0.0
    
    # Node statistics
    avg_nodes: float = 0.0
    median_nodes: float = 0.0
    min_nodes: int = float('inf')
    max_nodes: int = 0
    total_nodes: int = 0
    
    # Cut statistics
    avg_lazy: float = 0.0
    avg_cuts: float = 0.0
    total_lazy: int = 0
    total_cuts: int = 0
    
    # Gap statistics
    avg_gap: float = 0.0
    max_gap: float = 0.0
    
    # Raw data for median calculation
    runtimes: List[float] = field(default_factory=list)
    nodes_list: List[int] = field(default_factory=list)


class ExperimentAnalyzer:
    """Analyzer for OCST comparative experiments."""
    
    def __init__(self, csv_path: str):
        self.csv_path = Path(csv_path)
        self.results: List[InstanceResult] = []
        self.stats_by_algorithm: Dict[str, AlgorithmStats] = {}
        self.results_by_instance: Dict[str, Dict[str, InstanceResult]] = defaultdict(dict)
        
    def load_csv(self):
        """Load and parse CSV file."""
        if not self.csv_path.exists():
            raise FileNotFoundError(f"CSV file not found: {self.csv_path}")
        
        with open(self.csv_path, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                gap = float(row['mip_gap_percent'])
                result = InstanceResult(
                    instance_name=row['instance_name'],
                    algorithm=row['algorithm_name'],
                    n=int(row['num_nodes']),
                    m=int(row['num_edges']),
                    requirements=int(row['num_requirements']),
                    probability=float(row['probability']),
                    objective=float(row['best_found']),
                    nodes=int(row['nodes_explored']),
                    runtime=float(row['runtime_seconds']),
                    gap=gap,
                    lazy_constraints=int(row['lazy_constraints']),
                    cutting_planes=int(row['cutting_planes']),
                    relaxation=-1.0,  # Not in this CSV format
                    lower_bound=-1.0,  # Not in this CSV format
                    upper_bound=float(row['best_found']),
                    is_optimal=(gap < 0.01),
                    status="OPTIMAL" if gap < 0.01 else "NON-OPTIMAL"
                )
                self.results.append(result)
                self.results_by_instance[result.instance_name][result.algorithm] = result
    
    def compute_statistics(self):
        """Compute statistical summaries for each algorithm."""
        # Group results by algorithm
        by_algorithm = defaultdict(list)
        for result in self.results:
            by_algorithm[result.algorithm].append(result)
        
        # Compute stats for each algorithm
        for algo_name, results in by_algorithm.items():
            stats = AlgorithmStats(name=algo_name)
            stats.total_instances = len(results)
            stats.optimal_count = sum(1 for r in results if r.is_optimal)
            stats.optimal_percentage = (stats.optimal_count / stats.total_instances * 100) if stats.total_instances > 0 else 0
            
            # Runtime stats
            stats.runtimes = [r.runtime for r in results]
            stats.avg_runtime = sum(stats.runtimes) / len(stats.runtimes)
            stats.total_runtime = sum(stats.runtimes)
            stats.min_runtime = min(stats.runtimes)
            stats.max_runtime = max(stats.runtimes)
            stats.median_runtime = self._median(stats.runtimes)
            
            # Node stats
            stats.nodes_list = [r.nodes for r in results]
            stats.avg_nodes = sum(stats.nodes_list) / len(stats.nodes_list)
            stats.total_nodes = sum(stats.nodes_list)
            stats.min_nodes = min(stats.nodes_list)
            stats.max_nodes = max(stats.nodes_list)
            stats.median_nodes = self._median(stats.nodes_list)
            
            # Cut stats
            stats.total_lazy = sum(r.lazy_constraints for r in results)
            stats.total_cuts = sum(r.cutting_planes for r in results)
            stats.avg_lazy = stats.total_lazy / len(results)
            stats.avg_cuts = stats.total_cuts / len(results)
            
            # Gap stats
            gaps = [r.gap for r in results]
            stats.avg_gap = sum(gaps) / len(gaps)
            stats.max_gap = max(gaps)
            
            self.stats_by_algorithm[algo_name] = stats
    
    def _median(self, values: List[float]) -> float:
        """Calculate median of a list."""
        sorted_vals = sorted(values)
        n = len(sorted_vals)
        if n == 0:
            return 0.0
        if n % 2 == 0:
            return (sorted_vals[n//2 - 1] + sorted_vals[n//2]) / 2
        else:
            return sorted_vals[n//2]
    
    def check_consistency(self) -> List[str]:
        """Check if all algorithms found the same optimal values."""
        issues = []
        
        for instance_name, algo_results in self.results_by_instance.items():
            objectives = {algo: result.objective for algo, result in algo_results.items() if result.objective > 0}
            
            if len(objectives) > 0:
                unique_objectives = set(objectives.values())
                if len(unique_objectives) > 1:
                    issues.append(f"{instance_name}: Different objectives found!")
                    for algo, obj in objectives.items():
                        issues.append(f"  - {algo}: {obj:.0f}")
        
        return issues
    
    def find_fastest_algorithm(self) -> Dict[str, Dict[str, Any]]:
        """Find which algorithm is fastest for each instance."""
        fastest_per_instance = {}
        
        for instance_name, algo_results in self.results_by_instance.items():
            if not algo_results:
                continue
            
            fastest_algo = min(algo_results.items(), key=lambda x: x[1].runtime)
            runtimes = {algo: result.runtime for algo, result in algo_results.items()}
            
            fastest_per_instance[instance_name] = {
                'fastest': fastest_algo[0],
                'runtime': fastest_algo[1].runtime,
                'runtimes': runtimes
            }
        
        return fastest_per_instance
    
    def generate_report(self) -> str:
        """Generate comprehensive analysis report."""
        lines = []
        lines.append("\n" + "╔" + "═"*78 + "╗")
        lines.append("║" + " OCST COMPARATIVE EXPERIMENT ANALYSIS ".center(78) + "║")
        lines.append("╚" + "═"*78 + "╝\n")
        
        # Experiment info
        lines.append("📊 EXPERIMENT INFORMATION")
        lines.append("="*80)
        lines.append(f"CSV File: {self.csv_path.name}")
        lines.append(f"Total Executions: {len(self.results)}")
        lines.append(f"Unique Instances: {len(self.results_by_instance)}")
        lines.append(f"Algorithms: {', '.join(sorted(self.stats_by_algorithm.keys()))}")
        
        # Overall statistics by algorithm
        lines.append("\n\n📈 OVERALL STATISTICS BY ALGORITHM")
        lines.append("="*80)
        
        for algo_name in sorted(self.stats_by_algorithm.keys()):
            stats = self.stats_by_algorithm[algo_name]
            lines.append(f"\n{algo_name}:")
            lines.append("─"*80)
            lines.append(f"  Instances:        {stats.total_instances}")
            lines.append(f"  Optimal:          {stats.optimal_count}/{stats.total_instances} ({stats.optimal_percentage:.1f}%)")
            lines.append(f"  Avg Runtime:      {stats.avg_runtime:.3f}s")
            lines.append(f"  Median Runtime:   {stats.median_runtime:.3f}s")
            lines.append(f"  Min Runtime:      {stats.min_runtime:.3f}s")
            lines.append(f"  Max Runtime:      {stats.max_runtime:.3f}s")
            lines.append(f"  Total Runtime:    {stats.total_runtime:.3f}s")
            lines.append(f"  Avg Nodes:        {stats.avg_nodes:.1f}")
            lines.append(f"  Median Nodes:     {stats.median_nodes:.1f}")
            lines.append(f"  Max Nodes:        {stats.max_nodes}")
            lines.append(f"  Avg Lazy Cuts:    {stats.avg_lazy:.1f}")
            lines.append(f"  Avg Frac Cuts:    {stats.avg_cuts:.1f}")
            lines.append(f"  Avg Gap:          {stats.avg_gap:.4f}%")
            lines.append(f"  Max Gap:          {stats.max_gap:.4f}%")
        
        # Comparative table
        lines.append("\n\n📊 COMPARATIVE SUMMARY TABLE")
        lines.append("="*80)
        lines.append("┌────────────────────────────┬──────────┬──────────┬──────────┬──────────┐")
        lines.append("│ Metric                     │ FlowRlxd │ FlowBase │ PathBase │   Best   │")
        lines.append("├────────────────────────────┼──────────┼──────────┼──────────┼──────────┤")
        
        # Get stats in order
        fbr_stats = self.stats_by_algorithm.get('flow_based_relaxed_formulation')
        fb_stats = self.stats_by_algorithm.get('flow_based_formulation')
        pb_stats = self.stats_by_algorithm.get('path_based_formulation')
        
        if fbr_stats and fb_stats and pb_stats:
            # Runtime
            best_runtime = min(fbr_stats.avg_runtime, fb_stats.avg_runtime, pb_stats.avg_runtime)
            best_rt = "FBR" if best_runtime == fbr_stats.avg_runtime else ("FB" if best_runtime == fb_stats.avg_runtime else "PB")
            lines.append(f"│ Avg Runtime (s)            │ {fbr_stats.avg_runtime:>8.3f} │ {fb_stats.avg_runtime:>8.3f} │ {pb_stats.avg_runtime:>8.3f} │ {best_rt:>8} │")
            
            # Nodes
            best_nodes = min(fbr_stats.avg_nodes, fb_stats.avg_nodes, pb_stats.avg_nodes)
            best_nd = "FBR" if best_nodes == fbr_stats.avg_nodes else ("FB" if best_nodes == fb_stats.avg_nodes else "PB")
            lines.append(f"│ Avg Nodes Explored         │ {fbr_stats.avg_nodes:>8.1f} │ {fb_stats.avg_nodes:>8.1f} │ {pb_stats.avg_nodes:>8.1f} │ {best_nd:>8} │")
            
            # Lazy cuts
            lines.append(f"│ Avg Lazy Constraints       │ {fbr_stats.avg_lazy:>8.1f} │ {fb_stats.avg_lazy:>8.1f} │ {pb_stats.avg_lazy:>8.1f} │    -     │")
            
            # Fractional cuts
            lines.append(f"│ Avg Fractional Cuts        │ {fbr_stats.avg_cuts:>8.1f} │ {fb_stats.avg_cuts:>8.1f} │ {pb_stats.avg_cuts:>8.1f} │    -     │")
            
            # Optimal percentage
            lines.append(f"│ Optimal Solutions (%)      │ {fbr_stats.optimal_percentage:>8.1f} │ {fb_stats.optimal_percentage:>8.1f} │ {pb_stats.optimal_percentage:>8.1f} │    -     │")
        
        lines.append("└────────────────────────────┴──────────┴──────────┴──────────┴──────────┘")
        
        # Consistency check
        lines.append("\n\n✅ CONSISTENCY CHECK")
        lines.append("="*80)
        issues = self.check_consistency()
        if not issues:
            lines.append("✓ All algorithms found the same optimal values for all instances!")
        else:
            lines.append("⚠️  INCONSISTENCIES DETECTED:")
            for issue in issues:
                lines.append(f"  {issue}")
        
        # Fastest algorithm per instance
        lines.append("\n\n🏆 FASTEST ALGORITHM PER INSTANCE")
        lines.append("="*80)
        fastest = self.find_fastest_algorithm()
        
        # Count wins
        wins = defaultdict(int)
        for data in fastest.values():
            wins[data['fastest']] += 1
        
        lines.append("\nWins per algorithm:")
        for algo, count in sorted(wins.items(), key=lambda x: -x[1]):
            percentage = (count / len(fastest) * 100) if len(fastest) > 0 else 0
            lines.append(f"  {algo}: {count}/{len(fastest)} ({percentage:.1f}%)")
        
        # Top 10 slowest instances
        lines.append("\n\n⏱️  TOP 10 SLOWEST INSTANCES (by maximum runtime)")
        lines.append("="*80)
        
        max_runtimes = []
        for instance_name, algo_results in self.results_by_instance.items():
            max_rt = max(r.runtime for r in algo_results.values())
            max_runtimes.append((instance_name, max_rt, algo_results))
        
        max_runtimes.sort(key=lambda x: -x[1])
        
        for i, (instance_name, max_rt, algo_results) in enumerate(max_runtimes[:10], 1):
            lines.append(f"\n{i}. {instance_name} (max: {max_rt:.3f}s)")
            for algo_name in sorted(algo_results.keys()):
                result = algo_results[algo_name]
                lines.append(f"   {algo_name:40s}: {result.runtime:>8.3f}s  [{result.nodes:>6} nodes]")
        
        # Instance size analysis
        lines.append("\n\n📏 PERFORMANCE BY INSTANCE SIZE")
        lines.append("="*80)
        
        # Group by size
        size_groups = defaultdict(lambda: defaultdict(list))
        for result in self.results:
            size_key = f"n={result.n}, m={result.m}"
            size_groups[size_key][result.algorithm].append(result.runtime)
        
        lines.append("\nAverage runtime by (n, m):")
        for size_key in sorted(size_groups.keys()):
            lines.append(f"\n{size_key}:")
            for algo in sorted(size_groups[size_key].keys()):
                runtimes = size_groups[size_key][algo]
                avg = sum(runtimes) / len(runtimes)
                lines.append(f"  {algo:40s}: {avg:.3f}s (avg of {len(runtimes)} instances)")
        
        # Footer
        lines.append("\n" + "="*80)
        lines.append(f" Analysis completed: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')} ".center(80))
        lines.append("="*80 + "\n")
        
        return "\n".join(lines)
    
    def analyze(self) -> str:
        """Main analysis workflow."""
        print(f"\n🔍 Loading experiment data from: {self.csv_path}")
        self.load_csv()
        print(f"✓ Loaded {len(self.results)} results from {len(self.results_by_instance)} instances")
        
        print("\n📊 Computing statistics...")
        self.compute_statistics()
        print(f"✓ Computed stats for {len(self.stats_by_algorithm)} algorithms")
        
        print("\n📝 Generating report...")
        report = self.generate_report()
        print("✓ Report generated")
        
        return report


def main():
    """Main entry point."""
    if len(sys.argv) != 2:
        print("Usage: python analyze_experiment.py <csv_file>")
        print("\nExample:")
        print("  python analyze_experiment.py experiments/results/experimento_comparative_formulations_20251027_232256.csv")
        sys.exit(1)
    
    csv_path = sys.argv[1]
    
    try:
        analyzer = ExperimentAnalyzer(csv_path)
        report = analyzer.analyze()
        print(report)
        
        # Optionally save to file
        output_path = Path(csv_path).parent / f"{Path(csv_path).stem}_analysis.txt"
        with open(output_path, 'w') as f:
            f.write(report)
        
        print(f"\n💾 Report saved to: {output_path}")
        
    except FileNotFoundError as e:
        print(f"\n❌ Error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"\n❌ Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()

