"""
Helper module for requirement path tracing interaction.

This creates invisible clickable nodes at the midpoint of each requirement edge
to enable reliable click-based interaction (since clicking edges directly is unreliable in Bokeh).
"""

from bokeh.models import Circle, ColumnDataSource, HoverTool, TapTool
from bokeh.plotting import figure
import networkx as nx


def add_requirement_midpoint_nodes(plot: figure, req_graph: nx.Graph, layout: dict, 
                                   solution_plot_ref, requirements: list, solution_edges: list):
    """
    Add invisible clickable nodes at the midpoint of each requirement edge.
    
    Args:
        plot: Requirements plot figure
        req_graph: Requirements graph (not used, kept for compatibility)
        layout: Node layout positions
        solution_plot_ref: Reference to solution plot to update (not used, kept for compatibility)
        requirements: Original requirements list
        solution_edges: Solution tree edges (not used in this function)
    """
    from graph_renderer import accumulate_bidirectional_requirements
    
    # Get accumulated requirements
    accumulated = accumulate_bidirectional_requirements(requirements)
    
    # Compute midpoints and path info for each requirement edge
    midpoint_x = []
    midpoint_y = []
    req_origins = []
    req_destinations = []
    req_weights = []
    
    for origin, dest, weight in accumulated:
        if origin in layout and dest in layout:
            x1, y1 = layout[origin]
            x2, y2 = layout[dest]
            mid_x = (x1 + x2) / 2
            mid_y = (y1 + y2) / 2
            
            midpoint_x.append(mid_x)
            midpoint_y.append(mid_y)
            req_origins.append(origin)
            req_destinations.append(dest)
            req_weights.append(weight)
    
    # Create data source for midpoint nodes
    midpoint_source = ColumnDataSource(data={
        'x': midpoint_x,
        'y': midpoint_y,
        'origin': req_origins,
        'destination': req_destinations,
        'weight': req_weights
    })
    
    # Add invisible circles at midpoints (clickable)
    midpoint_renderer = plot.scatter(
        'x', 'y',
        source=midpoint_source,
        size=20,  # Large enough to click easily
        color='rgba(100, 150, 200, 0.05)',  # Almost invisible (5% opacity soft blue)
        line_color=None,
        hover_color='#5dade2',  # Soft blue on hover (doesn't conflict with red requirements)
        hover_alpha=0.5,
        selection_color='#3498db',  # Medium blue when selected
        selection_alpha=0.7
    )
    
    # Add tap tool for clicking (if not already present)
    has_tap = any(isinstance(tool, TapTool) for tool in plot.tools)
    if not has_tap:
        plot.add_tools(TapTool())
    
    # Add hover tool for midpoint nodes
    midpoint_hover = HoverTool(
        tooltips=[
            ("Click to trace:", "@origin → @destination"),
            ("Weight", "@weight{0.00}")
        ],
        renderers=[midpoint_renderer]
    )
    plot.add_tools(midpoint_hover)
    
    return midpoint_source, midpoint_renderer

