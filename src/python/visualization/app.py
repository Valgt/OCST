#!/usr/bin/env python3
"""
OCST Instance Visualizer - Bokeh Server Application

Interactive visualization tool for exploring OCST problem instances.

Usage:
    bokeh serve src/python/visualization/app.py --show
    
Then navigate to: http://localhost:5006/app
"""

from pathlib import Path
from bokeh.layouts import column, row
from bokeh.models import Select, Div, Button
from bokeh.plotting import curdoc
import random

from graph_loader import get_available_instances, load_instance
from graph_renderer import (
    create_graph_plot, update_graph_plot,
    create_requirements_plot, compute_spring_layout
)


# Configuration
DATA_DIR = Path(__file__).parent.parent.parent.parent / "data" / "input"

# Global state
current_instance = None
current_graph_plot = None
current_req_plot = None
current_layout = None


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
    min_cost = min(costs) if costs else 0
    max_cost = max(costs) if costs else 0
    avg_cost = sum(costs) / len(costs) if costs else 0
    
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
        </ul>
        
        <h4 style="color: #34495e;">Requirements:</h4>
        <ul style="margin: 5px 0;">
            <li><b>Count:</b> {len(requirements)}</li>
            <li><b>Total Weight:</b> {sum(r[2] for r in requirements):.2f}</li>
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
    global current_instance, current_graph_plot, current_req_plot, current_layout
    
    # Load new instance
    instance_path = DATA_DIR / f"{new}.json"
    current_instance = load_instance(instance_path)
    
    # Compute new layout (consistent for both plots)
    current_layout = compute_spring_layout(current_instance.graph)
    
    # Update graph plot (edges with costs) - using shared layout
    graph_title = f"Graph: {current_instance.name}"
    update_graph_plot_with_layout(current_graph_plot, current_instance.graph, current_layout, graph_title)
    
    # Update requirements plot
    req_title = f"Requirements: {current_instance.name}"
    requirements = current_instance.get_requirements()
    update_requirements_plot(current_req_plot, current_instance.graph, requirements, current_layout, req_title)
    
    # Update info panel
    new_info_panel = create_info_panel(current_instance)
    layout.children[2].children[2] = new_info_panel


def on_reorganize_click():
    """Callback when reorganize button is clicked."""
    global current_instance, current_graph_plot, current_req_plot, current_layout
    
    if current_instance is None:
        return
    
    # Generate new random seed
    new_seed = random.randint(1, 10000)
    
    # Compute new layout with different seed
    current_layout = compute_spring_layout(current_instance.graph, seed=new_seed)
    
    # Update both plots with new layout
    graph_title = f"Graph: {current_instance.name}"
    update_graph_plot_with_layout(current_graph_plot, current_instance.graph, current_layout, graph_title)
    
    req_title = f"Requirements: {current_instance.name}"
    requirements = current_instance.get_requirements()
    update_requirements_plot(current_req_plot, current_instance.graph, requirements, current_layout, req_title)


def create_graph_plot_with_layout(G, layout, title):
    """Create graph plot using a specific layout."""
    from bokeh.models import Circle, MultiLine, HoverTool, BoxZoomTool, ResetTool, WheelZoomTool, PanTool
    from bokeh.plotting import figure, from_networkx
    from graph_renderer import compute_edge_colors
    
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
    
    # Compute edge colors
    edge_colors = compute_edge_colors(G)
    for i, (u, v) in enumerate(G.edges()):
        G[u][v]['edge_color'] = edge_colors[i] if edge_colors else "#95a5a6"
    
    # Create renderer with provided layout
    graph_renderer = from_networkx(G, layout, scale=1, center=(0, 0))
    
    # Add cost data
    edge_costs = [G[u][v]['cost'] for u, v in G.edges()]
    graph_renderer.edge_renderer.data_source.data['cost'] = edge_costs
    graph_renderer.edge_renderer.data_source.data['edge_color'] = edge_colors
    
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


def update_graph_plot_with_layout(plot, G, layout, title):
    """Update graph plot with new data using provided layout."""
    from bokeh.models import HoverTool, Circle, MultiLine
    from bokeh.plotting import from_networkx
    from graph_renderer import compute_edge_colors
    
    # Clear existing renderers and hover tools
    plot.renderers = []
    plot.tools = [t for t in plot.tools if not isinstance(t, HoverTool)]
    
    # Update title
    plot.title.text = title
    
    # Compute edge colors
    edge_colors = compute_edge_colors(G)
    for i, (u, v) in enumerate(G.edges()):
        G[u][v]['edge_color'] = edge_colors[i] if edge_colors else "#95a5a6"
    
    # Create renderer with provided layout
    graph_renderer = from_networkx(G, layout, scale=1, center=(0, 0))
    
    # Add cost data
    edge_costs = [G[u][v]['cost'] for u, v in G.edges()]
    graph_renderer.edge_renderer.data_source.data['cost'] = edge_costs
    graph_renderer.edge_renderer.data_source.data['edge_color'] = edge_colors
    
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
    
    # Clear existing renderers and hover tools
    plot.renderers = []
    plot.tools = [t for t in plot.tools if not isinstance(t, HoverTool)]
    
    # Update title
    plot.title.text = title
    
    # Create requirements graph
    req_graph = nx.Graph()
    req_graph.add_nodes_from(G.nodes())
    
    req_weights = []
    for origin, destination, weight in requirements:
        if origin in G.nodes() and destination in G.nodes():
            req_graph.add_edge(origin, destination, weight=weight)
            req_weights.append(weight)
    
    # Compute colors
    if req_weights:
        min_weight = min(req_weights)
        max_weight = max(req_weights)
        
        if max_weight == min_weight:
            req_colors = ["#e74c3c"] * len(req_weights)
        else:
            req_colors = []
            for weight in req_weights:
                norm = (weight - min_weight) / (max_weight - min_weight)
                intensity = int(255 - norm * 100)
                color = f"#ff{intensity:02x}{intensity:02x}"
                req_colors.append(color)
        
        for i, (u, v) in enumerate(req_graph.edges()):
            req_graph[u][v]['edge_color'] = req_colors[i]
    
    # Create renderer
    graph_renderer = from_networkx(req_graph, layout, scale=1, center=(0, 0))
    
    # Add data
    if req_graph.edges():
        edge_weights = [req_graph[u][v]['weight'] for u, v in req_graph.edges()]
        edge_colors = [req_graph[u][v].get('edge_color', '#e74c3c') for u, v in req_graph.edges()]
        graph_renderer.edge_renderer.data_source.data['weight'] = edge_weights
        graph_renderer.edge_renderer.data_source.data['edge_color'] = edge_colors
    
    # Configure appearance
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
    global current_instance, current_graph_plot, current_req_plot, current_layout, layout
    
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
    
    # Compute layout once (shared by both plots)
    current_layout = compute_spring_layout(current_instance.graph)
    
    # Create requirements plot (left)
    req_title = f"Requirements: {current_instance.name}"
    requirements = current_instance.get_requirements()
    current_req_plot = create_requirements_plot(
        current_instance.graph, 
        requirements, 
        current_layout,
        req_title
    )
    
    # Create graph plot (center) - using the same layout
    graph_title = f"Graph: {current_instance.name}"
    current_graph_plot = create_graph_plot_with_layout(
        current_instance.graph, 
        current_layout, 
        graph_title
    )
    
    # Create info panel
    info_panel = create_info_panel(current_instance)
    
    # Create layout: requirements | graph | controls
    controls = column(instance_select, reorganize_btn, info_panel)
    layout = row(current_req_plot, current_graph_plot, controls)
    
    # Add to document
    curdoc().add_root(layout)
    curdoc().title = "OCST Instance Visualizer"


# Import networkx here to avoid circular import
import networkx as nx

# Run initialization
initialize_app()

