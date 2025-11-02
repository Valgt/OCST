#!/usr/bin/env python3
"""
Convert .ocstpin legacy format to standardized JSON format.

Usage:
    convert_ocstpin_to_json.py <input_file.ocstpin> [output_file.json]
    convert_ocstpin_to_json.py --batch <input_directory>

The script reads legacy .ocstpin format and converts it to JSON
following the instance.schema.v1.json specification.
"""

import json
import sys
import os
from pathlib import Path
from typing import Dict, List, Any, Optional
from datetime import datetime

def parse_ocstpin_file(filepath: str) -> Dict[str, Any]:
    """
    Parse legacy .ocstpin format file.
    
    Format:
        Line 1: n m probability
        Next m lines: u v cost
        Next line: num_requirements
        Next num_requirements lines: origin destination weight
    
    Returns: Dictionary with parsed data
    """
    with open(filepath, 'r') as f:
        lines = f.readlines()
    
    # Parse header: n m probability
    header = lines[0].strip().split()
    if len(header) < 3:
        raise ValueError(f"Invalid header format in {filepath}")
    
    n = int(header[0])
    m = int(header[1])
    probability = float(header[2])
    
    # Parse edges
    edges = []
    for i in range(1, m + 1):
        if i >= len(lines):
            raise ValueError(f"Unexpected end of file while reading edges at line {i + 1}")
        
        edge_line = lines[i].strip().split()
        if len(edge_line) < 3:
            raise ValueError(f"Invalid edge format at line {i + 1}")
        
        u = int(edge_line[0])
        v = int(edge_line[1])
        cost = float(edge_line[2])
        
        # Validate node indices
        if u < 0 or u >= n or v < 0 or v >= n:
            raise ValueError(f"Edge node index out of bounds: ({u},{v}) at line {i + 1}")
        
        edges.append({
            "source": u,
            "destination": v,
            "cost": cost
        })
    
    # Parse number of requirements
    if m + 1 >= len(lines):
        num_requirements = 0
    else:
        try:
            num_requirements = int(lines[m + 1].strip())
        except ValueError:
            raise ValueError(f"Invalid number of requirements at line {m + 2}")
    
    # Parse requirements
    requirements = []
    for i in range(m + 2, m + 2 + num_requirements):
        if i >= len(lines):
            raise ValueError(f"Unexpected end of file while reading requirements at line {i + 1}")
        
        req_line = lines[i].strip().split()
        if len(req_line) < 3:
            raise ValueError(f"Invalid requirement format at line {i + 1}")
        
        origin = int(req_line[0])
        dest = int(req_line[1])
        weight = float(req_line[2])
        
        # Validate node indices
        if origin < 0 or origin >= n or dest < 0 or dest >= n:
            raise ValueError(f"Requirement node index out of bounds: ({origin},{dest}) at line {i + 1}")
        
        requirements.append({
            "origin": origin,
            "destination": dest,
            "weight": weight
        })
    
    return {
        "nodes": n,
        "edges": edges,
        "requirements": requirements,
        "probability": probability
    }

def determine_tags(instance_name: str, num_nodes: int) -> List[str]:
    """
    Determine tags for an instance based on name and size.
    
    'quick_check' is reserved for fast regression subset (typically small instances).
    """
    tags = []
    
    # Add quick_check tag for small instances (<= 15 nodes)
    if num_nodes <= 15:
        tags.append("quick_check")
    
    # Extract base name for additional tags
    instance_lower = instance_name.lower()
    if "ocstpin" in instance_lower:
        tags.append("beasley")
    elif "orst" in instance_lower:
        if "big" in instance_lower:
            tags.append("orst_big")
        else:
            tags.append("orst")
    
    return tags

def convert_to_json(input_file: str, output_file: Optional[str] = None, 
                   tags: Optional[List[str]] = None) -> str:
    """
    Convert .ocstpin file to JSON format.
    
    Args:
        input_file: Path to .ocstpin file
        output_file: Optional output path (default: same name with .json extension in data/input/)
        tags: Optional list of tags (if None, will be determined automatically)
    
    Returns: Path to created JSON file
    """
    # Parse legacy format
    parsed = parse_ocstpin_file(input_file)
    
    # Extract instance name from filename
    instance_name = Path(input_file).stem
    
    # Determine tags if not provided
    if tags is None:
        tags = determine_tags(instance_name, parsed["nodes"])
    
    # Determine source based on instance name
    instance_lower = instance_name.lower()
    if "ocstpin" in instance_lower:
        source = "Beasley OR-Library"
        description = "Converted from legacy .ocstpin format"
    elif "orst" in instance_lower:
        if "big" in instance_lower:
            source = "ORST Big Instances"
            description = "Converted from legacy ORST Big format"
        else:
            source = "ORST Instances"
            description = "Converted from legacy ORST format"
    else:
        source = "Unknown"
        description = "Converted from legacy format"
    
    # Build JSON structure according to schema
    json_data = {
        "schema_version": "1.0",
        "name": instance_name,
        "tags": tags,
        "graph": {
            "nodes": parsed["nodes"],
            "edges": parsed["edges"]
        },
        "requirements": parsed["requirements"],
        "metadata": {
            "probability": parsed["probability"],
            "source": source,
            "generator": "manual",
            "description": description,
            "created_at": datetime.utcnow().isoformat() + "Z"
        }
    }
    
    # Determine output path
    if output_file is None:
        # Save in data/input/ with .json extension
        output_file = os.path.join("data", "input", f"{instance_name}.json")
    
    # Ensure output directory exists
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    
    # Write JSON file (pretty printed for readability)
    with open(output_file, 'w') as f:
        json.dump(json_data, f, indent=2, ensure_ascii=False)
    
    return output_file

def batch_convert(input_dir: str, output_dir: Optional[str] = None):
    """
    Convert all instance files (.ocstpin, orst*, orstBig*) in a directory.
    
    Args:
        input_dir: Directory containing instance files
        output_dir: Output directory (default: data/input/)
    """
    if output_dir is None:
        output_dir = "data/input"
    
    input_path = Path(input_dir)
    # Find all instance files: .ocstpin extension, or files starting with ocstpin, orst, orstBig
    instance_files = (
        list(input_path.glob("*.ocstpin")) + 
        list(input_path.glob("ocstpin*")) +
        list(input_path.glob("orst*")) +
        list(input_path.glob("orstBig*"))
    )
    
    # Filter out directories and only keep files
    instance_files = [f for f in instance_files if f.is_file()]
    
    # Remove duplicates (some patterns might match the same file)
    instance_files = list(set(instance_files))
    
    if not instance_files:
        print(f"No instance files found in {input_dir}")
        return
    
    print(f"Found {len(instance_files)} instance files to convert")
    
    converted = 0
    errors = 0
    
    for instance_file in sorted(instance_files):
        try:
            output_file = os.path.join(output_dir, f"{instance_file.stem}.json")
            convert_to_json(str(instance_file), output_file)
            print(f"✓ Converted {instance_file.name} -> {output_file}")
            converted += 1
        except Exception as e:
            print(f"✗ Error converting {instance_file.name}: {e}")
            errors += 1
    
    print(f"\nConversion complete: {converted} converted, {errors} errors")

def main():
    """Main entry point"""
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    
    if sys.argv[1] == "--batch":
        if len(sys.argv) < 3:
            print("Error: --batch requires input directory")
            sys.exit(1)
        batch_convert(sys.argv[2])
    else:
        input_file = sys.argv[1]
        output_file = sys.argv[2] if len(sys.argv) > 2 else None
        
        try:
            result = convert_to_json(input_file, output_file)
            print(f"✓ Converted {input_file} -> {result}")
        except Exception as e:
            print(f"✗ Error: {e}")
            sys.exit(1)

if __name__ == "__main__":
    main()

