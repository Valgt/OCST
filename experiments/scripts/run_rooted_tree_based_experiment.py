#!/usr/bin/env python3
"""
Rooted Tree-Based Formulation Experiment Runner

This script executes the rooted tree-based formulation algorithm on all available instances
and generates a comprehensive CSV report with results.

Usage:
    python run_rooted_tree_based_experiment.py [config_file]

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

class RootedTreeBasedExperimentRunner:
    """Main class for running rooted tree-based formulation experiments"""
    
    def __init__(self, config_file="experiments/config/rooted_tree_based_experiment.yaml"):
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
        self.logger.info("Rooted Tree-Based Formulation Experiment Started")
    
    def get_instances(self):
        """Get all instances to test based on configuration"""
        instances = []
        input_dir = self.config['instances']['input_directory']
        
        for group in self.config['instances']['groups']:
            pattern = os.path.join(input_dir, group['pattern'])
            matching_files = sorted(glob.glob(pattern))
            
            for file_path in matching_files:
                instance_name = os.path.basename(file_path)
                instances.append({
                    'name': instance_name,
                    'group': group['name'],
                    'path': file_path
                })
        
        return instances
    
    def run_algorithm(self, instance):
        """Run the algorithm on a single instance"""
        executable = self.config['algorithm']['executable']
        time_limit = self.config['algorithm']['time_limit']
        heuristics = self.config['algorithm']['heuristics']
        
        # Generate temporary output file
        temp_output = f"/tmp/rooted_tree_result_{instance['name']}.csv"
        
        # Build command
        cmd = [
            executable,
            instance['path'],
            temp_output,
            str(time_limit),
            str(heuristics)
        ]
        
        self.logger.info(f"Running: {instance['name']}")
        start_time = time.time()
        
        try:
            result = subprocess.run(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=time_limit + 10,
                text=True
            )
            
            elapsed = time.time() - start_time
            
            # Parse output CSV
            if os.path.exists(temp_output):
                with open(temp_output, 'r') as f:
                    csv_reader = csv.DictReader(f)
                    for row in csv_reader:
                        self.results.append(row)
                        self.logger.info(f"  ✓ {instance['name']}: {row.get('best_found', 'N/A')}")
                
                # Clean up temp file
                os.remove(temp_output)
                return True
            else:
                self.logger.error(f"  ❌ No output file generated for {instance['name']}")
                return False
                
        except subprocess.TimeoutExpired:
            self.logger.warning(f"  ⏱️ Timeout for {instance['name']}")
            return False
        except Exception as e:
            self.logger.error(f"  ❌ Error running {instance['name']}: {str(e)}")
            return False
    
    def save_results(self):
        """Save all results to CSV file"""
        if not self.results:
            self.logger.warning("No results to save")
            return
        
        output_file = self.config['output']['csv_file'].replace(
            '{timestamp}', self.timestamp
        )
        
        os.makedirs(os.path.dirname(output_file), exist_ok=True)
        
        # Write CSV
        with open(output_file, 'w', newline='') as f:
            if self.results:
                writer = csv.DictWriter(f, fieldnames=self.results[0].keys())
                writer.writeheader()
                writer.writerows(self.results)
        
        self.logger.info(f"✓ Results saved to {output_file}")
        print(f"\n{'='*80}")
        print(f"Results saved to: {output_file}")
        print(f"Total instances processed: {len(self.results)}")
        print(f"{'='*80}\n")
    
    def generate_summary(self):
        """Generate experiment summary"""
        if not self.results:
            return
        
        optimal_count = sum(1 for r in self.results if r.get('is_optimal') == 'True')
        total_runtime = sum(float(r.get('runtime_seconds', 0)) for r in self.results)
        
        print(f"\n{'='*80}")
        print("EXPERIMENT SUMMARY")
        print(f"{'='*80}")
        print(f"Total instances:     {len(self.results)}")
        print(f"Optimal solutions:   {optimal_count}/{len(self.results)}")
        print(f"Total runtime:       {total_runtime:.2f} seconds")
        print(f"Average runtime:     {total_runtime/len(self.results):.2f} seconds")
        print(f"{'='*80}\n")
    
    def run(self):
        """Main execution method"""
        print("\n" + "="*80)
        print("ROOTED TREE-BASED FORMULATION EXPERIMENT")
        print("="*80 + "\n")
        
        # Get instances
        instances = self.get_instances()
        self.logger.info(f"Found {len(instances)} instances to process")
        
        if not instances:
            self.logger.error("No instances found to process")
            return
        
        # Run algorithm on each instance
        success_count = 0
        for i, instance in enumerate(instances, 1):
            print(f"\n[{i}/{len(instances)}] Processing: {instance['name']}")
            if self.run_algorithm(instance):
                success_count += 1
        
        # Save results
        self.save_results()
        
        # Generate summary
        self.generate_summary()
        
        self.logger.info("Experiment completed")


def main():
    """Main entry point"""
    config_file = "experiments/config/rooted_tree_based_experiment.yaml"
    
    if len(sys.argv) > 1:
        config_file = sys.argv[1]
    
    runner = RootedTreeBasedExperimentRunner(config_file)
    runner.run()


if __name__ == "__main__":
    main()

