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
from bokeh.models import Select, Div
from bokeh.plotting import curdoc

from graph_loader import get_available_instances, load_instance
from graph_renderer import create_graph_plot, update_graph_plot


# Configuration
DATA_DIR = Path(__file__).parent.parent.parent.parent / "data" / "input"

# Global state
current_instance = None
current_plot = None


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
    global current_instance, current_plot
    
    # Load new instance
    instance_path = DATA_DIR / f"{new}.json"
    current_instance = load_instance(instance_path)
    
    # Update plot
    title = f"OCST Instance: {current_instance.name}"
    update_graph_plot(current_plot, current_instance.graph, title)
    
    # Update info panel
    new_info_panel = create_info_panel(current_instance)
    layout.children[0].children[1] = new_info_panel


# Initialize application
def initialize_app():
    """Initialize the Bokeh application."""
    global current_instance, current_plot, layout
    
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
    
    # Create initial plot
    title = f"OCST Instance: {current_instance.name}"
    current_plot = create_graph_plot(current_instance.graph, title)
    
    # Create info panel
    info_panel = create_info_panel(current_instance)
    
    # Create layout
    controls = column(instance_select, info_panel)
    layout = row(controls, current_plot)
    
    # Add to document
    curdoc().add_root(layout)
    curdoc().title = "OCST Instance Visualizer"


# Import networkx here to avoid circular import
import networkx as nx

# Run initialization
initialize_app()

