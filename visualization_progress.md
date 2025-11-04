# Visualization Progress Log

**Document Purpose:** Track incremental development of the interactive OCST instance visualization tool.

**Last Updated:** 2025-11-04  
**Current Phase:** Phase 1 - Basic Visualization + Bidirectional Interaction ✅ (STABLE)  
**Branch:** `visualizacion`  
**Technology:** Python + Bokeh + NetworkX  
**Status:** Production-ready interactive visualizer with robust callback management

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

**Issue #2: Callback Accumulation Bug** ⚠️ **CRITICAL**
- **Problem:** Interactions worked once, then stopped working or behaved erratically
- **Root Cause:** Bokeh's `on_change()` ACCUMULATES callbacks instead of replacing them. Every time `update_solution_plot()` destroyed renderers, we reconnected with `on_change()`, leading to exponential accumulation (1, 2, 4, 8... callbacks per click)
- **Solution:** Created `reconnect_solution_node_callback()` helper that:
  1. Removes old callback with `remove_on_change()` first
  2. Adds fresh callback with `on_change()`
  3. Ensures exactly ONE callback exists at any time
- **Impact:** Solved "clicks only work once or twice" issue
- **Status:** ✅ Fixed in commit fbf434d

**Issue #3: Reorganize Layout Breaks Interactions** ⚠️ **CRITICAL**
- **Problem:** After clicking "Reorganize Layout", ALL interactions stopped working (both midpoint clicks and node clicks)
- **Root Cause:** `on_reorganize_click()` called `update_requirements_plot()` (destroying midpoint nodes) and `update_solution_plot()` (destroying node callbacks) but never reconnected them
- **Solution:** 
  1. Re-add midpoint nodes after `update_requirements_plot()`
  2. Call `reconnect_solution_node_callback()` after `update_solution_plot()`
- **Impact:** All interactions now survive layout reorganization
- **Status:** ✅ Fixed in commit 92e8a7e

**Issue #4: Callback Loss on Deselection (Hidden Path Bug)** ⚠️ **CRITICAL**
- **Problem:** "Node clicks stop working after using visualizer for a while" - seemed random and intermittent
- **Root Cause:** When user clicks on background/empty space to deselect a requirement, Bokeh triggers midpoint callback with `new=[]`. Two callbacks had early-return paths that called `update_solution_plot()` to clear highlighting but didn't reconnect the node callback:
  - `on_midpoint_click_updated()` in `on_instance_change()`
  - `on_requirement_midpoint_click()` in `initialize_app()`
- **Why it seemed random:** Bug only manifested after deselecting (clicking background), not during normal interactions. Users naturally do this while exploring, so it appeared "after a while" rather than immediately
- **Solution:** Added `reconnect_solution_node_callback()` to BOTH deselection early-return paths
- **Impact:** Interactions now work reliably through ANY sequence of selections/deselections
- **Status:** ✅ Fixed in commit 5ec745a

**Issue #5: Bidirectional Interaction Interference**
- **Problem:** After using midpoint click, tree node clicks stopped working (and vice-versa)
- **Root Cause:** Selection states weren't being cleared mutually between the two interaction modes
- **Solution:** Each interaction now explicitly clears the other's selection state before activating
- **Status:** ✅ Fixed in commit 0968761

**Issue #6: Automatic Node Dimming on Selection**
- **Problem:** When clicking a tree node, other nodes became opaque/dimmed automatically
- **Root Cause:** Bokeh's default selection behavior dims non-selected glyphs
- **Solution:** Set `selection_glyph` and `nonselection_glyph` to normal appearance, disabling automatic dimming
- **Status:** ✅ Fixed in commit 495adcd

---

## 💡 Ideas and Future Enhancements

### High Priority (Quick Wins)
- **Export visualization to PNG/SVG:** Save current view as image for papers/reports
- **Show requirement weight on hover:** Currently only shows origin→destination, add weight value
- **Filter requirements by weight threshold:** Slider to hide low-weight requirements, focus on critical flows
- **Highlight multiple nodes simultaneously:** Ctrl+Click to select multiple tree nodes, see combined requirement flow
- **Path cost display:** When clicking requirement, show total path cost in solution tree

### Medium Priority (UX Improvements)
- **Color scheme selector:** Dark mode, colorblind-friendly palettes
- **Layout persistence:** Remember layout across instance switches (cache positions)
- **Instance comparison mode:** Side-by-side view of two instances
- **Search/filter nodes:** Text input to highlight specific node IDs
- **Edge thickness scale control:** Slider to adjust visibility of edge intensity differences
- **Legend panel:** Color/style guide for graph elements

### Research/Analysis Features
- **Structural properties overlay:** Bridges, articulation points, leaves (from Phase 2 roadmap)
- **Solution comparison:** Overlay solutions from different formulations, highlight differences
- **Heatmap of edge usage:** Across multiple instances or formulations
- **Requirement flow statistics:** Most-used paths, bottleneck nodes, flow distribution
- **Animate SEC cuts:** Show subtour elimination constraint iterations during solving
- **Sensitivity analysis:** Show impact of cost changes on solution

### Advanced/Future
- **Interactive graph editing:** Create/modify instances visually
- **Time-series analysis:** If we add timestamp data, animate graph evolution
- **3D visualization:** For very large graphs (WebGL-based)

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

