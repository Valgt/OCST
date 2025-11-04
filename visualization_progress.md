# Visualization Progress Log

**Document Purpose:** Track incremental development of the interactive OCST instance visualization tool.

**Last Updated:** 2025-11-04  
**Current Phase:** Phase 1 - Basic Visualization + Interactive Path Tracing ✅  
**Branch:** `visualizacion`  
**Technology:** Python + Bokeh + NetworkX

---

## 🎯 Current Objective

Build a basic interactive graph viewer that:
- Loads OCST instances from JSON
- Renders the graph using Bokeh
- Provides instance selection via dropdown
- Shows node/edge properties on hover

---

## 📋 Progress Tracker

### ✅ Phase 0: Setup
- [x] Create visualization branch (`visualizacion`)
- [x] Define project rules (`.cursor/rules/Visualization Phase.mdc`)
- [x] Create progress tracking document
- [x] Create project directory structure (`src/python/visualization/`)
- [ ] Install dependencies (Bokeh, NetworkX) - _User will test_

### ✅ Phase 1: Basic Visualization (Complete)
- [x] Implement `graph_loader.py`
  - [x] Read JSON instance files
  - [x] Parse into NetworkX graph
  - [x] Handle edges and nodes
  - [x] Extract requirements and metadata
- [x] Implement `graph_renderer.py`
  - [x] Convert NetworkX to Bokeh graph
  - [x] Apply spring layout (NetworkX)
  - [x] Configure hover tooltips (nodes show ID, edges show cost)
  - [x] Interactive tools (pan, zoom, reset)
  - [x] **Cost-based edge coloring** (darker = higher cost)
- [x] Create `app.py`
  - [x] Bokeh server application
  - [x] Instance selection dropdown
  - [x] Info panel with graph statistics
  - [x] Info panel with edge cost statistics (min/max/avg)
  - [x] Wire loader → renderer
- [x] Create README with usage instructions
- [x] Manual testing ✅
  - [x] Create Python virtual environment (`venv/`)
  - [x] Install dependencies (Bokeh 3.8.0, NetworkX 3.5, NumPy 2.3.4)
  - [x] Run `bokeh serve src/python/visualization/app.py --show`
  - [x] Server running successfully on http://localhost:5006/app
  - [x] Create convenience script `run_visualizer.sh`
  - [x] Fix Bokeh 3.x API compatibility issues
  - [x] Add cost-based edge intensity visualization

### ⏳ Phase 2: Structural Properties (Pending)
- [ ] Implement `graph_properties.py`
  - [ ] Compute bridges
  - [ ] Compute articulation points
  - [ ] Compute leaves
- [ ] Add property overlays to renderer
- [ ] Create UI toggles for property visibility
- [ ] Test on multiple instances

### ⏳ Phase 3: Solution Overlay (Pending)
- [ ] Implement `solution_loader.py`
- [ ] Parse solver results JSON
- [ ] Overlay optimal tree on graph
- [ ] Add solution/no-solution toggle

### ⏳ Phase 4: Advanced Analysis (Future)
- [ ] Property statistics panel
- [ ] Degree distribution histogram
- [ ] Export functionality
- [ ] Layout selection UI

---

## 🔍 Key Observations

*This section will capture insights about instance properties as we visualize them.*

### Technical Implementation Notes

**Phase 1 Implementation Details:**
- **Data Loading:** Reads from `data/input/*.json` using standardized schema v1.0
- **Graph Library:** NetworkX for graph representation and algorithms
- **Layout Algorithm:** Spring layout (Fruchterman-Reingold) with k=1.5, 50 iterations
- **Visualization:** Bokeh server app with interactive callbacks
- **Node Rendering:** Circle glyphs, size=20, blue (#3498db), red on hover
- **Edge Rendering:** MultiLine, grey (#95a5a6), red on hover, thickness=2
- **Hover Info:** Node ID for vertices, edge cost for edges
- **Info Panel:** Real-time statistics (nodes, edges, density, connectivity, requirements)

**Instance Paths:**
- Input: `data/input/*.json`
- Future: `data/output/test_instances/*.results.json` (for solutions)

### Instance Characteristics (To be observed during testing)
- **Bridges:** Count, distribution, impact on solution
- **Articulation Points:** Critical nodes, connectivity implications
- **Graph Density:** Sparse vs dense instances
- **Requirement Patterns:** Common origin/destination patterns

---

## 🐛 Issues and Blockers

### ✅ Resolved
**Issue #1: Bokeh 3.x API Compatibility**
- **Problem:** Blank page on initial load
- **Root Cause:** 
  - `from_networkx` import location changed in Bokeh 3.x (was in `bokeh.models.graphs`, now in `bokeh.plotting`)
  - `Circle` glyph no longer accepts `size` parameter, must use `radius` instead
- **Solution:** Updated imports and changed `size=20` to `radius=0.05`
- **Status:** ✅ Fixed in commit d360b4b

---

## 💡 Ideas and Future Enhancements

- Compare multiple instances side-by-side
- Animate SEC cuts during solving
- Visualize requirement paths in solution tree
- Heatmap of edge usage across different formulations
- Interactive graph editing (for instance generation)

---

## 📊 Current Features

### Quad-Plot Visualization (2x2 Grid) ✨
- **Top-Left:** Requirements graph (red nodes, grayscale edges, weight-based intensity)
- **Top-Right:** Infrastructure graph (grayscale, cost-based intensity)
- **Bottom-Left:** Spacer (reserved for future features)
- **Bottom-Right:** Solution tree (blue/orange nodes by degree, color-coded edges)
- **Right Panel:** Controls + statistics

### Interactive Features 🎮
- **Instance Selector:** Dropdown to switch between instances
- **Reorganize Layout:** Button to recompute graph layout (hierarchical for trees)
- **Marcar OCST:** Checkbox to highlight solution edges in the cost graph (green with intensity)

#### **Bidirectional Graph Interaction:** 🔄 ⭐⭐
1. **Requirement → Tree Path Tracing** (Click requirement edge)
   - **Hover over requirement edges** to see a tooltip with origin → destination
   - **Click on the tooltip area** (invisible blue node at edge midpoint) to trace its path in the solution tree
   - Uses invisible clickable nodes at requirement edge midpoints for reliable interaction
   - Computes shortest path in solution tree using NetworkX
   - Highlights path edges in **bright pink/magenta** with **very thick width (12px)** for maximum visibility
   - Selected requirement shows as darker blue circle at midpoint
   - Technical: Edge clicking is unreliable in Bokeh, so we use midpoint nodes as a workaround

2. **Tree Node → Requirements Flow** ⭐ **NEW** (Click tree node)
   - **Click on any node in the solution tree** (bottom-right) to see all requirements that flow through it
   - Highlights ALL requirement edges passing through that node
   - Highlighted requirements: **bright blue (#3498db), thick (6px), full opacity**
   - Dimmed requirements: **grayscale, normal width (3px), 30% opacity**
   - Pre-computes paths for all requirements using NetworkX shortest_path
   - Useful for understanding node importance and traffic patterns
   - Click another node to switch, or click elsewhere to clear highlighting

### Visual Encoding
- **Requirements Graph:**
  - Red nodes (#e74c3c)
  - Grayscale edges based on accumulated bidirectional weight
  - Zero-weight requirements filtered out
  - Dashed lines for zero-cost edges
- **Cost Graph:**
  - Grayscale edges based on cost (darker = more expensive)
  - Dashed lines for zero-cost edges
  - Optional green highlighting for solution edges (when "Marcar OCST" is checked)
- **Solution Tree:**
  - Node color by degree: Blue (low degree/leaves), Orange (high degree/hubs)
  - Edge color by endpoint degrees: Blue (branches), Orange (backbone)
  - **Pink/Magenta (#e91e63):** Highlighted requirement path (width: **12px** - VERY visible!)
  - Edge width: Normal 4px, Highlighted **12px** (3x thicker for clear visibility)

### Layout Algorithms
- **Hierarchical Layout:** For solution trees (minimizes crossings, BFS-based levels)
  - Parabolic vertical offset (curve_depth = 0.8 * y_spacing) for horizontal separation
  - Degree-based vertical offset to spread high-degree nodes
  - Horizontal jitter (±0.175) to prevent perfect vertical alignment
  - Multiple root candidates (center, periphery, all nodes) for layout variety
- **Spring Layout (Fruchterman-Reingold):** For graphs without solutions

---

**Development Philosophy:** One feature at a time. Test visually. Iterate based on findings.

