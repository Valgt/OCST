#!/usr/bin/env python3
"""
Graph Loader - Load OCST instances from JSON format.

This module reads OCST problem instances from standardized JSON files
and converts them to NetworkX graph objects for visualization.
"""

import json
from pathlib import Path
from typing import Dict, List, Tuple
import networkx as nx


class OCSTInstance:
    """Represents an OCST problem instance."""
    
    def __init__(self, filepath: Path):
        """
        Load an OCST instance from JSON file.
        
        Args:
            filepath: Path to JSON instance file
        """
        self.filepath = filepath
        self.data = self._load_json()
        self.name = self.data.get("name", filepath.stem)
        self.graph = self._build_graph()
        
    def _load_json(self) -> Dict:
        """Load and parse JSON file."""
        with open(self.filepath, 'r') as f:
            return json.load(f)
    
    def _build_graph(self) -> nx.Graph:
        """
        Build NetworkX graph from instance data.
        
        Returns:
            NetworkX Graph with nodes and weighted edges
        """
        G = nx.Graph()
        
        # Add nodes
        num_nodes = self.data["graph"]["nodes"]
        G.add_nodes_from(range(num_nodes))
        
        # Add edges with costs
        for edge in self.data["graph"]["edges"]:
            u = edge["source"]
            v = edge["destination"]
            cost = edge["cost"]
            G.add_edge(u, v, cost=cost)
        
        return G
    
    def get_requirements(self) -> List[Tuple[int, int, float]]:
        """
        Get list of requirements.
        
        Returns:
            List of tuples (origin, destination, weight)
        """
        requirements = []
        for req in self.data.get("requirements", []):
            requirements.append((
                req["origin"],
                req["destination"],
                req["weight"]
            ))
        return requirements
    
    def get_metadata(self) -> Dict:
        """Get instance metadata."""
        return self.data.get("metadata", {})
    
    def get_tags(self) -> List[str]:
        """Get instance tags."""
        return self.data.get("tags", [])
    
    def __repr__(self):
        return f"OCSTInstance(name={self.name}, nodes={self.graph.number_of_nodes()}, edges={self.graph.number_of_edges()})"


def get_available_instances(data_dir: Path) -> List[Path]:
    """
    Get list of available instance files.
    
    Args:
        data_dir: Directory containing JSON instance files
        
    Returns:
        Sorted list of Path objects for instance files
    """
    instance_files = sorted(data_dir.glob("*.json"))
    return instance_files


def load_instance(filepath: Path) -> OCSTInstance:
    """
    Load a single OCST instance.
    
    Args:
        filepath: Path to instance JSON file
        
    Returns:
        OCSTInstance object
    """
    return OCSTInstance(filepath)


def load_solution(filepath: Path):
    """
    Load solution from JSON file.
    
    Args:
        filepath: Path to solution JSON file
        
    Returns:
        Dictionary with solution data (tree_edges, tree_cost, etc.) or None if not found
    """
    with open(filepath, 'r') as f:
        data = json.load(f)
    
    return data.get('solution', None)

