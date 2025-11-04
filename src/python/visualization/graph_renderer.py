#!/usr/bin/env python3
"""
Graph Renderer - Create Bokeh visualizations of OCST instances.

This module handles the conversion of NetworkX graphs to Bokeh plots
with interactive features (hover, zoom, pan).
"""

import networkx as nx
from bokeh.plotting import figure
from bokeh.models import (
    Circle, MultiLine, HoverTool, BoxZoomTool, 
    ResetTool, WheelZoomTool, PanTool
)
from bokeh.models.graphs import from_networkx
from typing import Dict, Tuple


def compute_spring_layout(G: nx.Graph, seed: int = 42) -> Dict[int, Tuple[float, float]]:
    """
    Compute spring layout positions for graph nodes.
    
    Args:
        G: NetworkX graph
        seed: Random seed for reproducibility
        
    Returns:
        Dictionary mapping node ID to (x, y) coordinates
    """
    return nx.spring_layout(G, seed=seed, k=1.5, iterations=50)


def create_graph_plot(G: nx.Graph, title: str = "OCST Instance") -> figure:
    """
    Create a Bokeh plot for the given graph.
    
    Args:
        G: NetworkX graph to visualize
        title: Plot title
        
    Returns:
        Bokeh figure object
    """
    # Create figure
    plot = figure(
        title=title,
        width=900,
        height=700,
        x_range=(-1.2, 1.2),
        y_range=(-1.2, 1.2),
        toolbar_location="above",
        tools=""
    )
    
    # Add tools
    plot.add_tools(
        PanTool(),
        WheelZoomTool(),
        BoxZoomTool(),
        ResetTool()
    )
    
    # Configure plot appearance
    plot.background_fill_color = "#f5f5f5"
    plot.grid.grid_line_color = None
    plot.axis.visible = False
    
    # Compute layout
    layout = compute_spring_layout(G)
    
    # Create graph renderer from NetworkX
    graph_renderer = from_networkx(G, layout, scale=1, center=(0, 0))
    
    # Configure node appearance
    graph_renderer.node_renderer.glyph = Circle(
        size=20,
        fill_color="#3498db",
        line_color="#2c3e50",
        line_width=2
    )
    graph_renderer.node_renderer.hover_glyph = Circle(
        size=20,
        fill_color="#e74c3c",
        line_color="#c0392b",
        line_width=2
    )
    
    # Configure edge appearance
    graph_renderer.edge_renderer.glyph = MultiLine(
        line_color="#95a5a6",
        line_alpha=0.6,
        line_width=2
    )
    graph_renderer.edge_renderer.hover_glyph = MultiLine(
        line_color="#e74c3c",
        line_alpha=0.8,
        line_width=3
    )
    
    # Add hover tool for nodes
    node_hover = HoverTool(
        tooltips=[("Node ID", "@index")],
        renderers=[graph_renderer.node_renderer]
    )
    plot.add_tools(node_hover)
    
    # Add hover tool for edges (show cost)
    edge_hover = HoverTool(
        tooltips=[("Cost", "@cost")],
        renderers=[graph_renderer.edge_renderer]
    )
    plot.add_tools(edge_hover)
    
    # Add graph to plot
    plot.renderers.append(graph_renderer)
    
    return plot


def update_graph_plot(plot: figure, G: nx.Graph, title: str):
    """
    Update an existing plot with a new graph.
    
    Args:
        plot: Existing Bokeh figure
        G: New NetworkX graph
        title: New title
    """
    # Clear existing renderers
    plot.renderers = []
    
    # Update title
    plot.title.text = title
    
    # Compute new layout
    layout = compute_spring_layout(G)
    
    # Create new graph renderer
    graph_renderer = from_networkx(G, layout, scale=1, center=(0, 0))
    
    # Configure node appearance
    graph_renderer.node_renderer.glyph = Circle(
        size=20,
        fill_color="#3498db",
        line_color="#2c3e50",
        line_width=2
    )
    graph_renderer.node_renderer.hover_glyph = Circle(
        size=20,
        fill_color="#e74c3c",
        line_color="#c0392b",
        line_width=2
    )
    
    # Configure edge appearance
    graph_renderer.edge_renderer.glyph = MultiLine(
        line_color="#95a5a6",
        line_alpha=0.6,
        line_width=2
    )
    graph_renderer.edge_renderer.hover_glyph = MultiLine(
        line_color="#e74c3c",
        line_alpha=0.8,
        line_width=3
    )
    
    # Add graph to plot
    plot.renderers.append(graph_renderer)

