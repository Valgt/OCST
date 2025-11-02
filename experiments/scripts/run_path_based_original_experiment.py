#!/usr/bin/env python3
"""
Path-Based Formulation Original Experiment Runner

This script executes the original path-based formulation algorithm (baseline/reference)
on all available instances and generates a comprehensive CSV report with results.

This version serves as the golden reference for comparison against standardized versions.

Usage:
    python run_path_based_original_experiment.py [config_file]

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

class PathBasedOriginalExperimentRunner:
    """Main class for running path-based formulation original (baseline) experiments"""
    
    def __init__(self, config_file="experiments/config/path_based_original_experiment.yaml"):
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
            log_filename = f"experiments/results/experiment_path_based_original_{self.timestamp}.log"
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
        self.logger.info("Path-Based Formulation Original (Baseline) Experiment Started")
    
    def get_instances(self):
        """Get all instances to test based on configuration"""
        instances = []
        input_dir = self.config['instances']['input_directory']
        
        for group in self.config['instances']['groups']:
            pattern = os.path.join(input_dir, group['pattern'])
            found = glob.glob(pattern)
            instances.extend([(f, group['name']) for f in sorted(found)])
        
        self.logger.info(f"Found {len(instances)} instances to process")
        return instances
    
    def run_instance(self, instance_path, group_name):
        """Run the algorithm on a single instance"""
        executable = self.config['execution']['executable_path']
        time_limit = self.config['execution'].get('time_limits_by_group', {}).get(group_name, 
                                                                                    self.config['execution']['time_limit'])
        
        if not os.path.exists(executable):
            self.logger.error(f"Executable not found: {executable}")
            return None
        
        instance_name = os.path.basename(instance_path)
        self.logger.info(f"Running {instance_name} (group: {group_name}, time_limit: {time_limit}s)")
        
        start_time = time.time()
        
        try:
            result = subprocess.run(
                [executable, instance_path],
                capture_output=True,
                text=True,
                timeout=time_limit + 60  # Add buffer for cleanup
            )
            
            elapsed_time = time.time() - start_time
            
            # Parse output (this will depend on the actual output format)
            output_lines = result.stdout.split('\n')
            
            # Extract objective value and other metrics from output
            objective = None
            runtime = elapsed_time
            status = "COMPLETED" if result.returncode == 0 else "ERROR"
            
            for line in output_lines:
                if "Objective" in line or "objective" in line.lower():
                    try:
                        # Try to extract objective value
                        parts = line.split()
                        for i, part in enumerate(parts):
                            if "objective" in part.lower() and i + 1 < len(parts):
                                objective = float(parts[i + 1])
                                break
                    except:
                        pass
            
            return {
                'instance': instance_name,
                'group': group_name,
                'objective': objective,
                'runtime': runtime,
                'status': status,
                'return_code': result.returncode
            }
            
        except subprocess.TimeoutExpired:
            self.logger.warning(f"Instance {instance_name} timed out after {time_limit}s")
            return {
                'instance': instance_name,
                'group': group_name,
                'objective': None,
                'runtime': time_limit,
                'status': 'TIMEOUT',
                'return_code': -1
            }
        except Exception as e:
            self.logger.error(f"Error running {instance_name}: {e}")
            return {
                'instance': instance_name,
                'group': group_name,
                'objective': None,
                'runtime': 0,
                'status': 'ERROR',
                'return_code': -1
            }
    
    def run_experiments(self):
        """Run experiments on all instances"""
        instances = self.get_instances()
        
        total = len(instances)
        completed = 0
        
        for instance_path, group_name in instances:
            result = self.run_instance(instance_path, group_name)
            if result:
                self.results.append(result)
                completed += 1
                self.logger.info(f"Progress: {completed}/{total} instances completed")
        
        self.logger.info(f"Experiment completed: {completed}/{total} instances processed")
    
    def save_results(self):
        """Save results to CSV file"""
        output_file = f"experiments/results/path_based_original_{self.timestamp}.csv"
        os.makedirs(os.path.dirname(output_file), exist_ok=True)
        
        with open(output_file, 'w', newline='') as f:
            if not self.results:
                return
            
            writer = csv.DictWriter(f, fieldnames=self.results[0].keys())
            writer.writeheader()
            writer.writerows(self.results)
        
        self.logger.info(f"Results saved to {output_file}")
        print(f"\n✓ Results saved to {output_file}")
        print(f"  Total instances: {len(self.results)}")
        print(f"  Successful: {sum(1 for r in self.results if r['status'] == 'COMPLETED')}")
        print(f"  Errors: {sum(1 for r in self.results if r['status'] == 'ERROR')}")
        print(f"  Timeouts: {sum(1 for r in self.results if r['status'] == 'TIMEOUT')}")

def main():
    """Main entry point"""
    config_file = sys.argv[1] if len(sys.argv) > 1 else "experiments/config/path_based_original_experiment.yaml"
    
    runner = PathBasedOriginalExperimentRunner(config_file)
    runner.run_experiments()
    runner.save_results()

if __name__ == "__main__":
    main()

