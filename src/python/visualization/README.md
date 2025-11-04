# OCST Instance Visualizer

Interactive visualization tool for exploring OCST problem instances using Bokeh.

## Installation

Install dependencies:

```bash
cd src/python/visualization
pip install -r requirements.txt
```

## Usage

Run the Bokeh server application:

```bash
# From project root
bokeh serve src/python/visualization/app.py --show

# Or with custom port
bokeh serve src/python/visualization/app.py --port 5007 --show
```

The application will open in your default browser at `http://localhost:5006/app`.

## Features (Current Implementation)

### Phase 1: Basic Visualization ✓
- **Instance Selection:** Dropdown menu to select from available JSON instances
- **Graph Rendering:** Interactive graph visualization with spring layout
- **Node Hover:** Displays node ID on hover
- **Edge Hover:** Displays edge cost on hover
- **Pan/Zoom:** Interactive navigation (mouse drag to pan, scroll to zoom)
- **Info Panel:** Shows instance properties:
  - Graph statistics (nodes, edges, density, connectivity)
  - Requirements count and total weight
  - Tags and metadata

## Data Sources

- **Instances:** Loaded from `data/input/*.json`
- **Format:** Standardized OCST instance JSON schema v1.0

## Architecture

- `app.py` - Main Bokeh server application
- `graph_loader.py` - Load JSON instances and convert to NetworkX
- `graph_renderer.py` - Create Bokeh plots from NetworkX graphs
- `requirements.txt` - Python dependencies

## Next Steps (Planned)

- Phase 2: Add structural property overlays (bridges, articulation points, leaves)
- Phase 3: Overlay optimal solutions from solver results
- Phase 4: Advanced analysis and statistics

