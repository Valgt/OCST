#!/usr/bin/env python3
"""
Graph Renderer - Create Bokeh visualizations of OCST instances.

This module handles the conversion of NetworkX graphs to Bokeh plots
with interactive features (hover, zoom, pan).
"""

import networkx as nx
from bokeh.plotting import figure, from_networkx
from bokeh.models import (
    Circle, MultiLine, HoverTool, BoxZoomTool, 
    ResetTool, WheelZoomTool, PanTool, CustomJS
)
from bokeh.transform import linear_cmap
from bokeh.palettes import Greys256
from typing import Dict, Tuple, List


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


def compute_edge_colors(G: nx.Graph) -> List[str]:
    """
    Compute edge colors based on cost (darker = higher cost).
    Special handling for cost=0 edges (shown in green as "free" edges).
    
    Args:
        G: NetworkX graph with 'cost' edge attribute
        
    Returns:
        List of color hex strings for each edge
    """
    # Get all edge costs
    costs = [G[u][v]['cost'] for u, v in G.edges()]
    
    if not costs:
        return []
    
    # Separate zero-cost edges (special case)
    colors = []
    non_zero_costs = [c for c in costs if c > 0]
    
    if not non_zero_costs:
        # All edges have cost 0 - make them green (free edges)
        return ["#27ae60"] * len(costs)
    
    # Normalize only non-zero costs
    min_cost = min(non_zero_costs)
    max_cost = max(non_zero_costs)
    
    for cost in costs:
        if cost == 0:
            # Cost 0 = green (free edge - very important!)
            colors.append("#27ae60")
        elif max_cost == min_cost:
            # All non-zero costs are the same
            colors.append("#808080")
        else:
            # Normalize and map to grayscale
            norm_cost = (cost - min_cost) / (max_cost - min_cost)
            # Map [0, 1] to [200, 50] for RGB values (light to dark)
            intensity = int(200 - norm_cost * 150)
            color = f"#{intensity:02x}{intensity:02x}{intensity:02x}"
            colors.append(color)
    
    return colors


def accumulate_bidirectional_requirements(requirements: list) -> list:
    """
    Accumulate requirements in both directions and filter zero-weight ones.
    
    If requirement (u,v,a) and (v,u,b) exist, they become a single (u,v,a+b).
    Requirements with accumulated weight = 0 are removed.
    
    Args:
        requirements: List of (origin, destination, weight) tuples
        
    Returns:
        List of (u, v, accumulated_weight) tuples with u < v (canonical form)
    """
    # Dictionary to accumulate weights: {(u,v): total_weight}
    accumulated = {}
    
    for origin, destination, weight in requirements:
        # Canonical form: always (min, max) to treat as undirected
        u, v = min(origin, destination), max(origin, destination)
        
        if (u, v) not in accumulated:
            accumulated[(u, v)] = 0
        accumulated[(u, v)] += weight
    
    # Filter out zero-weight requirements and convert to list
    result = [(u, v, w) for (u, v), w in accumulated.items() if abs(w) > 1e-9]
    
    return result


def create_requirements_plot(G: nx.Graph, requirements: list, layout: Dict, title: str = "Requirements") -> figure:
    """
    Create a Bokeh plot showing requirements overlaid on the graph topology.
    
    Requirements are accumulated bidirectionally (u→v + v→u) and zero-weight ones are filtered.
    
    Args:
        G: NetworkX graph (for topology/layout)
        requirements: List of (origin, destination, weight) tuples
        layout: Node positions (same as main graph)
        title: Plot title
        
    Returns:
        Bokeh figure object
    """
    # Create figure with same dimensions as main plot
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
    
    # Accumulate bidirectional requirements and filter zeros
    accumulated_reqs = accumulate_bidirectional_requirements(requirements)
    
    # Create a graph with only nodes (no edges from original graph)
    req_graph = nx.Graph()
    req_graph.add_nodes_from(G.nodes())
    
    # Add accumulated requirement edges
    req_weights = []
    for u, v, weight in accumulated_reqs:
        if u in G.nodes() and v in G.nodes():
            req_graph.add_edge(u, v, weight=weight)
            req_weights.append(weight)
    
    # Normalize requirement weights for coloring (red intensity)
    if req_weights:
        min_weight = min(req_weights)
        max_weight = max(req_weights)
        
        if max_weight == min_weight:
            req_colors = ["#e74c3c"] * len(req_weights)
        else:
            req_colors = []
            for weight in req_weights:
                norm = (weight - min_weight) / (max_weight - min_weight)
                # Light red to dark red
                intensity = int(255 - norm * 100)
                color = f"#ff{intensity:02x}{intensity:02x}"
                req_colors.append(color)
        
        # Add colors as edge attribute
        for i, (u, v) in enumerate(req_graph.edges()):
            req_graph[u][v]['edge_color'] = req_colors[i]
    
    # Create graph renderer with provided layout
    graph_renderer = from_networkx(req_graph, layout, scale=1, center=(0, 0))
    
    # Add weight data for hover
    if req_graph.edges():
        edge_weights = [req_graph[u][v]['weight'] for u, v in req_graph.edges()]
        edge_colors = [req_graph[u][v].get('edge_color', '#e74c3c') for u, v in req_graph.edges()]
        graph_renderer.edge_renderer.data_source.data['weight'] = edge_weights
        graph_renderer.edge_renderer.data_source.data['edge_color'] = edge_colors
    
    # Configure node appearance (smaller, gray)
    graph_renderer.node_renderer.glyph = Circle(
        radius=0.04,
        fill_color="#95a5a6",
        line_color="#7f8c8d",
        line_width=1
    )
    graph_renderer.node_renderer.hover_glyph = Circle(
        radius=0.04,
        fill_color="#3498db",
        line_color="#2980b9",
        line_width=2
    )
    
    # Configure edge appearance (requirements in red tones)
    graph_renderer.edge_renderer.glyph = MultiLine(
        line_color="edge_color",
        line_alpha=0.7,
        line_width=3
    )
    graph_renderer.edge_renderer.hover_glyph = MultiLine(
        line_color="#c0392b",
        line_alpha=1.0,
        line_width=5
    )
    
    # Add hover tool for nodes
    node_hover = HoverTool(
        tooltips=[("Node ID", "@index")],
        renderers=[graph_renderer.node_renderer]
    )
    plot.add_tools(node_hover)
    
    # Add hover tool for requirement edges
    edge_hover = HoverTool(
        tooltips=[
            ("Weight", "@weight{0.00}"),
            ("Requirement", "(@start, @end)")
        ],
        renderers=[graph_renderer.edge_renderer]
    )
    plot.add_tools(edge_hover)
    
    # Add graph to plot
    plot.renderers.append(graph_renderer)
    
    return plot


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
    
    # Compute edge colors based on cost
    edge_colors = compute_edge_colors(G)
    
    # Add colors as edge attribute
    for i, (u, v) in enumerate(G.edges()):
        G[u][v]['edge_color'] = edge_colors[i] if edge_colors else "#95a5a6"
    
    # Create graph renderer from NetworkX
    graph_renderer = from_networkx(G, layout, scale=1, center=(0, 0))
    
    # Add cost data to edge renderer for hover tooltips
    edge_costs = [G[u][v]['cost'] for u, v in G.edges()]
    graph_renderer.edge_renderer.data_source.data['cost'] = edge_costs
    graph_renderer.edge_renderer.data_source.data['edge_color'] = edge_colors
    
    # Configure node appearance
    graph_renderer.node_renderer.glyph = Circle(
        radius=0.05,
        fill_color="#3498db",
        line_color="#2c3e50",
        line_width=2
    )
    graph_renderer.node_renderer.hover_glyph = Circle(
        radius=0.05,
        fill_color="#e74c3c",
        line_color="#c0392b",
        line_width=2
    )
    
    # Configure edge appearance with cost-based colors
    graph_renderer.edge_renderer.glyph = MultiLine(
        line_color="edge_color",
        line_alpha=0.8,
        line_width=2.5
    )
    graph_renderer.edge_renderer.hover_glyph = MultiLine(
        line_color="#e74c3c",
        line_alpha=1.0,
        line_width=4
    )
    
    # Add hover tool for nodes
    node_hover = HoverTool(
        tooltips=[("Node ID", "@index")],
        renderers=[graph_renderer.node_renderer]
    )
    plot.add_tools(node_hover)
    
    # Add hover tool for edges (show cost with formatting)
    edge_hover = HoverTool(
        tooltips=[
            ("Cost", "@cost{0.00}"),
            ("Edge", "(@start, @end)")
        ],
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
    # Clear existing renderers and tools
    plot.renderers = []
    # Remove old hover tools (keep pan, zoom, reset, box_zoom)
    plot.tools = [t for t in plot.tools if not isinstance(t, HoverTool)]
    
    # Update title
    plot.title.text = title
    
    # Compute new layout
    layout = compute_spring_layout(G)
    
    # Compute edge colors based on cost
    edge_colors = compute_edge_colors(G)
    
    # Add colors as edge attribute
    for i, (u, v) in enumerate(G.edges()):
        G[u][v]['edge_color'] = edge_colors[i] if edge_colors else "#95a5a6"
    
    # Create new graph renderer
    graph_renderer = from_networkx(G, layout, scale=1, center=(0, 0))
    
    # Add cost data to edge renderer for hover tooltips
    edge_costs = [G[u][v]['cost'] for u, v in G.edges()]
    graph_renderer.edge_renderer.data_source.data['cost'] = edge_costs
    graph_renderer.edge_renderer.data_source.data['edge_color'] = edge_colors
    
    # Configure node appearance
    graph_renderer.node_renderer.glyph = Circle(
        radius=0.05,
        fill_color="#3498db",
        line_color="#2c3e50",
        line_width=2
    )
    graph_renderer.node_renderer.hover_glyph = Circle(
        radius=0.05,
        fill_color="#e74c3c",
        line_color="#c0392b",
        line_width=2
    )
    
    # Configure edge appearance with cost-based colors
    graph_renderer.edge_renderer.glyph = MultiLine(
        line_color="edge_color",
        line_alpha=0.8,
        line_width=2.5
    )
    graph_renderer.edge_renderer.hover_glyph = MultiLine(
        line_color="#e74c3c",
        line_alpha=1.0,
        line_width=4
    )
    
    # Add hover tool for nodes
    node_hover = HoverTool(
        tooltips=[("Node ID", "@index")],
        renderers=[graph_renderer.node_renderer]
    )
    plot.add_tools(node_hover)
    
    # Add hover tool for edges (show cost with formatting)
    edge_hover = HoverTool(
        tooltips=[
            ("Cost", "@cost{0.00}"),
            ("Edge", "(@start, @end)")
        ],
        renderers=[graph_renderer.edge_renderer]
    )
    plot.add_tools(edge_hover)
    
    # Add graph to plot
    plot.renderers.append(graph_renderer)

