# Project Structure - OCST

This document defines the **project organization** that can **evolve** as the project grows. Unlike code standards, this structure is flexible and adaptable.

**⚠️ CRITICAL RULE: ALL documentation, file names, directory names, configuration files, and any project-related text MUST be written in English. No exceptions.**

## 1. Organization Philosophy

### Basic Principles
- **Modularity**: Each algorithm is independent
- **Scalability**: Easy to add new algorithms
- **Clarity**: Intuitive structure for navigation
- **Separation**: Code, documentation, data, and configuration are separated
- **English-only**: All text content must be in English

### Project Evolution
```
Phase 1: Prototyping → One algorithm per directory, everything self-contained
Phase 2: Growth → Separate common elements
Phase 3: Maturity → Complete structure with abstractions
```

## 2. Current Directory Structure

```
OCST/
├── README.md                    # Project description
├── requirements.txt             # Python dependencies
├── Makefile                     # Automated compilation
├── .gitignore                   # Ignored files
│
├── ia_standards/                # 📋 Project standards (AI tools interface)
│   ├── CODE_STANDARDS.md        # Code rules (fixed)
│   └── PROJECT_STRUCTURE.md     # This file (evolves)
│
├── src/                         # 💻 Source code
│   ├── cpp/                     # C++ implementations
│   │   ├── algorithms/          # Main algorithms
│   │   ├── common/              # Shared code
│   │   ├── data_structures/     # Data structures
│   │   ├── utils/               # Utilities
│   │   └── tests/               # Unit tests
│   ├── python/                  # Python scripts
│   └── scripts/                 # Automation scripts
│
├── data/                        # 📊 Data and results
│   ├── input/                   # Test instances
│   ├── output/                  # Experiment results
│   └── benchmarks/              # Reference instances
│
├── experiments/                 # 🧪 Experiment configuration
│   ├── config/                  # YAML configuration files
│   ├── results/                 # Organized results
│   └── scripts/                 # Experimentation scripts
│
├── docs/                        # 📚 Documentation
│   ├── papers/                  # Reference papers
│   ├── notes/                   # Research notes
│   └── presentations/           # Presentations
│
├── thesis/                      # 📖 LaTeX files
│   ├── main.tex                 # Main file
│   ├── chapters/                # Chapters
│   ├── figures/                 # Figures and diagrams
│   └── bibliography/            # References
│
└── build/                       # 🔧 Compilation files (git ignore)
    ├── objects/
    └── executables/
```

### File Naming Convention by Purpose

#### Core Project Files (snake_case)
```
src/cpp/algorithms/branch_bound_formulation_a/
├── readme.md
├── formulation.md
├── pseudocode.md
├── implementation_notes.md
├── config.yaml
├── algorithm.cpp
└── test_instances.cpp
```

#### AI Tools Interface Files (UPPER_SNAKE_CASE)
These files are specifically designed for AI tool interaction and development workflow management, not core project functionality:
```
ia_standards/
├── CODE_STANDARDS.md           # AI-readable code standards
├── PROJECT_STRUCTURE.md        # AI-readable project organization
├── DEVELOPMENT_GUIDELINES.md   # AI development assistance
└── AI_PROMPTS.md               # Prompts for AI assistance
```

## 3. Per-Algorithm Structure (Current Phase)

### Individual Organization
```
src/cpp/algorithms/branch_bound_formulation_a/
├── readme.md                    # Algorithm description and purpose
├── formulation.md              # 📐 PLI formulation (LaTeX + explanations)
├── pseudocode.md               # 🔄 Detailed step-by-step pseudocode
├── implementation_notes.md     # 💡 Design decisions and rationale
├── config.yaml                 # ⚙️ Algorithm-specific configuration
├── algorithm.cpp               # 💻 Main implementation
└── test_instances.cpp          # 🧪 Tests with specific instances
```

### Expected Content per File

#### `readme.md` - Overview
```markdown
# Branch and Bound - Formulation A

## Description
Exact Branch and Bound algorithm for OCST based on the formulation from 
[Author, Year] that uses binary variables for tree edges.

## Characteristics
- Complexity: O(2^n)
- Best for: Small to medium instances (n ≤ 100)
- Source paper: [Complete reference]

## Quick Execution
```bash
cd build && ./branch_bound_formulation_a ../data/input/small_01.ocst
```
```

#### `formulation.md` - Mathematics
```markdown
# PLI Formulation - Branch and Bound A

## Decision Variables
$$x_{ij} \in \{0,1\} \quad \forall (i,j) \in E$$
*$x_{ij} = 1$ if edge $(i,j)$ is in the optimal tree*

$$y_i \geq 0 \quad \forall i \in V$$
*$y_i$ represents the communication cost of node $i$*

## Objective Function
$$\min \sum_{(i,j) \in E} c_{ij} x_{ij} + \sum_{i \in V} d_i y_i$$

## Constraints
...
```

#### `implementation_notes.md` - Design Decisions
```markdown
# Implementation Notes

## Key Decisions
1. **Data structure**: Using adjacency matrix for efficient access
2. **Branch strategy**: Branching on x_ij variables with highest objective impact
3. **Bound calculation**: Lower bound using Gurobi's linear relaxation

## Applied Optimizations
- Preprocessing to eliminate dominated edges
- Dynamic connectivity cuts
- Warm start with heuristic solution
```

### Configuration Files
```yaml
# config.yaml per algorithm
algorithm:
  name: "Branch and Bound - Formulation A"
  description: "Exact B&B with binary edge variables"
  
solver:
  time_limit: 3600
  gap_tolerance: 1e-6
  branching_strategy: "most_fractional"
  
preprocessing:
  eliminate_dominated_edges: true
  add_connectivity_cuts: true
  use_heuristic_warm_start: true

output:
  save_tree: true
  save_metrics: true
  log_level: "INFO"
```

## 4. Common Elements (When Project Grows)

### `src/cpp/common/` - Shared Code
```
common/
├── instance_parser.h/cpp       # Instance file reading
├── solution.h/cpp              # Common solution structure
├── metrics_collector.h/cpp     # Metrics collection
├── gurobi_base.h/cpp          # Basic Gurobi wrapper
├── graph_utils.h/cpp          # Graph utilities
└── config_manager.h/cpp        # YAML configuration management
```

### `src/cpp/data_structures/` - Basic Structures
```
data_structures/
├── graph.h/cpp                 # Graph representation
├── tree.h/cpp                  # Tree representation
├── ocst_instance.h/cpp         # Complete problem instance
└── priority_queue.h/cpp        # Optimized priority queue
```

## 5. Metrics and Benchmarking System

### Metrics Structure
```cpp
struct AlgorithmMetrics {
    // Time
    double total_time;
    double gurobi_time;
    double preprocessing_time;
    
    // Branch and Bound
    int nodes_explored;
    int nodes_pruned;
    double best_bound;
    double best_solution;
    
    // Memory
    size_t peak_memory_usage;
    
    // Algorithm-specific
    std::map<std::string, double> custom_metrics;
    
    void export_to_json(const std::string& filename) const;
    void export_to_csv(const std::string& filename) const;
};
```

### Algorithm Comparison
```
experiments/results/
├── comparison_small_instances/
│   ├── metrics_summary.json
│   ├── performance_chart.pdf
│   └── detailed_results.csv
├── comparison_medium_instances/
└── comparison_large_instances/
```

## 6. Development Workflow

### Adding New Algorithm
1. **Create directory**: `src/cpp/algorithms/new_algorithm/`
2. **Document formulation**: Create `formulation.md` with LaTeX
3. **Write pseudocode**: Detail in `pseudocode.md`
4. **Implement**: Create `algorithm.cpp` following standards
5. **Configure**: Create algorithm-specific `config.yaml`
6. **Test**: Implement `test_instances.cpp`
7. **Document decisions**: Complete `implementation_notes.md`

### Evolution to Common Code
```
Criteria: If 3+ algorithms use the same functionality
  ↓
Extract to common/ → Maintain backward compatibility
```

## 7. Experiment Configuration

### Configuration Structure
```yaml
# experiments/config/comparison_study.yaml
experiment:
  name: "Comparison of B&B Formulations"
  description: "Compare different B&B formulations on various instance sizes"
  
algorithms:
  - name: "branch_bound_formulation_a"
    config: "algorithms/branch_bound_formulation_a/config.yaml"
  - name: "branch_bound_formulation_b"  
    config: "algorithms/branch_bound_formulation_b/config.yaml"

instances:
  small: "data/input/small_instances/*.ocst"
  medium: "data/input/medium_instances/*.ocst"
  
global_settings:
  time_limit: 1800
  repetitions: 3
  
output:
  directory: "experiments/results/comparison_study_2024"
  formats: ["json", "csv", "pdf"]
```

## 8. Data Management

### Instance Format
```
data/input/
├── small_instances/        # n ≤ 50
│   ├── random_10_dense.ocst
│   └── grid_25.ocst
├── medium_instances/       # 50 < n ≤ 200
└── large_instances/        # n > 200

data/benchmarks/           # Reference instances from papers
├── steinlib_converted/
└── custom_generated/
```

### Experiment Results
```
data/output/
├── algorithm_name/
│   ├── solutions/         # Solution trees
│   ├── logs/             # Detailed logs
│   └── metrics/          # Metrics in JSON/CSV
└── comparisons/          # Comparative studies
```

## 9. Thesis Integration

### LaTeX Connection
- **Figures**: Automatically generate in `thesis/figures/`
- **Tables**: Export metrics to `thesis/tables/`
- **References**: Maintain `thesis/bibliography/references.bib`

### Automation Scripts
```bash
# scripts/generate_thesis_assets.py
python scripts/generate_thesis_assets.py --experiment comparison_study
  → Generates PDF figures for LaTeX
  → Exports tables in LaTeX format
  → Updates references automatically
```

## 10. Documentation Standards

### All Documentation Files Must Include:
- **Purpose**: Clear description of what the file/algorithm does
- **Usage**: How to compile, run, or use
- **Examples**: Concrete examples with expected output
- **Dependencies**: What libraries or tools are required
- **References**: Academic papers or sources

### File Naming Examples
```
# ✅ CORRECT - English names with proper case
Core project files (snake_case):
├── readme.md
├── formulation.md
├── pseudocode.md
├── implementation_notes.md

AI interface files (UPPER_SNAKE_CASE):
├── CODE_STANDARDS.md
├── PROJECT_STRUCTURE.md
├── DEVELOPMENT_GUIDELINES.md

# ❌ INCORRECT - Spanish/other languages or wrong case
├── LEEME.md
├── formulacion.md
├── pseudocodigo.md
├── notas_implementacion.md
```

---

## Next Evolution Steps

### Current Phase (Prototyping)
- ✅ One algorithm per self-contained directory
- ✅ Rich documentation in Markdown (English only)
- ✅ Individual testing

### Phase 2 (When we have 3+ algorithms)
- 📦 Extract common elements to `common/`
- 📊 Unified benchmarking system
- ⚙️ Centralized experiment configuration

### Phase 3 (Mature project)
- 🏗️ Abstractions for algorithm types
- 📈 Web dashboard for results
- 🤖 CI/CD for automatic experiments

**This structure evolves according to project needs, but all content remains in English.**
