#!/usr/bin/env python3
"""
Comparative Formulations Experiment Runner

This script executes multiple algorithms on the same instances and generates
a comprehensive CSV report for comparison.

Usage:
    python run_comparative_experiment.py [config_file]

Author: OCST Project
Version: 1.0
"""

import os
import sys
import yaml
import csv
import subprocess
import time
import glob
from datetime import datetime
from pathlib import Path
import logging

class ComparativeExperimentRunner:
    """Main class for running comparative experiments across multiple algorithms"""
    
    def __init__(self, config_file="experiments/config/comparative_experiment.yaml"):
        """Initialize the experiment runner with configuration"""
        self.config = self._load_config(config_file)
        self.timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.setup_logging()
        self.results = []
        
    def _load_config(self, config_file):
        """Load configuration from YAML file"""
        try:
            with open(config_file, 'r') as f:
                config = yaml.safe_load(f)
            print(f"✓ Configuration loaded from {config_file}")
            return config
        except FileNotFoundError:
            print(f"❌ Configuration file not found: {config_file}")
            sys.exit(1)
        except yaml.YAMLError as e:
            print(f"❌ Error parsing YAML configuration: {e}")
            sys.exit(1)
    
    def setup_logging(self):
        """Setup logging configuration"""
        log_level = getattr(logging, self.config['logging']['level'].upper())
        
        if self.config['logging']['log_to_file']:
            log_filename = f"experiments/results/experiment_{self.timestamp}.log"
            os.makedirs(os.path.dirname(log_filename), exist_ok=True)
            logging.basicConfig(
                level=log_level,
                format='%(asctime)s - %(levelname)s - %(message)s',
                handlers=[
                    logging.FileHandler(log_filename),
                    logging.StreamHandler(sys.stdout)
                ]
            )
        else:
            logging.basicConfig(
                level=log_level,
                format='%(asctime)s - %(levelname)s - %(message)s'
            )
        
        self.logger = logging.getLogger(__name__)
        self.logger.info("Comparative Formulations Experiment Started")
    
    def get_instances(self):
        """Get all instances to test based on configuration"""
        instances = []
        input_dir = self.config['instances']['input_directory']
        
        for group in self.config['instances']['groups']:
            pattern = os.path.join(input_dir, group['pattern'])
            group_instances = glob.glob(pattern)
            
            for instance_path in sorted(group_instances):
                instance_name = os.path.basename(instance_path)
                instances.append({
                    'name': instance_name,
                    'path': instance_path,
                    'group': group['name'],
                    'description': group['description']
                })
        
        self.logger.info(f"Found {len(instances)} instances to test")
        return instances
    
    def _get_time_limit_for_group(self, group_name):
        """Get time limit for a specific instance group"""
        time_limits = self.config['execution'].get('time_limits_by_group', {})
        return time_limits.get(group_name, self.config['execution']['time_limit'])
    
    def get_optimal_solution(self, instance_name):
        """Get the known optimal solution for an instance"""
        output_dir = self.config['instances']['output_directory']
        sol_file = os.path.join(output_dir, f"{instance_name}.sol")
        
        try:
            with open(sol_file, 'r') as f:
                # Parse as integer to avoid floating point precision issues
                optimal_value = int(float(f.readline().strip()))
            return optimal_value
        except (FileNotFoundError, ValueError):
            self.logger.warning(f"No optimal solution found for {instance_name}")
            return None
    
    def run_instance_with_algorithm(self, instance_info, algorithm_info):
        """Run a specific algorithm on a single instance"""
        instance_name = instance_info['name']
        instance_path = instance_info['path']
        algorithm_name = algorithm_info['name']
        
        self.logger.info(f"Running {algorithm_name} on {instance_name}")
        
        # Prepare command
        executable = algorithm_info['executable_path']
        
        # Get time limit for this instance group
        time_limit = self._get_time_limit_for_group(instance_info['group'])
        
        heuristics = self.config['execution']['heuristics_level']
        
        cmd = [executable, instance_path, "", str(time_limit), str(heuristics)]
        
        try:
            # Run the algorithm
            start_time = time.time()
            result = subprocess.run(
                cmd, 
                capture_output=True, 
                text=True, 
                timeout=time_limit + 10  # Add buffer for subprocess timeout
            )
            runtime = time.time() - start_time
            
            # Parse output
            output_lines = result.stdout.split('\n')
            
            # Extract results
            result_data = self._parse_output(output_lines, instance_info, algorithm_info, runtime)
            
            if result.returncode == 0:
                self.logger.info(f"✓ {algorithm_name} on {instance_name}: {result_data['best_found']} (optimal: {result_data['is_optimal']})")
            else:
                self.logger.error(f"❌ {algorithm_name} on {instance_name}: Execution failed")
                result_data['error'] = result.stderr
            
            return result_data
            
        except subprocess.TimeoutExpired:
            self.logger.error(f"❌ {algorithm_name} on {instance_name}: Timeout after {time_limit}s")
            return {
                'instance_name': instance_name,
                'algorithm_name': algorithm_name,
                'algorithm_description': algorithm_info['description'],
                'error': 'Timeout',
                'runtime_seconds': time_limit,
                'best_found': None,
                'is_optimal': False
            }
        except Exception as e:
            self.logger.error(f"❌ {algorithm_name} on {instance_name}: Error - {str(e)}")
            return {
                'instance_name': instance_name,
                'algorithm_name': algorithm_name,
                'algorithm_description': algorithm_info['description'],
                'error': str(e),
                'runtime_seconds': 0,
                'best_found': None,
                'is_optimal': False
            }
    
    def _parse_output(self, output_lines, instance_info, algorithm_info, runtime):
        """Parse the algorithm output to extract results"""
        result_data = {
            'instance_name': instance_info['name'],
            'instance_group': instance_info['group'],
            'algorithm_name': algorithm_info['name'],
            'algorithm_description': algorithm_info['description'],
            'runtime_seconds': runtime,
            'error': None
        }
        
        # Initialize with defaults
        result_data.update({
            'num_nodes': 0,
            'num_edges': 0,
            'num_requirements': 0,
            'probability': 0.0,
            'best_found': None,
            'is_optimal': False,
            'nodes_explored': 0,
            'lazy_constraints': 0,
            'cutting_planes': 0,
            'mip_gap_percent': 100.0,
            'gurobi_status': -1
        })
        
        # Parse output lines
        for line in output_lines:
            # Handle both "Instance stats:" and "Instance:" formats
            if "Instance stats:" in line or (line.startswith("Instance:") and "nodes" in line):
                # Extract: Instance stats: 10 nodes, 24 edges, 15 requirements
                # OR: Instance: 10 nodes, 24 edges, 15 requirements
                parts = line.split(':')[1].strip().split(',')
                for part in parts:
                    part = part.strip()
                    if 'nodes' in part:
                        result_data['num_nodes'] = int(part.split()[0])
                    elif 'edges' in part:
                        result_data['num_edges'] = int(part.split()[0])
                    elif 'requirements' in part:
                        result_data['num_requirements'] = int(part.split()[0])
            
            # Handle both "Objective value:" and "Objective:" formats
            elif "Objective value:" in line or (line.startswith("Objective:") and ":" in line):
                # Extract: Objective value: 1340 OR Objective: 1340.00
                value_str = line.split(':')[-1].strip()
                try:
                    # Parse as integer to avoid floating point precision issues
                    result_data['best_found'] = int(float(value_str))
                except ValueError:
                    result_data['best_found'] = None
            
            elif "Status:" in line:
                # Extract: Status: OPTIMAL
                status = line.split(':')[1].strip()
                result_data['is_optimal'] = (status == "OPTIMAL")
            
            elif "Nodes explored:" in line:
                # Extract: Nodes explored: 1
                result_data['nodes_explored'] = int(line.split(':')[1].strip())
            
            elif "MIP gap:" in line or "MIP Gap:" in line:
                # Extract: MIP gap: 0% OR MIP Gap: 0.0000%
                gap_str = line.split(':')[1].strip().replace('%', '')
                try:
                    result_data['mip_gap_percent'] = float(gap_str)
                except ValueError:
                    result_data['mip_gap_percent'] = 100.0
            
            elif "Lazy constraints added:" in line:
                # Extract: Lazy constraints added: 10
                try:
                    result_data['lazy_constraints'] = int(line.split(':')[1].strip())
                except (ValueError, IndexError):
                    result_data['lazy_constraints'] = 0
            
            elif "Cutting planes added:" in line:
                # Extract: Cutting planes added: 5
                try:
                    result_data['cutting_planes'] = int(line.split(':')[1].strip())
                except (ValueError, IndexError):
                    result_data['cutting_planes'] = 0
        
        # Get optimal solution
        result_data['optimal_known'] = self.get_optimal_solution(instance_info['name'])
        
        # Validate consistency between found solution and solution file
        if result_data['best_found'] is not None and result_data['optimal_known'] is not None:
            if result_data['best_found'] != result_data['optimal_known']:
                self.logger.warning(f"⚠️ {algorithm_info['name']} on {instance_info['name']}: Found {result_data['best_found']} but optimal is {result_data['optimal_known']}")
        
        # Add experiment parameters
        result_data.update({
            'time_limit': self._get_time_limit_for_group(instance_info['group']),
            'heuristics_level': self.config['execution']['heuristics_level'],
            'algorithm_version': self.config['experiment']['version']
        })
        
        return result_data
    
    def save_results(self):
        """Save results to CSV file"""
        # Create results directory
        results_dir = self.config['output']['results_directory']
        os.makedirs(results_dir, exist_ok=True)
        
        # Generate filename
        filename_template = self.config['output']['csv_filename_template']
        csv_filename = filename_template.format(
            algorithm=self.config['experiment']['name'],
            timestamp=self.timestamp
        )
        csv_path = os.path.join(results_dir, csv_filename)
        
        # Get column order from config
        columns = self.config['output']['columns']
        
        # Write CSV
        with open(csv_path, 'w', newline='', encoding='utf-8') as csvfile:
            writer = csv.DictWriter(csvfile, fieldnames=columns)
            writer.writeheader()
            
            for result in self.results:
                # Only include columns that exist in config
                filtered_result = {col: result.get(col, '') for col in columns}
                writer.writerow(filtered_result)
        
        self.logger.info(f"✓ Results saved to: {csv_path}")
        return csv_path
    
    def generate_summary(self):
        """Generate experiment summary"""
        if not self.results:
            return
        
        # Group results by algorithm
        algorithms = {}
        for result in self.results:
            alg_name = result.get('algorithm_name', 'Unknown')
            if alg_name not in algorithms:
                algorithms[alg_name] = []
            algorithms[alg_name].append(result)
        
        total_instances = len(set(r['instance_name'] for r in self.results))
        total_runs = len(self.results)
        
        summary = f"""
=== COMPARATIVE EXPERIMENT SUMMARY ===
Experiment: {self.config['experiment']['algorithm']}
Version: {self.config['experiment']['version']}
Timestamp: {self.timestamp}

Overall Statistics:
  Total Instances: {total_instances}
  Total Algorithm Runs: {total_runs}
  Algorithms Tested: {len(algorithms)}

Algorithm Performance:
"""
        
        for alg_name, alg_results in algorithms.items():
            optimal_solutions = sum(1 for r in alg_results if r.get('is_optimal', False))
            failed_runs = sum(1 for r in alg_results if r.get('error'))
            avg_runtime = sum(r.get('runtime_seconds', 0) for r in alg_results) / len(alg_results)
            max_runtime = max(r.get('runtime_seconds', 0) for r in alg_results)
            
            summary += f"""
  {alg_name}:
    Runs: {len(alg_results)}
    Optimal: {optimal_solutions} ({optimal_solutions/len(alg_results)*100:.1f}%)
    Failed: {failed_runs} ({failed_runs/len(alg_results)*100:.1f}%)
    Avg Runtime: {avg_runtime:.3f}s
    Max Runtime: {max_runtime:.3f}s
"""
        
        # Calculate time limits by group
        time_limits_info = self.config['execution'].get('time_limits_by_group', {})
        time_limits_str = ", ".join([f"{group}: {limit}s" for group, limit in time_limits_info.items()])
        if not time_limits_str:
            time_limits_str = f"Default: {self.config['execution']['time_limit']}s"
        
        summary += f"""
Configuration:
  Heuristics Level: {self.config['execution']['heuristics_level']}
  Time Limits: {time_limits_str}
========================================
"""
        
        print(summary)
        self.logger.info("Comparative experiment completed successfully")
    
    def run_experiment(self, short_mode=False):
        """Main method to run the complete comparative experiment"""
        self.logger.info("Starting Comparative Formulations Experiment")
        
        # Get all instances
        instances = self.get_instances()
        
        if not instances:
            self.logger.error("No instances found to test")
            return
        
        # Short mode: only run first instance
        if short_mode:
            instances = [instances[0]]
            print(f"🔬 SHORT MODE: Testing only {instances[0]['name']}")
            self.logger.info(f"SHORT MODE: Testing only {instances[0]['name']}")
        
        # Get all algorithms
        algorithms = self.config['execution']['algorithms']
        
        if not algorithms:
            self.logger.error("No algorithms configured")
            return
        
        # Run each algorithm on each instance
        total_runs = len(instances) * len(algorithms)
        current_run = 0
        
        for instance_info in instances:
            for algorithm_info in algorithms:
                current_run += 1
                self.logger.info(f"Progress: {current_run}/{total_runs} - {algorithm_info['name']} on {instance_info['name']}")
                result = self.run_instance_with_algorithm(instance_info, algorithm_info)
                self.results.append(result)
        
        # Save results
        csv_path = self.save_results()
        
        # Generate summary
        if self.config['analysis']['generate_summary']:
            self.generate_summary()
        
        return csv_path

def main():
    """Main entry point"""
    # Check for --short flag
    short_mode = '--short' in sys.argv
    
    # Remove --short from args to get config file
    args = [arg for arg in sys.argv[1:] if arg != '--short']
    config_file = args[0] if args else "experiments/config/comparative_experiment.yaml"
    
    print("🚀 Comparative Formulations Experiment Runner")
    if short_mode:
        print("🔬 SHORT MODE: Testing only 1 instance")
    print("=" * 50)
    
    try:
        runner = ComparativeExperimentRunner(config_file)
        csv_path = runner.run_experiment(short_mode=short_mode)
        
        print(f"\n✅ Comparative experiment completed successfully!")
        print(f"📊 Results saved to: {csv_path}")
        
    except KeyboardInterrupt:
        print("\n⚠️ Experiment interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n❌ Experiment failed: {str(e)}")
        sys.exit(1)

if __name__ == "__main__":
    main()
