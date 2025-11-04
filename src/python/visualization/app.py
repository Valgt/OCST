#!/usr/bin/env python3
"""
OCST Instance Visualizer - Bokeh Server Application

Interactive visualization tool for exploring OCST problem instances.

Usage:
    bokeh serve src/python/visualization/app.py --show
    
Then navigate to: http://localhost:5006/app
"""

from pathlib import Path
from bokeh.layouts import column, row, gridplot
from bokeh.models import Select, Div, Button, Spacer, CheckboxGroup
from bokeh.plotting import curdoc
import random

from graph_loader import get_available_instances, load_instance, load_solution
from graph_renderer import (
    create_graph_plot, update_graph_plot,
    create_requirements_plot, compute_spring_layout, compute_hierarchical_layout,
    accumulate_bidirectional_requirements,
    create_solution_plot, update_solution_plot,
    find_path_in_tree,
    compute_node_to_requirements_mapping
)


# Configuration
DATA_DIR = Path(__file__).parent.parent.parent.parent / "data" / "input"
DATA_OUTPUT_DIR = Path(__file__).parent.parent.parent.parent / "data" / "output" / "test_instances"

# Global state
current_instance = None
current_solution = None
current_graph_plot = None
current_req_plot = None
current_solution_plot = None
current_layout = None
highlight_solution = False
layout_counter = 0  # For cycling through different hierarchical layouts
selected_requirement = None  # Currently selected requirement (origin, destination)
node_to_requirements = {}  # Mapping of node_id -> list of requirements passing through it
highlighted_requirements = []  # Currently highlighted requirements in req plot


def get_instance_options():
    """Get list of available instances for dropdown."""
    instances = get_available_instances(DATA_DIR)
    return [f.stem for f in instances]


def create_info_panel(instance):
    """Create information panel showing instance details."""
    graph = instance.graph
    requirements = instance.get_requirements()
    metadata = instance.get_metadata()
    tags = instance.get_tags()
    
    # Compute edge cost statistics
    costs = [graph[u][v]['cost'] for u, v in graph.edges()]
    zero_cost_edges = sum(1 for c in costs if c == 0)
    min_cost = min(costs) if costs else 0
    max_cost = max(costs) if costs else 0
    avg_cost = sum(costs) / len(costs) if costs else 0
    
    # Compute accumulated requirements
    accumulated_reqs = accumulate_bidirectional_requirements(requirements)
    total_accumulated_weight = sum(w for _, _, w in accumulated_reqs)
    
    info_html = f"""
    <div style="background-color: #ecf0f1; padding: 15px; border-radius: 5px; margin-bottom: 10px;">
        <h3 style="margin-top: 0; color: #2c3e50;">Instance: {instance.name}</h3>
        
        <h4 style="color: #34495e;">Graph Properties:</h4>
        <ul style="margin: 5px 0;">
            <li><b>Nodes:</b> {graph.number_of_nodes()}</li>
            <li><b>Edges:</b> {graph.number_of_edges()}</li>
            <li><b>Density:</b> {nx.density(graph):.3f}</li>
            <li><b>Connected:</b> {"Yes" if nx.is_connected(graph) else "No"}</li>
        </ul>
        
        <h4 style="color: #34495e;">Edge Costs:</h4>
        <ul style="margin: 5px 0;">
            <li><b>Min:</b> {min_cost:.2f} <span style="color: #c8c8c8;">●</span> (light)</li>
            <li><b>Max:</b> {max_cost:.2f} <span style="color: #323232;">●</span> (dark)</li>
            <li><b>Avg:</b> {avg_cost:.2f}</li>
            <li><b>Free (cost=0):</b> {zero_cost_edges} <span style="color: #27ae60;">●</span></li>
        </ul>
        
        <h4 style="color: #34495e;">Requirements:</h4>
        <ul style="margin: 5px 0;">
            <li><b>Original:</b> {len(requirements)}</li>
            <li><b>Accumulated:</b> {len(accumulated_reqs)}</li>
            <li><b>Total Weight:</b> {total_accumulated_weight:.2f}</li>
        </ul>
        
        <h4 style="color: #34495e;">Metadata:</h4>
        <ul style="margin: 5px 0;">
            <li><b>Tags:</b> {", ".join(tags) if tags else "None"}</li>
            <li><b>Source:</b> {metadata.get("source", "Unknown")}</li>
        </ul>
    </div>
    """
    
    return Div(text=info_html, width=300)


def on_instance_change(attr, old, new):
    """Callback when user selects a different instance."""
    global current_instance, current_solution, current_graph_plot, current_req_plot, current_solution_plot, current_layout, selected_requirement
    
    # Load new instance
    instance_path = DATA_DIR / f"{new}.json"
    current_instance = load_instance(instance_path)
    
    # Load solution (if available)
    solution_path = DATA_OUTPUT_DIR / f"{new}.results.json"
    if solution_path.exists():
        current_solution = load_solution(solution_path)
    else:
        current_solution = None
    
    # Reset selected requirement
    selected_requirement = None
    
    # Compute new layout (hierarchical if solution available, spring otherwise)
    if current_solution and 'tree_edges' in current_solution:
        current_layout = compute_hierarchical_layout(current_instance.graph, current_solution['tree_edges'])
    else:
        current_layout = compute_spring_layout(current_instance.graph)
    
    # Update graph plot (edges with costs) - using shared layout
    solution_edges = current_solution.get('tree_edges', []) if current_solution else []
    graph_title = f"Graph: {current_instance.name}"
    update_graph_plot_with_layout(current_graph_plot, current_instance.graph, current_layout, graph_title, solution_edges, highlight_solution)
    
    # Update requirements plot
    req_title = f"Requirements: {current_instance.name}"
    requirements = current_instance.get_requirements()
    update_requirements_plot(current_req_plot, current_instance.graph, requirements, current_layout, req_title)
    
    # Re-add midpoint nodes for new instance (note: this adds to existing renderers)
    # TODO: Should clear old midpoint renderers first, but for now they'll be replaced
    from req_hover_helper import add_requirement_midpoint_nodes
    midpoint_source, midpoint_renderer = add_requirement_midpoint_nodes(
        current_req_plot,
        None,
        current_layout,
        None,
        requirements,
        current_solution.get('tree_edges', []) if current_solution else []
    )
    
    # Reconnect callback
    def on_midpoint_click_updated(attr, old, new):
        global current_solution, current_solution_plot, current_layout, selected_requirement, highlighted_requirements, current_req_plot
        if not new or current_solution is None:
            if current_solution and 'tree_edges' in current_solution:
                solution_title = f"Solution: {current_instance.name} (Cost: {current_solution.get('tree_cost', 'N/A')})"
                update_solution_plot(current_solution_plot, current_instance.graph, 
                                   current_solution['tree_edges'], current_layout, solution_title, None)
            selected_requirement = None
            return
        try:
            # Clear node-based requirement highlighting (inverse interaction)
            highlighted_requirements = []
            highlight_requirements_in_plot(current_req_plot, [])
            
            # Clear solution tree node selection
            try:
                node_source = current_solution_plot.renderers[0].node_renderer.data_source
                node_source.selected.indices = []
            except:
                pass
            
            idx = new[0]
            origin = midpoint_source.data['origin'][idx]
            destination = midpoint_source.data['destination'][idx]
            selected_requirement = (origin, destination)
            solution_edges = current_solution.get('tree_edges', [])
            highlight_path = find_path_in_tree(solution_edges, origin, destination)
            solution_title = f"Solution: {current_instance.name} (Cost: {current_solution.get('tree_cost', 'N/A')})"
            update_solution_plot(current_solution_plot, current_instance.graph, 
                               current_solution['tree_edges'], current_layout, solution_title, highlight_path)
            
            # Reconnect solution node callback after updating (helper handles cleanup)
            reconnect_solution_node_callback()
        except Exception as e:
            print(f"Error: {e}")
            import traceback
            traceback.print_exc()
    
    midpoint_source.selected.on_change('indices', on_midpoint_click_updated)
    
    # Update solution plot (no highlighting since requirement was reset)
    if current_solution and 'tree_edges' in current_solution:
        solution_title = f"Solution: {current_instance.name} (Cost: {current_solution.get('tree_cost', 'N/A')})"
        update_solution_plot(current_solution_plot, current_instance.graph, current_solution['tree_edges'], current_layout, solution_title, None)
        
        # Recompute node-to-requirements mapping
        global node_to_requirements
        all_nodes = list(current_instance.graph.nodes())
        node_to_requirements = compute_node_to_requirements_mapping(
            requirements,
            current_solution['tree_edges'],
            all_nodes
        )
        
        # Reconnect callback to solution plot nodes (helper handles cleanup)
        reconnect_solution_node_callback()
    
    # Update info panel (layout is now: row(plots, controls))
    # controls = column(instance_select, reorganize_btn, highlight_checkbox, info_panel)
    new_info_panel = create_info_panel(current_instance)
    layout.children[1].children[3] = new_info_panel


def on_reorganize_click():
    """Callback when reorganize button is clicked."""
    global current_instance, current_solution, current_graph_plot, current_req_plot, current_solution_plot, current_layout, layout_counter, selected_requirement
    
    if current_instance is None:
        return
    
    # Increment counter for variety
    layout_counter += 1
    
    # Compute new layout
    if current_solution and 'tree_edges' in current_solution:
        # Use hierarchical layout with different root
        current_layout = compute_hierarchical_layout(current_instance.graph, current_solution['tree_edges'], layout_counter)
    else:
        # Use spring layout with new random seed
        new_seed = random.randint(1, 10000)
        current_layout = compute_spring_layout(current_instance.graph, seed=new_seed)
    
    # Update all plots with new layout
    solution_edges = current_solution.get('tree_edges', []) if current_solution else []
    graph_title = f"Graph: {current_instance.name}"
    update_graph_plot_with_layout(current_graph_plot, current_instance.graph, current_layout, graph_title, solution_edges, highlight_solution)
    
    req_title = f"Requirements: {current_instance.name}"
    requirements = current_instance.get_requirements()
    update_requirements_plot(current_req_plot, current_instance.graph, requirements, current_layout, req_title)
    
    # Recompute highlight path if a requirement is selected
    highlight_path = None
    if selected_requirement and current_solution and 'tree_edges' in current_solution:
        origin, destination = selected_requirement
        highlight_path = find_path_in_tree(current_solution['tree_edges'], origin, destination)
    
    if current_solution and 'tree_edges' in current_solution:
        solution_title = f"Solution: {current_instance.name} (Cost: {current_solution.get('tree_cost', 'N/A')})"
        update_solution_plot(current_solution_plot, current_instance.graph, current_solution['tree_edges'], current_layout, solution_title, highlight_path)


def reconnect_solution_node_callback():
    """
    Safely reconnect the solution node click callback.
    This is needed after update_solution_plot() destroys renderers.
    Removes old callback first to avoid accumulation.
    """
    global current_solution_plot
    try:
        node_source = current_solution_plot.renderers[0].node_renderer.data_source
        # Remove old callback if exists (prevents accumulation)
        try:
            node_source.selected.remove_on_change('indices', on_solution_node_click)
        except:
            pass  # First time, no callback to remove
        # Add fresh callback
        node_source.selected.on_change('indices', on_solution_node_click)
    except Exception as e:
        print(f"Warning: Could not reconnect solution node callback: {e}")


def highlight_requirements_in_plot(req_plot, requirements_to_highlight):
    """
    Highlight specific requirements in the requirements plot.
    
    Args:
        req_plot: Requirements plot figure
        requirements_to_highlight: List of (origin, dest) tuples to highlight
    """
    try:
        # Access the edge renderer
        edge_renderer = req_plot.renderers[0].edge_renderer
        edge_source = edge_renderer.data_source
        
        # Get current edge data
        origins = edge_source.data.get('origin', [])
        destinations = edge_source.data.get('destination', [])
        default_colors = edge_source.data.get('edge_color', [])
        default_dashes = edge_source.data.get('line_dash', [])
        
        # Create set of requirements to highlight (canonical form)
        highlight_set = set()
        for origin, dest in requirements_to_highlight:
            highlight_set.add((min(origin, dest), max(origin, dest)))
        
        # Update colors and widths for highlighted requirements
        new_colors = []
        new_widths = []
        new_alphas = []
        
        for i in range(len(origins)):
            canonical = (min(origins[i], destinations[i]), max(origins[i], destinations[i]))
            
            if canonical in highlight_set:
                # Highlight: bright blue
                new_colors.append("#3498db")
                new_widths.append(6)  # Thicker
                new_alphas.append(1.0)  # Full opacity
            else:
                # Normal: use default color
                new_colors.append(default_colors[i] if i < len(default_colors) else "#808080")
                new_widths.append(3)
                new_alphas.append(0.3)  # Dimmed
        
        # Update the data source
        edge_source.data['edge_color'] = new_colors
        edge_source.data['edge_width'] = new_widths
        edge_source.data['edge_alpha'] = new_alphas
        
    except Exception as e:
        print(f"Error highlighting requirements: {e}")
        import traceback
        traceback.print_exc()


def on_solution_node_click(attr, old, new):
    """Callback when user clicks on a node in the solution tree."""
    global current_instance, current_solution, current_req_plot, current_solution_plot, node_to_requirements, highlighted_requirements, selected_requirement
    
    if current_instance is None or current_solution is None:
        return
    
    try:
        # Access the node renderer's data source from solution plot
        node_source = current_solution_plot.renderers[0].node_renderer.data_source
        selected_indices = node_source.selected.indices
        
        if not selected_indices:
            # No node selected - clear highlights
            highlighted_requirements = []
            highlight_requirements_in_plot(current_req_plot, [])
            selected_requirement = None
        else:
            # Clear requirement path highlighting (inverse interaction)
            selected_requirement = None
            solution_title = f"Solution: {current_instance.name} (Cost: {current_solution.get('tree_cost', 'N/A')})"
            update_solution_plot(current_solution_plot, current_instance.graph, 
                               current_solution['tree_edges'], current_layout, solution_title, None)
            
            # Reconnect this callback after updating plot (helper handles cleanup)
            reconnect_solution_node_callback()
            
            # Clear midpoint selection from requirements plot
            try:
                # Find midpoint renderer (should be last renderer added)
                for renderer in current_req_plot.renderers:
                    if hasattr(renderer, 'data_source') and 'origin' in renderer.data_source.data:
                        renderer.data_source.selected.indices = []
            except:
                pass
            
            # Get the selected node ID
            node_idx = selected_indices[0]
            node_ids = list(node_source.data['index'])
            selected_node = node_ids[node_idx]
            
            # Get requirements passing through this node
            reqs_through_node = node_to_requirements.get(selected_node, [])
            highlighted_requirements = reqs_through_node
            
            # Highlight these requirements in the requirements plot
            highlight_requirements_in_plot(current_req_plot, reqs_through_node)
            
            print(f"Node {selected_node} selected: {len(reqs_through_node)} requirements pass through it")
    
    except Exception as e:
        print(f"Error in solution node click handler: {e}")
        import traceback
        traceback.print_exc()


def on_highlight_change(attr, old, new):
    """Callback when highlight checkbox changes."""
    global current_instance, current_solution, current_graph_plot, current_layout, highlight_solution
    
    if current_instance is None:
        return
    
    # Update highlight state
    highlight_solution = len(new) > 0  # True if checkbox is checked
    
    # Redraw graph plot with/without highlighting
    solution_edges = current_solution.get('tree_edges', []) if current_solution else []
    graph_title = f"Graph: {current_instance.name}"
    update_graph_plot_with_layout(current_graph_plot, current_instance.graph, current_layout, graph_title, solution_edges, highlight_solution)


def compute_edge_colors_with_solution(G, solution_edges, attribute='cost'):
    """
    Compute edge colors highlighting solution edges in green, others in gray.
    Green intensity based on cost (darker = higher cost).
    
    Args:
        G: NetworkX graph
        solution_edges: List of dicts with 'source' and 'destination' keys
        attribute: Edge attribute to use for coloring
        
    Returns:
        Tuple of (colors list, line_dash list)
    """
    # Create set of solution edges (undirected)
    solution_set = set()
    for edge in solution_edges:
        u, v = edge['source'], edge['destination']
        solution_set.add((min(u, v), max(u, v)))
    
    # Get all edge values
    edges_list = list(G.edges())
    values = [G[u][v][attribute] for u, v in edges_list]
    
    if not values:
        return [], []
    
    colors = []
    line_dashes = []
    
    # Separate solution and non-solution edges for normalization
    solution_values = []
    non_solution_values = []
    
    for i, (u, v) in enumerate(edges_list):
        canonical = (min(u, v), max(u, v))
        value = values[i]
        if canonical in solution_set and value > 0:
            solution_values.append(value)
        elif value > 0:
            non_solution_values.append(value)
    
    # Normalize solution edges (green scale)
    min_sol = min(solution_values) if solution_values else 0
    max_sol = max(solution_values) if solution_values else 0
    
    # Normalize non-solution edges (gray scale)
    min_non = min(non_solution_values) if non_solution_values else 0
    max_non = max(non_solution_values) if non_solution_values else 0
    
    for i, (u, v) in enumerate(edges_list):
        canonical = (min(u, v), max(u, v))
        value = values[i]
        
        if canonical in solution_set:
            # Solution edge = green with intensity based on cost
            if value == 0 or abs(value) < 1e-9:
                # Zero cost solution edge = light green + dashed
                colors.append("#52c985")  # Light green
                line_dashes.append("dashed")
            elif max_sol == min_sol:
                colors.append("#27ae60")  # Medium green
                line_dashes.append("solid")
            else:
                norm = (value - min_sol) / (max_sol - min_sol)
                # Map [0, 1] to [light green, dark green]
                # Light: #52c985, Dark: #1e7e34
                intensity_r = int(82 - norm * 52)   # 82 -> 30
                intensity_g = int(201 - norm * 75)  # 201 -> 126
                intensity_b = int(133 - norm * 81)  # 133 -> 52
                colors.append(f"#{intensity_r:02x}{intensity_g:02x}{intensity_b:02x}")
                line_dashes.append("solid")
        elif value == 0 or abs(value) < 1e-9:
            # Non-solution zero cost = light gray + dashed
            colors.append("#c8c8c8")
            line_dashes.append("dashed")
        else:
            # Non-solution edge = gray with intensity
            if max_non == min_non:
                colors.append("#a0a0a0")  # Medium gray
            else:
                norm = (value - min_non) / (max_non - min_non)
                intensity = int(200 - norm * 150)  # Light to dark gray
                colors.append(f"#{intensity:02x}{intensity:02x}{intensity:02x}")
            line_dashes.append("solid")
    
    return colors, line_dashes


def create_graph_plot_with_layout(G, layout, title, solution_edges=None, highlight=False):
    """Create graph plot using a specific layout."""
    from bokeh.models import Circle, MultiLine, HoverTool, BoxZoomTool, ResetTool, WheelZoomTool, PanTool
    from bokeh.plotting import figure, from_networkx
    from graph_renderer import compute_edge_colors_and_styles
    
    plot = figure(
        title=title,
        width=900,
        height=700,
        x_range=(-1.2, 1.2),
        y_range=(-1.2, 1.2),
        toolbar_location="above",
        tools=""
    )
    
    plot.add_tools(PanTool(), WheelZoomTool(), BoxZoomTool(), ResetTool())
    plot.background_fill_color = "#f5f5f5"
    plot.grid.grid_line_color = None
    plot.axis.visible = False
    
    # Compute edge colors and styles
    if highlight and solution_edges:
        edge_colors, edge_line_dashes = compute_edge_colors_with_solution(G, solution_edges, attribute='cost')
    else:
        edge_colors, edge_line_dashes = compute_edge_colors_and_styles(G, attribute='cost')
    
    for i, (u, v) in enumerate(G.edges()):
        G[u][v]['edge_color'] = edge_colors[i] if edge_colors else "#95a5a6"
        G[u][v]['line_dash'] = edge_line_dashes[i] if edge_line_dashes else "solid"
    
    # Create renderer with provided layout
    graph_renderer = from_networkx(G, layout, scale=1, center=(0, 0))
    
    # Add cost data
    edge_costs = [G[u][v]['cost'] for u, v in G.edges()]
    graph_renderer.edge_renderer.data_source.data['cost'] = edge_costs
    graph_renderer.edge_renderer.data_source.data['edge_color'] = edge_colors
    graph_renderer.edge_renderer.data_source.data['line_dash'] = edge_line_dashes
    
    # Configure appearance
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
    
    # Add hover tools
    node_hover = HoverTool(
        tooltips=[("Node ID", "@index")],
        renderers=[graph_renderer.node_renderer]
    )
    plot.add_tools(node_hover)
    
    edge_hover = HoverTool(
        tooltips=[
            ("Cost", "@cost{0.00}"),
            ("Edge", "(@start, @end)")
        ],
        renderers=[graph_renderer.edge_renderer]
    )
    plot.add_tools(edge_hover)
    
    plot.renderers.append(graph_renderer)
    return plot


def update_graph_plot_with_layout(plot, G, layout, title, solution_edges=None, highlight=False):
    """Update graph plot with new data using provided layout."""
    from bokeh.models import HoverTool, Circle, MultiLine
    from bokeh.plotting import from_networkx
    from graph_renderer import compute_edge_colors_and_styles
    
    # Clear existing renderers and hover tools
    plot.renderers = []
    plot.tools = [t for t in plot.tools if not isinstance(t, HoverTool)]
    
    # Update title
    plot.title.text = title
    
    # Compute edge colors and styles
    if highlight and solution_edges:
        edge_colors, edge_line_dashes = compute_edge_colors_with_solution(G, solution_edges, attribute='cost')
    else:
        edge_colors, edge_line_dashes = compute_edge_colors_and_styles(G, attribute='cost')
    
    for i, (u, v) in enumerate(G.edges()):
        G[u][v]['edge_color'] = edge_colors[i] if edge_colors else "#95a5a6"
        G[u][v]['line_dash'] = edge_line_dashes[i] if edge_line_dashes else "solid"
    
    # Create renderer with provided layout
    graph_renderer = from_networkx(G, layout, scale=1, center=(0, 0))
    
    # Add cost data
    edge_costs = [G[u][v]['cost'] for u, v in G.edges()]
    graph_renderer.edge_renderer.data_source.data['cost'] = edge_costs
    graph_renderer.edge_renderer.data_source.data['edge_color'] = edge_colors
    graph_renderer.edge_renderer.data_source.data['line_dash'] = edge_line_dashes
    
    # Configure appearance
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
    
    # Add hover tools
    node_hover = HoverTool(
        tooltips=[("Node ID", "@index")],
        renderers=[graph_renderer.node_renderer]
    )
    plot.add_tools(node_hover)
    
    edge_hover = HoverTool(
        tooltips=[
            ("Cost", "@cost{0.00}"),
            ("Edge", "(@start, @end)")
        ],
        renderers=[graph_renderer.edge_renderer]
    )
    plot.add_tools(edge_hover)
    
    plot.renderers.append(graph_renderer)


def update_requirements_plot(plot, G, requirements, layout, title):
    """Update requirements plot with new data."""
    from bokeh.models import HoverTool, Circle, MultiLine
    import networkx as nx
    from bokeh.plotting import from_networkx
    from graph_renderer import compute_edge_colors_and_styles
    
    # Clear existing renderers and hover tools
    plot.renderers = []
    plot.tools = [t for t in plot.tools if not isinstance(t, HoverTool)]
    
    # Update title
    plot.title.text = title
    
    # Accumulate bidirectional requirements and filter zeros
    accumulated_reqs = accumulate_bidirectional_requirements(requirements)
    
    # Create requirements graph
    req_graph = nx.Graph()
    req_graph.add_nodes_from(G.nodes())
    
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
    
    # Create renderer
    graph_renderer = from_networkx(req_graph, layout, scale=1, center=(0, 0))
    
    # Add data and edge info for interaction
    if req_graph.edges():
        edge_list = list(req_graph.edges())
        edge_weights = [req_graph[u][v]['weight'] for u, v in edge_list]
        edge_colors = [req_graph[u][v].get('edge_color', '#808080') for u, v in edge_list]
        edge_line_dashes = [req_graph[u][v].get('line_dash', 'solid') for u, v in edge_list]
        # Store origin and destination for click handling
        edge_origins = [u for u, v in edge_list]
        edge_destinations = [v for u, v in edge_list]
        
        graph_renderer.edge_renderer.data_source.data['weight'] = edge_weights
        graph_renderer.edge_renderer.data_source.data['edge_color'] = edge_colors
        graph_renderer.edge_renderer.data_source.data['line_dash'] = edge_line_dashes
        graph_renderer.edge_renderer.data_source.data['origin'] = edge_origins
        graph_renderer.edge_renderer.data_source.data['destination'] = edge_destinations
        graph_renderer.edge_renderer.data_source.data['edge_width'] = [3] * len(edge_list)  # Default width
        graph_renderer.edge_renderer.data_source.data['edge_alpha'] = [0.8] * len(edge_list)  # Default alpha
    
    # Configure appearance (RED nodes for requirements)
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
    
    # Configure edge appearance (grayscale based on weight, dashed for zero-weight, dynamic width/alpha)
    graph_renderer.edge_renderer.glyph = MultiLine(
        line_color="edge_color",
        line_dash="line_dash",
        line_alpha="edge_alpha",  # Use dynamic alpha
        line_width="edge_width"   # Use dynamic width
    )
    graph_renderer.edge_renderer.hover_glyph = MultiLine(
        line_color="#e74c3c",
        line_alpha=1.0,
        line_width=5
    )
    
    # Add hover tools
    node_hover = HoverTool(
        tooltips=[("Node ID", "@index")],
        renderers=[graph_renderer.node_renderer]
    )
    plot.add_tools(node_hover)
    
    edge_hover = HoverTool(
        tooltips=[
            ("Weight", "@weight{0.00}"),
            ("Requirement", "(@start, @end)")
        ],
        renderers=[graph_renderer.edge_renderer]
    )
    plot.add_tools(edge_hover)
    
    plot.renderers.append(graph_renderer)


# Initialize application
def initialize_app():
    """Initialize the Bokeh application."""
    global current_instance, current_solution, current_graph_plot, current_req_plot, current_solution_plot, current_layout, layout
    
    # Get available instances
    instance_options = get_instance_options()
    
    if not instance_options:
        # No instances found
        error_div = Div(text="<h2>Error: No instances found in data/input/</h2>")
        curdoc().add_root(error_div)
        return
    
    # Load first instance
    first_instance = instance_options[0]
    instance_path = DATA_DIR / f"{first_instance}.json"
    current_instance = load_instance(instance_path)
    
    # Load solution (if available)
    solution_path = DATA_OUTPUT_DIR / f"{first_instance}.results.json"
    if solution_path.exists():
        current_solution = load_solution(solution_path)
    else:
        current_solution = None
    
    # Create dropdown for instance selection
    instance_select = Select(
        title="Select Instance:",
        value=first_instance,
        options=instance_options,
        width=300
    )
    instance_select.on_change('value', on_instance_change)
    
    # Create reorganize button
    reorganize_btn = Button(
        label="🔄 Reorganize Layout",
        button_type="primary",
        width=300
    )
    reorganize_btn.on_click(on_reorganize_click)
    
    # Create checkbox for highlighting OCST solution
    highlight_checkbox = CheckboxGroup(
        labels=["Marcar OCST"],
        active=[],
        width=300
    )
    highlight_checkbox.on_change('active', on_highlight_change)
    
    # Compute layout once (shared by all plots) - hierarchical if solution available
    if current_solution and 'tree_edges' in current_solution:
        current_layout = compute_hierarchical_layout(current_instance.graph, current_solution['tree_edges'])
    else:
        current_layout = compute_spring_layout(current_instance.graph)
    
    # Create requirements plot (top-left)
    req_title = f"Requirements: {current_instance.name}"
    requirements = current_instance.get_requirements()
    current_req_plot = create_requirements_plot(
        current_instance.graph, 
        requirements, 
        current_layout,
        req_title
    )
    
    # Add invisible clickable nodes at midpoints of requirement edges
    from req_hover_helper import add_requirement_midpoint_nodes
    midpoint_source, midpoint_renderer = add_requirement_midpoint_nodes(
        current_req_plot,
        None,  # Will create internally
        current_layout,
        None,  # Solution plot reference (set later)
        requirements,
        current_solution.get('tree_edges', []) if current_solution else []
    )
    
    # Connect selection callback to midpoint nodes
    def on_requirement_midpoint_click(attr, old, new):
        """Callback when user clicks on a requirement midpoint node."""
        global current_solution, current_solution_plot, current_layout, selected_requirement, highlighted_requirements, current_req_plot
        
        if not new or current_solution is None:
            # No selection or no solution - clear highlight
            if current_solution and 'tree_edges' in current_solution:
                solution_title = f"Solution: {current_instance.name} (Cost: {current_solution.get('tree_cost', 'N/A')})"
                update_solution_plot(current_solution_plot, current_instance.graph, 
                                   current_solution['tree_edges'], current_layout, solution_title, None)
            selected_requirement = None
            return
        
        try:
            # Clear node-based requirement highlighting (inverse interaction)
            highlighted_requirements = []
            highlight_requirements_in_plot(current_req_plot, [])
            
            # Clear solution tree node selection
            try:
                node_source = current_solution_plot.renderers[0].node_renderer.data_source
                node_source.selected.indices = []
            except:
                pass
            
            # Get the selected midpoint
            idx = new[0]
            origin = midpoint_source.data['origin'][idx]
            destination = midpoint_source.data['destination'][idx]
            selected_requirement = (origin, destination)
            
            # Find path in solution tree
            solution_edges = current_solution.get('tree_edges', [])
            highlight_path = find_path_in_tree(solution_edges, origin, destination)
            
            # Redraw solution plot with highlighting
            solution_title = f"Solution: {current_instance.name} (Cost: {current_solution.get('tree_cost', 'N/A')})"
            update_solution_plot(current_solution_plot, current_instance.graph, 
                               current_solution['tree_edges'], current_layout, solution_title, highlight_path)
            
            # Reconnect solution node callback after updating (helper handles cleanup)
            reconnect_solution_node_callback()
                
        except Exception as e:
            print(f"Error in requirement midpoint click handler: {e}")
            import traceback
            traceback.print_exc()
    
    midpoint_source.selected.on_change('indices', on_requirement_midpoint_click)
    
    # Create graph plot (top-right) - using the same layout
    solution_edges = current_solution.get('tree_edges', []) if current_solution else []
    graph_title = f"Graph: {current_instance.name}"
    current_graph_plot = create_graph_plot_with_layout(
        current_instance.graph, 
        current_layout, 
        graph_title,
        solution_edges,
        highlight_solution
    )
    
    # Create solution plot (bottom-right) - using the same layout
    if current_solution and 'tree_edges' in current_solution:
        solution_title = f"Solution: {current_instance.name} (Cost: {current_solution.get('tree_cost', 'N/A')})"
        current_solution_plot = create_solution_plot(
            current_instance.graph,
            current_solution['tree_edges'],
            current_layout,
            solution_title
        )
        
        # Compute node-to-requirements mapping for bidirectional interaction
        global node_to_requirements
        all_nodes = list(current_instance.graph.nodes())
        node_to_requirements = compute_node_to_requirements_mapping(
            requirements,
            current_solution['tree_edges'],
            all_nodes
        )
        
        # Make solution plot nodes clickable
        from bokeh.models import TapTool
        tap_tool = TapTool()
        current_solution_plot.add_tools(tap_tool)
        
        # Connect click callback to solution plot nodes (using helper for consistency)
        reconnect_solution_node_callback()
        
    else:
        # Create empty spacer if no solution
        current_solution_plot = Div(text="<h3>No solution available</h3>", width=900, height=700)
    
    # Create info panel
    info_panel = create_info_panel(current_instance)
    
    # Create 2x2 layout:
    # Top row: requirements (clickable) | graph
    # Bottom row: spacer | solution (shows traced paths)
    # Right sidebar: controls
    # Note: Click on requirement edges to trace their path in the solution tree
    top_row = row(current_req_plot, current_graph_plot)
    bottom_row = row(Spacer(width=900, height=700), current_solution_plot)
    plots = column(top_row, bottom_row)
    controls = column(instance_select, reorganize_btn, highlight_checkbox, info_panel)
    layout = row(plots, controls)
    
    # Add to document
    curdoc().add_root(layout)
    curdoc().title = "OCST Instance Visualizer"


# Import networkx here to avoid circular import
import networkx as nx

# Run initialization
initialize_app()

