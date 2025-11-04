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


def compute_hierarchical_layout(G: nx.Graph, solution_edges: list, root_choice: int = 0) -> Dict[int, Tuple[float, float]]:
    """
    Compute hierarchical tree layout based on solution edges.
    
    This layout arranges nodes in levels to minimize edge crossings.
    Nodes are positioned vertically by their depth in the tree,
    and horizontally spaced to avoid overlaps.
    
    Args:
        G: NetworkX graph with all nodes
        solution_edges: List of dicts with 'source' and 'destination' keys
        root_choice: Index to cycle through different roots and orientations (for variety)
        
    Returns:
        Dictionary mapping node ID to (x, y) coordinates
    """
    # Build tree from solution edges
    tree = nx.Graph()
    tree.add_nodes_from(G.nodes())
    
    for edge in solution_edges:
        u, v = edge['source'], edge['destination']
        tree.add_edge(u, v)
    
    # If not connected or empty, fall back to spring layout
    if not solution_edges or not nx.is_connected(tree):
        return nx.spring_layout(G, seed=42, k=1.5, iterations=50)
    
    # Strategy for choosing root based on root_choice
    # We cycle through: centers, periphery, and all nodes for maximum variety
    all_nodes = list(tree.nodes())
    center_nodes = nx.center(tree)
    periphery_nodes = nx.periphery(tree)
    
    # Create a pool of candidate roots
    candidate_roots = []
    candidate_roots.extend(center_nodes)  # Centers first (balanced trees)
    candidate_roots.extend([n for n in periphery_nodes if n not in center_nodes])  # Periphery (tall trees)
    candidate_roots.extend([n for n in all_nodes if n not in candidate_roots])  # Others
    
    # Select root based on root_choice
    if candidate_roots:
        root_idx = root_choice % len(candidate_roots)
        root = candidate_roots[root_idx]
    else:
        root = all_nodes[0]
    
    # BFS to assign levels
    levels = {root: 0}
    parent = {root: None}
    queue = [root]
    visited = {root}
    
    while queue:
        node = queue.pop(0)
        for neighbor in tree.neighbors(node):
            if neighbor not in visited:
                visited.add(neighbor)
                levels[neighbor] = levels[node] + 1
                parent[neighbor] = node
                queue.append(neighbor)
    
    # Group nodes by level
    max_level = max(levels.values())
    nodes_by_level = [[] for _ in range(max_level + 1)]
    for node, level in levels.items():
        nodes_by_level[level].append(node)
    
    # Compute positions
    layout = {}
    
    # Vertical spacing
    y_spacing = 2.0 / (max_level + 1) if max_level > 0 else 1.0
    
    for level, nodes in enumerate(nodes_by_level):
        # Y coordinate (top to bottom)
        y_base = 1.0 - level * y_spacing
        
        # Horizontal spacing
        n = len(nodes)
        if n == 1:
            x_positions = [0.0]
            y_offsets = [0.0]
        else:
            # Spread nodes evenly across the width
            x_positions = [2.0 * i / (n - 1) - 1.0 for i in range(n)]
            
            # Create a VERY strong curve for excellent edge visibility
            # Strategy: Use parabolic curve + additional vertical spread based on node degree
            y_offsets = []
            curve_depth = 0.8 * y_spacing  # Much deeper curve (80% of level spacing)
            
            for i, x in enumerate(x_positions):
                # Base parabolic curve
                parabola_offset = -curve_depth * (x ** 2)
                
                # Additional spread based on node degree within the tree
                # Nodes with more children/connections get pushed up significantly
                node = nodes[i]
                node_degree = tree.degree(node) if node in tree else 1
                degree_offset = (node_degree - 1) * 0.15 * y_spacing
                
                # Combine offsets
                total_offset = parabola_offset + degree_offset
                y_offsets.append(total_offset)
        
        # Try to minimize crossings by ordering siblings based on parent position
        if level > 0:
            # Sort nodes by their parent's x-coordinate
            nodes_with_parent_x = []
            for node in nodes:
                p = parent.get(node)
                parent_x = layout[p][0] if p is not None and p in layout else 0.0
                nodes_with_parent_x.append((node, parent_x))
            
            # Alternate sorting direction based on root_choice for variety
            reverse_sort = (root_choice % 2 == 1)
            nodes_with_parent_x.sort(key=lambda x: x[1], reverse=reverse_sort)
            nodes = [n for n, _ in nodes_with_parent_x]
        
        # Assign positions with curved offset and strong horizontal jitter
        for i, node in enumerate(nodes):
            y = y_base + y_offsets[i]
            
            # Add STRONG horizontal jitter to prevent perfect vertical alignment
            # Use node ID and level to create consistent but varied offsets
            jitter_seed = (node * 17 + level * 13) % 100
            x_jitter = (jitter_seed / 100.0 - 0.5) * 0.35  # Range: [-0.175, 0.175] - MUCH stronger
            
            x = x_positions[i] + x_jitter
            layout[node] = (x, y)
    
    return layout


def compute_edge_colors_and_styles(G: nx.Graph, attribute: str = 'cost') -> tuple:
    """
    Compute edge colors based on cost/weight (darker = higher value).
    Special handling for value=0 edges (shown as dashed lines).
    
    Args:
        G: NetworkX graph with cost/weight edge attribute
        attribute: Edge attribute to use ('cost' for graph, 'weight' for requirements)
        
    Returns:
        Tuple of (colors list, line_dash list) where line_dash is 'solid' or 'dashed'
    """
    # Get all edge values
    values = [G[u][v][attribute] for u, v in G.edges()]
    
    if not values:
        return [], []
    
    colors = []
    line_dashes = []
    non_zero_values = [v for v in values if v > 0]
    
    if not non_zero_values:
        # All edges have value 0 - light gray dashed
        return ["#c8c8c8"] * len(values), ["dashed"] * len(values)
    
    # Normalize only non-zero values
    min_val = min(non_zero_values)
    max_val = max(non_zero_values)
    
    for value in values:
        if value == 0 or abs(value) < 1e-9:
            # Value 0 = light gray + dashed (free edge)
            colors.append("#c8c8c8")
            line_dashes.append("dashed")
        elif max_val == min_val:
            # All non-zero values are the same
            colors.append("#808080")
            line_dashes.append("solid")
        else:
            # Normalize and map to grayscale
            norm_val = (value - min_val) / (max_val - min_val)
            # Map [0, 1] to [200, 50] for RGB values (light to dark)
            intensity = int(200 - norm_val * 150)
            color = f"#{intensity:02x}{intensity:02x}{intensity:02x}"
            colors.append(color)
            line_dashes.append("solid")
    
    return colors, line_dashes


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
    
    # Compute edge colors and styles (same grayscale as graph, based on weight)
    req_colors, req_line_dashes = compute_edge_colors_and_styles(req_graph, attribute='weight')
    
    # Add colors and line dash as edge attributes
    for i, (u, v) in enumerate(req_graph.edges()):
        req_graph[u][v]['edge_color'] = req_colors[i] if req_colors else "#808080"
        req_graph[u][v]['line_dash'] = req_line_dashes[i] if req_line_dashes else "solid"
    
    # Create graph renderer with provided layout
    graph_renderer = from_networkx(req_graph, layout, scale=1, center=(0, 0))
    
    # Add weight data for hover
    if req_graph.edges():
        edge_weights = [req_graph[u][v]['weight'] for u, v in req_graph.edges()]
        edge_colors = [req_graph[u][v].get('edge_color', '#808080') for u, v in req_graph.edges()]
        edge_line_dashes = [req_graph[u][v].get('line_dash', 'solid') for u, v in req_graph.edges()]
        graph_renderer.edge_renderer.data_source.data['weight'] = edge_weights
        graph_renderer.edge_renderer.data_source.data['edge_color'] = edge_colors
        graph_renderer.edge_renderer.data_source.data['line_dash'] = edge_line_dashes
    
    # Configure node appearance (RED nodes for requirements)
    graph_renderer.node_renderer.glyph = Circle(
        radius=0.04,
        fill_color="#e74c3c",
        line_color="#c0392b",
        line_width=2
    )
    graph_renderer.node_renderer.hover_glyph = Circle(
        radius=0.04,
        fill_color="#c0392b",
        line_color="#a93226",
        line_width=2
    )
    
    # Configure edge appearance (grayscale based on weight, dashed for zero-weight)
    graph_renderer.edge_renderer.glyph = MultiLine(
        line_color="edge_color",
        line_dash="line_dash",
        line_alpha=0.8,
        line_width=3
    )
    graph_renderer.edge_renderer.hover_glyph = MultiLine(
        line_color="#e74c3c",
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
    
    # Compute edge colors and styles based on cost
    edge_colors, edge_line_dashes = compute_edge_colors_and_styles(G, attribute='cost')
    
    # Add colors and line dash as edge attributes
    for i, (u, v) in enumerate(G.edges()):
        G[u][v]['edge_color'] = edge_colors[i] if edge_colors else "#95a5a6"
        G[u][v]['line_dash'] = edge_line_dashes[i] if edge_line_dashes else "solid"
    
    # Create graph renderer from NetworkX
    graph_renderer = from_networkx(G, layout, scale=1, center=(0, 0))
    
    # Add cost data to edge renderer for hover tooltips
    edge_costs = [G[u][v]['cost'] for u, v in G.edges()]
    graph_renderer.edge_renderer.data_source.data['cost'] = edge_costs
    graph_renderer.edge_renderer.data_source.data['edge_color'] = edge_colors
    graph_renderer.edge_renderer.data_source.data['line_dash'] = edge_line_dashes
    
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
    
    # Configure edge appearance with cost-based colors and line dash
    graph_renderer.edge_renderer.glyph = MultiLine(
        line_color="edge_color",
        line_dash="line_dash",
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
    
    # Compute edge colors and styles based on cost
    edge_colors, edge_line_dashes = compute_edge_colors_and_styles(G, attribute='cost')
    
    # Add colors and line dash as edge attributes
    for i, (u, v) in enumerate(G.edges()):
        G[u][v]['edge_color'] = edge_colors[i] if edge_colors else "#95a5a6"
        G[u][v]['line_dash'] = edge_line_dashes[i] if edge_line_dashes else "solid"
    
    # Create new graph renderer
    graph_renderer = from_networkx(G, layout, scale=1, center=(0, 0))
    
    # Add cost data to edge renderer for hover tooltips
    edge_costs = [G[u][v]['cost'] for u, v in G.edges()]
    graph_renderer.edge_renderer.data_source.data['cost'] = edge_costs
    graph_renderer.edge_renderer.data_source.data['edge_color'] = edge_colors
    graph_renderer.edge_renderer.data_source.data['line_dash'] = edge_line_dashes
    
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
    
    # Configure edge appearance with cost-based colors and line dash
    graph_renderer.edge_renderer.glyph = MultiLine(
        line_color="edge_color",
        line_dash="line_dash",
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


def create_solution_plot(G: nx.Graph, solution_edges: list, layout: Dict, title: str = "Solution Tree") -> figure:
    """
    Create a Bokeh plot for the solution tree.
    
    Args:
        G: NetworkX graph with all nodes and edges
        solution_edges: List of edges in the solution (dicts with 'source' and 'destination')
        layout: Node position layout (same as main graph)
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
    plot.background_fill_color = "#f0f8ff"  # Light blue background for solution
    plot.grid.grid_line_color = None
    plot.axis.visible = False
    
    # Create solution graph with only selected edges
    solution_graph = nx.Graph()
    solution_graph.add_nodes_from(G.nodes())
    
    # Add only the edges from the solution
    for edge in solution_edges:
        u, v = edge['source'], edge['destination']
        # Get cost from original graph
        if G.has_edge(u, v):
            cost = G[u][v]['cost']
        elif G.has_edge(v, u):
            cost = G[v][u]['cost']
        else:
            cost = 0  # Fallback
        solution_graph.add_edge(u, v, cost=cost)
    
    # Create graph renderer with provided layout
    graph_renderer = from_networkx(solution_graph, layout, scale=1, center=(0, 0))
    
    # Add cost data for hover
    if solution_graph.edges():
        edge_costs = [solution_graph[u][v]['cost'] for u, v in solution_graph.edges()]
        edge_colors = ["#27ae60"] * len(list(solution_graph.edges()))  # Green for solution
        graph_renderer.edge_renderer.data_source.data['cost'] = edge_costs
        graph_renderer.edge_renderer.data_source.data['edge_color'] = edge_colors
    
    # Configure node appearance (solution nodes in green)
    graph_renderer.node_renderer.glyph = Circle(
        radius=0.05,
        fill_color="#27ae60",
        line_color="#229954",
        line_width=2
    )
    graph_renderer.node_renderer.hover_glyph = Circle(
        radius=0.05,
        fill_color="#229954",
        line_color="#1e8449",
        line_width=3
    )
    
    # Configure edge appearance (solution edges in green)
    graph_renderer.edge_renderer.glyph = MultiLine(
        line_color="edge_color",
        line_alpha=0.9,
        line_width=4
    )
    graph_renderer.edge_renderer.hover_glyph = MultiLine(
        line_color="#1e8449",
        line_alpha=1.0,
        line_width=6
    )
    
    # Add hover tool for nodes
    node_hover = HoverTool(
        tooltips=[("Node ID", "@index")],
        renderers=[graph_renderer.node_renderer]
    )
    plot.add_tools(node_hover)
    
    # Add hover tool for edges
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


def update_solution_plot(plot: figure, G: nx.Graph, solution_edges: list, layout: Dict, title: str):
    """
    Update solution plot with new data.
    
    Args:
        plot: Existing Bokeh figure
        G: NetworkX graph with all nodes and edges
        solution_edges: List of edges in the solution
        layout: Node position layout
        title: New title
    """
    # Clear existing renderers and hover tools
    plot.renderers = []
    plot.tools = [t for t in plot.tools if not isinstance(t, HoverTool)]
    
    # Update title
    plot.title.text = title
    
    # Create solution graph with only selected edges
    solution_graph = nx.Graph()
    solution_graph.add_nodes_from(G.nodes())
    
    # Add only the edges from the solution
    for edge in solution_edges:
        u, v = edge['source'], edge['destination']
        # Get cost from original graph
        if G.has_edge(u, v):
            cost = G[u][v]['cost']
        elif G.has_edge(v, u):
            cost = G[v][u]['cost']
        else:
            cost = 0  # Fallback
        solution_graph.add_edge(u, v, cost=cost)
    
    # Create graph renderer with provided layout
    graph_renderer = from_networkx(solution_graph, layout, scale=1, center=(0, 0))
    
    # Add cost data for hover
    if solution_graph.edges():
        edge_costs = [solution_graph[u][v]['cost'] for u, v in solution_graph.edges()]
        edge_colors = ["#27ae60"] * len(list(solution_graph.edges()))  # Green for solution
        graph_renderer.edge_renderer.data_source.data['cost'] = edge_costs
        graph_renderer.edge_renderer.data_source.data['edge_color'] = edge_colors
    
    # Configure node appearance (solution nodes in green)
    graph_renderer.node_renderer.glyph = Circle(
        radius=0.05,
        fill_color="#27ae60",
        line_color="#229954",
        line_width=2
    )
    graph_renderer.node_renderer.hover_glyph = Circle(
        radius=0.05,
        fill_color="#229954",
        line_color="#1e8449",
        line_width=3
    )
    
    # Configure edge appearance (solution edges in green)
    graph_renderer.edge_renderer.glyph = MultiLine(
        line_color="edge_color",
        line_alpha=0.9,
        line_width=4
    )
    graph_renderer.edge_renderer.hover_glyph = MultiLine(
        line_color="#1e8449",
        line_alpha=1.0,
        line_width=6
    )
    
    # Add hover tool for nodes
    node_hover = HoverTool(
        tooltips=[("Node ID", "@index")],
        renderers=[graph_renderer.node_renderer]
    )
    plot.add_tools(node_hover)
    
    # Add hover tool for edges
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
