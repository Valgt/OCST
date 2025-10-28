#!/usr/bin/env python3
"""
Flow-Based Relaxed Formulation Experiment Runner

This script executes the flow-based relaxed formulation algorithm on all available instances
and generates a comprehensive CSV report with results.

Usage:
    python run_flow_based_relaxed_experiment.py [config_file]

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

class FlowBasedRelaxedExperimentRunner:
    """Main class for running flow-based relaxed formulation experiments"""
    
    def __init__(self, config_file="experiments/config/flow_based_relaxed_experiment.yaml"):
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
        self.logger.info("Flow-Based Relaxed Formulation Experiment Started")
    
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
    
    def run_instance(self, instance_info):
        """Run the algorithm on a single instance"""
        instance_name = instance_info['name']
        instance_path = instance_info['path']
        
        self.logger.info(f"Running instance: {instance_name}")
        
        # Prepare command
        executable = self.config['execution']['executable_path']
        
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
            result_data = self._parse_output(output_lines, instance_info, runtime)
            
            if result.returncode == 0:
                self.logger.info(f"✓ {instance_name}: {result_data['best_found']} (optimal: {result_data['is_optimal']})")
            else:
                self.logger.error(f"❌ {instance_name}: Execution failed")
                result_data['error'] = result.stderr
            
            return result_data
            
        except subprocess.TimeoutExpired:
            self.logger.error(f"❌ {instance_name}: Timeout after {time_limit}s")
            return {
                'instance_name': instance_name,
                'error': 'Timeout',
                'runtime_seconds': time_limit,
                'best_found': None,
                'is_optimal': False
            }
        except Exception as e:
            self.logger.error(f"❌ {instance_name}: Error - {str(e)}")
            return {
                'instance_name': instance_name,
                'error': str(e),
                'runtime_seconds': 0,
                'best_found': None,
                'is_optimal': False
            }
    
    def _parse_output(self, output_lines, instance_info, runtime):
        """Parse the algorithm output to extract results"""
        result_data = {
            'instance_name': instance_info['name'],
            'instance_group': instance_info['group'],
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
            'mip_gap_percent': 100.0,
            'gurobi_status': -1
        })
        
        # Parse output lines
        for line in output_lines:
            if "Instance stats:" in line:
                # Extract: Instance stats: 10 nodes, 24 edges, 15 requirements
                parts = line.split(':')[1].strip().split(',')
                for part in parts:
                    part = part.strip()
                    if 'nodes' in part:
                        result_data['num_nodes'] = int(part.split()[0])
                    elif 'edges' in part:
                        result_data['num_edges'] = int(part.split()[0])
                    elif 'requirements' in part:
                        result_data['num_requirements'] = int(part.split()[0])
            
            elif "Objective value:" in line:
                # Extract: Objective value: 1340
                value_str = line.split(':')[1].strip()
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
            
            elif "MIP gap:" in line:
                # Extract: MIP gap: 0%
                gap_str = line.split(':')[1].strip().replace('%', '')
                try:
                    result_data['mip_gap_percent'] = float(gap_str)
                except ValueError:
                    result_data['mip_gap_percent'] = 100.0
        
        # Get optimal solution
        result_data['optimal_known'] = self.get_optimal_solution(instance_info['name'])
        
        # Validate consistency between found solution and solution file
        if result_data['best_found'] is not None and result_data['optimal_known'] is not None:
            if result_data['best_found'] != result_data['optimal_known']:
                self.logger.warning(f"⚠️ {instance_info['name']}: Found {result_data['best_found']} but optimal is {result_data['optimal_known']}")
        
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
        
        total_instances = len(self.results)
        optimal_solutions = sum(1 for r in self.results if r.get('is_optimal', False))
        failed_instances = sum(1 for r in self.results if r.get('error'))
        
        avg_runtime = sum(r.get('runtime_seconds', 0) for r in self.results) / total_instances
        max_runtime = max(r.get('runtime_seconds', 0) for r in self.results)
        
        # Calculate time limits by group
        time_limits_info = self.config['execution'].get('time_limits_by_group', {})
        time_limits_str = ", ".join([f"{group}: {limit}s" for group, limit in time_limits_info.items()])
        if not time_limits_str:
            time_limits_str = f"Default: {self.config['execution']['time_limit']}s"
        
        summary = f"""
=== EXPERIMENT SUMMARY ===
Algorithm: {self.config['experiment']['algorithm']}
Version: {self.config['experiment']['version']}
Timestamp: {self.timestamp}

Instances:
  Total: {total_instances}
  Optimal: {optimal_solutions} ({optimal_solutions/total_instances*100:.1f}%)
  Failed: {failed_instances} ({failed_instances/total_instances*100:.1f}%)

Performance:
  Average Runtime: {avg_runtime:.3f} seconds
  Maximum Runtime: {max_runtime:.3f} seconds
  Time Limits: {time_limits_str}

Configuration:
  Heuristics Level: {self.config['execution']['heuristics_level']}
  Executable: {self.config['execution']['executable_path']}
========================
"""
        
        print(summary)
        self.logger.info("Experiment completed successfully")
    
    def run_experiment(self):
        """Main method to run the complete experiment"""
        self.logger.info("Starting Flow-Based Relaxed Formulation Experiment")
        
        # Get all instances
        instances = self.get_instances()
        
        if not instances:
            self.logger.error("No instances found to test")
            return
        
        # Run each instance
        for i, instance_info in enumerate(instances, 1):
            self.logger.info(f"Progress: {i}/{len(instances)} - {instance_info['name']}")
            result = self.run_instance(instance_info)
            self.results.append(result)
        
        # Save results
        csv_path = self.save_results()
        
        # Generate summary
        if self.config['analysis']['generate_summary']:
            self.generate_summary()
        
        return csv_path

def main():
    """Main entry point"""
    config_file = sys.argv[1] if len(sys.argv) > 1 else "experiments/config/flow_based_relaxed_experiment.yaml"
    
    print("🚀 Flow-Based Relaxed Formulation Experiment Runner")
    print("=" * 50)
    
    try:
        runner = FlowBasedRelaxedExperimentRunner(config_file)
        csv_path = runner.run_experiment()
        
        print(f"\n✅ Experiment completed successfully!")
        print(f"📊 Results saved to: {csv_path}")
        
    except KeyboardInterrupt:
        print("\n⚠️ Experiment interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n❌ Experiment failed: {str(e)}")
        sys.exit(1)

if __name__ == "__main__":
    main()

