#!/usr/bin/env python3
"""
Debug script to analyze ocstpin1 instance and check for requirement conflicts
"""

def parse_instance(filename):
    with open(filename, 'r') as f:
        lines = f.readlines()
    
    # Parse header
    header = lines[0].strip().split()
    n, m, prob = int(header[0]), int(header[1]), float(header[2])
    
    print(f"Nodes: {n}, Edges: {m}, Probability: {prob}")
    
    # Parse edges
    edges = []
    for i in range(1, m + 1):
        parts = lines[i].strip().split()
        u, v, cost = int(parts[0]), int(parts[1]), float(parts[2])
        edges.append((u, v, cost))
    
    # Parse requirements
    num_reqs = int(lines[m + 1].strip())
    requirements = []
    for i in range(m + 2, m + 2 + num_reqs):
        parts = lines[i].strip().split()
        origin, dest, weight = int(parts[0]), int(parts[1]), float(parts[2])
        requirements.append((origin, dest, weight))
    
    return n, edges, requirements

def analyze_requirements(n, requirements):
    print(f"\n=== ORIGINAL REQUIREMENTS ===")
    for i, (origin, dest, weight) in enumerate(requirements):
        print(f"  {i}: ({origin}, {dest}) weight={weight}")
    
    print(f"\n=== REQUIREMENTS STARTING FROM NODE 0 ===")
    from_zero = [(origin, dest, weight) for origin, dest, weight in requirements if origin == 0]
    for origin, dest, weight in from_zero:
        print(f"  ({origin}, {dest}) weight={weight}")
    
    print(f"\n=== ARTIFICIAL REQUIREMENTS TO ADD ===")
    existing_from_zero = set()
    for origin, dest, weight in requirements:
        existing_from_zero.add((origin, dest))
        # Note: NOT adding reverse direction as requirements are directional
    
    print("Existing requirements (directional):", existing_from_zero)
    
    artificial_added = []
    for v in range(n):
        if v != 0:  # root_node = 0
            req_pair = (0, v)
            if req_pair not in existing_from_zero:
                artificial_added.append(req_pair)
                print(f"  Would add: (0, {v}) weight=0")
            else:
                print(f"  SKIP (exists): (0, {v}) - already exists with weight={dict(requirements).get(req_pair, 'unknown')}")
    
    print(f"\nArtificial requirements to add: {len(artificial_added)}")
    print(f"Total requirements after: {len(requirements) + len(artificial_added)}")
    
    return artificial_added

def main():
    filename = "data/input/test_instances/ocstpin1"
    n, edges, requirements = parse_instance(filename)
    artificial_added = analyze_requirements(n, requirements)
    
    # Calculate expected objective
    print(f"\n=== OBJECTIVE CALCULATION ===")
    total_weight = 0
    for origin, dest, weight in requirements:
        if weight > 0:  # Only count non-artificial requirements
            total_weight += weight
            print(f"  ({origin}, {dest}): weight={weight}")
    
    print(f"Total weight from original requirements: {total_weight}")
    
    # Read expected optimal
    try:
        with open("data/output/test_instances/ocstpin1.sol", 'r') as f:
            expected_optimal = float(f.readline().strip())
        print(f"Expected optimal solution: {expected_optimal}")
        
        if total_weight > 0:
            print(f"Ratio (optimal/total_weight): {expected_optimal/total_weight:.2f}")
    except Exception as e:
        print(f"Could not read expected solution: {e}")

if __name__ == "__main__":
    main()
