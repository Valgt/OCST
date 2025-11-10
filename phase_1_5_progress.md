# Phase 1.5 Progress Log - Standardization Baseline

**Document Purpose:** Track progress, decisions, and blockers for Phase 1.5 standardization workstreams.

**Last Updated:** 2025-11-10  
**Current Status:** Workstream 1 (Unified data model and loaders) - Testing & Integration phase  
**Control Version:** `path_based_formulation_original` (frozen)  
**Migration Target:** `path_based_formulation` (active development)

---

## 🎯 Overall Strategy

### Parallel Versions Policy
- **Control:** `path_based_formulation_original` - Best performing formulation, frozen except for minimal fixes
- **Migrating:** `path_based_formulation` - Prototyping common infrastructure before promotion to `src/cpp/common/`
- **Pending:** `flow_based_formulation`, `flow_based_relaxed_formulation`, `rooted_tree_based_formulation` - Awaiting validation

### Promotion Criteria
Once `path_based_formulation` achieves parity with control:
1. Dual-run experiments show matching objectives/bounds
2. Performance overhead < 10% (JSON loader, logging)
3. Regression tests pass on `quick_check` set
4. Unit test coverage for common components

Then promote:
- `src/cpp/algorithms/path_based_formulation/include/*.h` → `src/cpp/common/`
- Change namespace `ocst::path_based` → `ocst::common`
- Update include guards `OCST_PATH_BASED_*` → `OCST_COMMON_*`

---

## 📊 Workstream Status

### ✅ Completed

#### JSON Schemas (Phase 1.5 requirement)
- **Location:** `schemas/`
- **Files:**
  - `instance.schema.v1.json` - Instance format (graph + requirements, immutable)
  - `result.schema.v1.json` - Result format (status, solution, metrics, reproducibility)
  - `config.schema.v1.json` - Configuration format
- **Validation:** Schema compliance with JSON Schema draft 2020-12 ✓

#### Instance Data Conversion
- **Status:** All instances converted to JSON format
- **Location:** `data/input/*.json`
- **Count:** 28+ instances (ocstpin0-14, orst0-9, orstBig0-2)
- **Tags:** `quick_check` tag assigned to regression subset (e.g., ocstpin0)
- **Note:** Conversion done ad-hoc; no standalone converter script yet

#### Common Headers (Prototyping in `path_based_formulation/include/`)

**1. `common_types.h`** ✅
- **Namespace:** `ocst::path_based` (will become `ocst::common`)
- **Include guard:** `OCST_PATH_BASED_COMMON_TYPES_H` (will become `OCST_COMMON_TYPES_H`)
- **Structures:**
  - `Edge` - Graph edge with source, destination, cost
  - `Requirement` - Communication demand with origin, destination, weight
  - `OCSTInstance` - Complete instance with:
    - Graph data (nodes, edges, probability)
    - Adjacency matrix for O(1) edge lookups
    - Edge index map for bidirectional queries
    - Validation logic
- **Methods:**
  - `add_edge()` - Maintains adjacency structures
  - `add_requirement()` - Bounds checking
  - `has_edge()`, `get_edge_index()` - Query API
  - `validate()` - Consistency checks
- **Complexity:** O(n²) space for adjacency matrix, O(1) edge queries

**2. `instance_loader.h`** ✅
- **Namespace:** `ocst::path_based`
- **Include guard:** `OCST_PATH_BASED_INSTANCE_LOADER_H`
- **Dependencies:** nlohmann/json (expects `-Ithird_party` flag)
- **Function:** `load_instance(filename)` → `OCSTInstance`
- **Schema support:** Validates `schema_version == "1.0"`
- **Error handling:**
  - Validates required fields (graph, requirements, nodes, edges)
  - Validates node indices (0 ≤ i < n)
  - Validates non-negative costs/weights
  - Throws descriptive `std::runtime_error` with context
- **Complexity:** O(n + m) streaming parse, no quadratic materialization ✓
- **Tested:** Manual testing only (no unit tests yet)

**3. `result_serializer.h`** ✅
- **Namespace:** `ocst::path_based`
- **Include guard:** `OCST_PATH_BASED_RESULT_SERIALIZER_H`
- **Dependencies:** nlohmann/json
- **Key structures:**
  
  ```cpp
  enum class OptimizationStatus {
      OPTIMAL, TIME_LIMIT, INFEASIBLE, UNBOUNDED,
      SUBOPTIMAL, INTERRUPTED, ERROR
  };
  
  struct RuntimeStats {
      double wall_clock_seconds;
      double cpu_seconds;
      int solver_nodes;
      int solver_iterations;
      std::string termination_reason;
  };
  
  struct TreeEdge {
      int source, destination;
  };
  
  struct SolutionTree {
      std::vector<TreeEdge> tree_edges;
      double tree_cost;
      bool is_spanning_tree;
      bool is_connected;
  };
  
  struct Reproducibility {
      std::string git_commit;
      bool git_dirty;
      int seed;
      std::string timestamp;
  };
  
  struct ResultPayload {
      // Schema & identification
      std::string schema_version = "1.0";
      std::string run_uuid;  // UUIDv4
      
      // Instance
      std::string instance_name;
      std::vector<std::string> instance_tags;
      
      // Solver
      std::string solver_id;
      std::string solver_version;
      std::string formulation;
      
      // Configuration
      std::string config_digest;
      std::map<std::string, std::string> config_params;
      
      // Status
      OptimizationStatus optimization_status_code;
      std::string optimization_status_description;
      bool has_solution;
      bool has_bound;
      
      // Metrics
      double objective;
      double primal_bound;
      double dual_bound;
      double gap;
      double gap_percent;
      double best_solution_time;
      
      // Runtime
      RuntimeStats runtime_stats;
      
      // Solution
      SolutionTree solution;
      
      // Reproducibility
      Reproducibility reproducibility;
      
      // Artifacts
      std::string log_file_path;
      std::string result_file_path;
      std::string warm_start_path;
      
      // Formulation-specific
      std::map<std::string, std::string> solver_metadata;
      
      // Validation
      std::pair<bool, std::string> validate() const;
  };
  ```

- **Class:** `ResultSerializer` (static methods only)
  - `to_json(payload)` → `nlohmann::json` - Serialization with validation
  - `write_to_file(payload, filepath, pretty=true)` - Direct file write with directory creation
- **Compliance:** Fully implements `result.schema.v1.json` specification ✓
- **Validation:** Pre-serialization checks (run_uuid, instance_name, solver_id required; logical consistency)

#### nlohmann/json Integration
- **Status:** ✅ In use
- **Location:** `third_party/nlohmann/` (assumed from include paths)
- **Compiler flag:** `-Ithird_party`
- **Version:** Not documented (should verify)

---

### ✅ Recently Completed

#### Validation of Prototyped Components (PASSED - 2025-11-04)
- **Dual-run validation:** 25/25 instances PASS (objectives + edges match exactly)
- **Numerical precision:** Deep code analysis confirms identical behavior
- **Result:** 100% parity, diff = 0.0 (not just < 1e-6, but exact)
- **Reports:** `experiments/validation/dual_run_20251103_225726/`
- **Conclusion:** **READY FOR PROMOTION** ✅

---

### ✅ Recently Completed (2025-11-10)

#### Unit Tests - **100% SUCCESS**
- **Location:** `tests/unit/`
- **Framework:** Google Test (compiled from source, no CMake dependency)
- **Test Files Created:**
  - `common_types_test.cpp` - 20 tests (Edge, Requirement, OCSTInstance)
  - `instance_loader_test.cpp` - 17 tests (JSON parsing, validation, error handling)
  - `result_serializer_test.cpp` - 25 tests (serialization, validation, file writing)
- **Results:** ✅ **62/62 tests PASSED (100%)**
- **Coverage:** > 80% (all critical paths tested)
- **Build System:** Custom Makefile with direct g++ compilation
- **Compilation Time:** ~3s for full rebuild

#### Technical Integrations Completed
- **UUID Library:** ✅ sole.hpp downloaded to `third_party/sole/` (single-header, MIT license)
- **Build Info Generator:** ✅ Makefile automatically generates `src/cpp/common/build_info.h` with:
  - `GIT_COMMIT` (short hash)
  - `GIT_DIRTY` (bool)
  - `BUILD_TIMESTAMP` (ISO 8601 UTC)
- **CSV Post-Processor:** ✅ `scripts/standardization/results_to_csv.py` (310 lines)
  - Converts JSON results to CSV for quick analysis
  - Supports single directory or recursive aggregation
  - Extracts 23 key fields per result
  - JSON remains source of truth

#### Code Improvements
- **`common_types.h`:** Updated `validate()` to return `std::pair<bool, std::string>` with descriptive error messages
- **`instance_loader.h`:** Enhanced error messages for better debugging
- **Build System:** Integrated `build_info` target into main Makefile

#### Regression Test Bed
- **Files needed:**
  - `tests/data/regression/quick_checks.json` - List of instances tagged `quick_check`
  - `tests/data/regression/metadata.json` - Expected optimal costs and bounds
  - `tests/data/regression/golden/` - Golden result artifacts from control solver
- **Purpose:** Automate parity validation (control vs migrating)
- **Instances:** Identify 3-4 instances that run < 1 minute
- **Currently identified:** `ocstpin0` (has `quick_check` tag in JSON)

#### Performance Benchmarks
- **Target location:** `scripts/standardization/benchmarks/`
- **Scenarios:**
  - JSON loader overhead (compare parse time vs legacy)
  - Memory usage (O(n + m) guarantee)
  - Logging overhead (when implemented in Workstream 4)
- **Baseline:** Control solver performance on `quick_check` set
- **Acceptance:** < 10% overhead on JSON loader
- **Storage:** Results → `experiments/benchmarks/`

#### Legacy Converter Script
- **File:** `scripts/standardization/convert_ocstpin_to_json.py`
- **Purpose:** One-time conversion of `.ocstpin` files to JSON
- **Status:** Not implemented (instances already converted manually)
- **Priority:** Low (conversion already done, but useful for documentation/reproducibility)
- **Note:** Solvers never read legacy format; this is offline tooling only

#### Dual-Run Experiment Automation
- **Purpose:** Compare control vs migrating solver systematically
- **Workflow:**
  1. Run `path_based_formulation_original` on instance set
  2. Run `path_based_formulation` on same set
  3. Compare objectives, bounds, runtime distributions
  4. Report discrepancies
- **Output:** Comparison report (CSV/JSON) + approval logs
- **Location:** `experiments/validation/dual_run_reports/`

#### Configuration Loading (Workstream 4 dependency)
- **File:** `src/cpp/common/config.h` (future)
- **Purpose:** Load `config.schema.v1.json` files
- **Class:** `ocst::common::Config` with `ConfigDefaults`
- **Requirements:**
  - Declarative JSON config for all solvers
  - Default values for optional parameters
  - Fail-fast on missing required keys
- **Blocked by:** Workstream 1 validation (need stable loader contract)

---

## 🚧 Current Blockers

**None.** Workstream 1 is in validation phase; all prerequisite components are implemented.

---

## 🔍 Observations & Decisions

### Decision: Namespace Strategy
- **Context:** Headers start in `ocst::path_based` for prototyping
- **Rationale:** Avoid breaking other code during experimentation
- **Promotion plan:** Change to `ocst::common` when moving to `src/cpp/common/`
- **Impact:** Single find-replace operation; minimal risk

### Decision: No Over-Engineering for Validation Phase
- **Context:** ResultPayload has full schema compliance, but not all fields used yet
- **Rationale:** Focus on proving the architecture works before adding complexity
- **Trade-off:** Complete structure now means less refactoring later
- **Status:** Accepted

### Observation: Spanish Documentation Debt
- **Status:** Multiple files still in Spanish (formulation.md, Makefile messages, etc.)
- **Decision:** Defer translation until Phase 1.5 completion
- **Rationale:** Focus on functional correctness first; localization is cosmetic at this stage
- **Tracking:** Document as Phase 2 cleanup task

### Observation: Missing Conversion Script
- **Status:** Instances already converted, but no reproducible script
- **Impact:** Low (one-time operation already done)
- **Decision:** Defer to "nice to have" unless we receive new legacy instances
- **Mitigation:** Document manual conversion process in `docs/standardization/`

### Observation: ResultPayload Completeness
- **Status:** `result_serializer.h` already implements ALL fields required by Phase 1.5 spec
- **Includes:**
  - run_uuid (UUIDv4) ✓
  - git_commit, git_dirty, seed ✓
  - config_digest ✓
  - runtime_stats (wall-clock, nodes, iterations) ✓
  - artifacts (log, result, warm-start paths) ✓
  - solver_metadata (formulation-specific JSON) ✓
- **Decision:** No additional work needed on result schema for Workstream 1

---

## 📋 Next Steps (Priority Order)

### ✅ Completed
1. ~~**Dual-run validation**~~ - DONE ✅
   - ✅ Executed both solvers on 25 quick_check instances
   - ✅ Compared objectives (100% match within 1e-6)
   - ✅ Verified edge identity (100% exact match)
   - ✅ Results stored in `experiments/validation/dual_run_20251103_225726/`
   - ✅ Created reusable validation script

### Immediate (This Week)
1. **Identify regression test set** - Formalize `quick_check` instances
   - Create `tests/data/regression/quick_checks.json` with instance list
   - Document expected optimal values in `tests/data/regression/metadata.json`
   - Use validation report as source of truth for expected values

### Short-term (Next 2 Weeks)
3. **Unit tests for common headers**
   - Set up Google Test framework
   - Write tests for `instance_loader` (parsing, validation, errors)
   - Write tests for `result_serializer` (serialization, validation)
   - Write tests for `common_types` (OCSTInstance methods)
   - Target: 80%+ coverage

4. **Performance benchmarking**
   - Create `scripts/standardization/benchmarks/loader_overhead.py`
   - Measure JSON parse time on quick_check set
   - Compare against control solver (if measurable)
   - Document results in benchmark report

### Medium-term (Before Promotion)
5. **Self-review checklist**
   - Run full test suite (unit + regression)
   - Performance validation (< 10% overhead)
   - Code review of all three headers
   - Documentation completeness check

6. **Promotion to src/cpp/common/**
   - Rename namespace: `ocst::path_based` → `ocst::common`
   - Update include guards: `OCST_PATH_BASED_*` → `OCST_COMMON_*`
   - Move files to `src/cpp/common/`
   - Update all includes in `path_based_formulation/algorithm.cpp`
   - Update Makefile with `-Isrc/cpp/common` flag

7. **Begin Workstream 2 (Common solver interface)**
   - Design `FormulationSolver` base class
   - Define lifecycle hooks (configure, solve, collect_metrics)
   - Plan refactoring strategy for existing formulations

---

## ✅ Technical Decisions (2025-11-10)

### Decision #1: UUID Library - **sole** (APPROVED)
- **Question:** What UUIDv4 library should we use for `run_uuid`?
- **Options evaluated:** sole, stduuid, Boost.UUID, DIY implementation
- **Decision:** **sole** (https://github.com/r-lyeh-archived/sole)
- **Rationale:**
  - ✅ **Single-header library** - Zero build complexity
  - ✅ **Lightweight** - No dependencies, ~500 lines of code
  - ✅ **MIT License** - Compatible with research projects
  - ✅ **Production-proven** - Used in multiple C++ projects
  - ❌ Boost.UUID - Too heavy (entire Boost dependency)
  - ❌ stduuid - Requires C++17, more complex integration
  - ❌ DIY - Unnecessary reinvention, security risks
- **Implementation:** 
  - Download `sole.hpp` to `third_party/sole/`
  - Include guard: `#include "sole.hpp"`
  - Usage: `std::string uuid = sole::uuid4().str();`
- **Status:** ⏳ Pending integration

### Decision #2: Git Hash Extraction - **Makefile + build_info.h** (APPROVED)
- **Question:** How do we extract git commit hash at runtime?
- **Options evaluated:** 
  1. Makefile generates `build_info.h` with preprocessor macros
  2. Runtime `system("git rev-parse --short HEAD")`
  3. CMake approach
- **Decision:** **Makefile generates `build_info.h`**
- **Rationale:**
  - ✅ **Build-time extraction** - No runtime dependency on git binary
  - ✅ **Reproducible** - Hash captured at compile time, immutable
  - ✅ **Deployment-friendly** - Works in environments without git
  - ✅ **Makefile integration** - We already use Make, no new tools
  - ❌ Runtime system() - Fails if git not installed, slower, fragile
  - ❌ CMake - Would require migration from Make
- **Implementation:**
  ```makefile
  # Makefile rule to generate build_info.h
  src/cpp/common/build_info.h: FORCE
      @echo "Generating build_info.h..."
      @mkdir -p src/cpp/common
      @echo "#ifndef OCST_COMMON_BUILD_INFO_H" > $@
      @echo "#define OCST_COMMON_BUILD_INFO_H" >> $@
      @echo "" >> $@
      @echo "#define GIT_COMMIT \"$(shell git rev-parse --short HEAD 2>/dev/null || echo unknown)\"" >> $@
      @echo "#define GIT_DIRTY $(shell git diff-index --quiet HEAD -- 2>/dev/null && echo false || echo true)" >> $@
      @echo "#define BUILD_TIMESTAMP \"$(shell date -u +%Y-%m-%dT%H:%M:%SZ)\"" >> $@
      @echo "" >> $@
      @echo "#endif // OCST_COMMON_BUILD_INFO_H" >> $@
  ```
- **Usage in C++:**
  ```cpp
  #include "build_info.h"
  payload.reproducibility.git_commit = GIT_COMMIT;
  payload.reproducibility.git_dirty = GIT_DIRTY;
  payload.reproducibility.timestamp = BUILD_TIMESTAMP;
  ```
- **Status:** ⏳ Pending implementation

### Decision #3: CSV Export - **Python Post-Processor** (APPROVED)
- **Question:** Should we implement CSV export for results in addition to JSON?
- **Options evaluated:**
  1. Add CSV writer to `result_serializer.h` (C++)
  2. Separate Python post-processor
- **Decision:** **Python post-processor** (`scripts/standardization/results_to_csv.py`)
- **Rationale:**
  - ✅ **Separation of concerns** - C++ does computation, Python does transformation
  - ✅ **Flexibility** - Easy to modify CSV schema without recompiling
  - ✅ **JSON is source of truth** - C++ only maintains one canonical format
  - ✅ **Pandas integration** - Natural for aggregation/analysis
  - ✅ **Maintainability** - CSV logic separate from solver code
  - ❌ C++ CSV writer - Adds complexity, harder to maintain, duplicates logic
- **Implementation:**
  - Script: `scripts/standardization/results_to_csv.py`
  - Input: `experiments/**/standardized/*.results.json`
  - Output: `experiments/**/standardized/summary.csv`
  - Fields: instance_name, solver_id, objective, runtime, status, gap_percent
  - Optional: `--aggregate` flag for multi-run summaries
- **Status:** ⏳ Pending implementation

### Decision #4: Adjacency Matrix in OCSTInstance - **KEEP CURRENT** (CONFIRMED)
- **Question:** Should `OCSTInstance` maintain O(n²) adjacency matrix?
- **Current:** O(n²) space for adjacency matrix + edge index map
- **Alternative:** On-demand edge lookups with O(m) space
- **Decision:** **Keep current approach**
- **Rationale:**
  - ✅ **O(1) edge queries** - Critical for Gurobi model building
  - ✅ **Proven in practice** - Works well for current instance sizes (n < 200)
  - ✅ **Formulation requirements** - Path-based formulation needs fast edge lookups
  - ❌ Space concern valid but not critical at current scale
- **Status:** ✅ No action needed

---

## 📝 Open Questions

*All major technical decisions for Workstream 1 have been resolved. Future questions will be added here.*

---

## 📚 References

- **Phase 1.5 spec:** `.cursor/rules/Phase 1.5 – Standardization Baseline.mdc`
- **JSON schemas:** `schemas/*.schema.v1.json`
- **Code standards:** `ia_standards/CODE_STANDARDS.md`
- **Project structure:** `ia_standards/PROJECT_STRUCTURE.md`
- **Control solver:** `src/cpp/algorithms/path_based_formulation_original/`
- **Migrating solver:** `src/cpp/algorithms/path_based_formulation/`

---

## 📊 Workstream Timeline Estimate

```
Workstream 1 (Data model & loaders):  ████████░░ 95% complete ⚡⚡
  ├─ JSON schemas                     ✅ Done
  ├─ Instance conversion              ✅ Done
  ├─ Common headers (prototype)       ✅ Done
  ├─ Dual-run validation              ✅ Done (100% parity achieved!)
  ├─ Unit tests                       ✅ Done (62/62 tests, 100% pass rate!)
  ├─ Technical integrations           ✅ Done (UUID, build_info, CSV export)
  ├─ Performance benchmarks           ⏳ Pending (1 week)
  └─ Promotion to src/cpp/common/     ⏳ Ready (pending benchmarks)

Workstream 2 (Solver interface):      ░░░░░░░░░░ 0% (awaiting W1 completion)
Workstream 3 (Results schema):        ████████░░ 80% (ResultPayload done, tested, CSV export ready)
Workstream 4 (Config & logging):      ░░░░░░░░░░ 0% (deferred)
Workstream 5 (Experiment pipeline):   ░░░░░░░░░░ 0% (deferred)
```

---

## 📊 Validation Summary (2025-11-04)

**Status:** ✅ PASSED (100% parity)  
**Instances:** 25 quick_check instances  
**Tolerance:** 1e-6

| Metric | Result |
|--------|--------|
| Objectives | 25/25 (100%) |
| Edge Identity | 25/25 (100%) |
| Numerical Precision | All diffs = 0.0 |

**Artifacts:**
- Script: `scripts/standardization/validate_path_based_migration.py`
- Report: `experiments/validation/dual_run_20251103_225726/`

**Deep Dive - Numerical Precision:**

✅ **Gurobi Configuration:** Both solvers identical
- NumericFocus = 2
- MIPGap = 1e-6
- MIPGapAbs = 1e-6

✅ **Objective Construction:** Identical in both solvers
```cpp
objective += req.weight * edge.cost * y_vars_[r][2*e];
```

✅ **Value Retrieval:** Identical from Gurobi
```cpp
result.objective_value = model_.get(GRB_DoubleAttr_ObjVal);
```

⚠️ **Output Handling Difference:**
- Control: Uses `setprecision(0)` when writing (rounds at I/O)
- Migrating: Uses `std::round()` before storing (rounds in memory)
- **Impact:** None for integer-cost instances (all current data)
- **Note:** Both produce same final values

**Conclusion:** Ready for promotion after unit tests.

---

## ✍️ Changelog

### 2025-11-10 - Unit Tests & Technical Integrations Completed
- ✅ **Unit Tests:** 62/62 tests implemented and passing (100% success rate)
  - common_types_test.cpp (20 tests)
  - instance_loader_test.cpp (17 tests)
  - result_serializer_test.cpp (25 tests)
- ✅ **Google Test Integration:** Compiled from source without CMake dependency
- ✅ **UUID Library:** sole.hpp integrated in third_party/
- ✅ **Build Info System:** Makefile auto-generates git hash + timestamp
- ✅ **CSV Export Tool:** Python post-processor for result JSON files
- ✅ **Technical Decisions Documented:** UUID, Git Hash, CSV Export strategies
- ✅ **Code Improvements:** Enhanced validation with descriptive error messages
- 📊 Workstream 1 now at ~95% completion (pending: performance benchmarks only)

### 2025-11-04 - Dual-Run Validation Completed
- ✅ Created validation script with 1e-6 numerical tolerance
- ✅ Executed dual-run experiments on 25 quick_check instances
- ✅ Achieved 100% parity (objectives + edges + precision)
- ✅ Fixed .sol parser bug (multi-line format)
- ✅ Documented validation results and artifacts
- ✅ Updated workstream status: validation PASSED
- 📊 Workstream 1 now at ~90% completion (pending: unit tests, performance benchmarks)

### 2025-11-04 - Initial Document
- Created progress log for Phase 1.5
- Documented current state of Workstream 1
- Identified completed components (schemas, headers, instance data)
- Listed pending tasks (unit tests, benchmarks, validation)
- Noted open questions (UUID library, git hash extraction, CSV export)
- Estimated 80% completion for Workstream 1 (validation phase)

---

**Next Review:** After dual-run validation (1 week)  
**Document Owner:** Sergio  
**Status:** 🟢 Active Development

