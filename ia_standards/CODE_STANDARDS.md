# Code Standards - OCST Project

This document defines the coding rules that **MUST NOT change** throughout the project. These are fixed standards that ensure code consistency and quality.

**⚠️ CRITICAL RULE: ALL code, comments, documentation, variable names, function names, and any text in the codebase MUST be written in English. No exceptions.**

## 1. Language Requirements

### Mandatory English Usage
- **All code**: Variables, functions, classes, constants, file names
- **All comments**: Inline comments, documentation blocks, TODO comments
- **All documentation**: README files, technical notes, API documentation
- **All configuration**: YAML files, JSON configs, command line parameters
- **All output**: Log messages, error messages, console output
- **All tests**: Test names, assertions, test documentation

### Examples
```cpp
// ✅ CORRECT - English only
class OCSTSolver {
private:
    int num_nodes;
    double objective_value;
    std::vector<Edge> tree_edges;
    
public:
    /**
     * @brief Solves the OCST problem using Branch and Bound
     * @param instance The OCST instance to solve
     * @return Solution The optimal solution found
     */
    Solution solve(const OCSTInstance& instance);
};

// ❌ INCORRECT - Spanish/other languages
class ResolverdorOCST {  // Spanish class name
private:
    int numero_nodos;    // Spanish variable
    // Resolver el problema  // Spanish comment
};
```

## 2. Naming Conventions

### Variables and Functions
- **Style**: `snake_case`
- **Language**: English only
- **Examples**:
  ```cpp
  int num_nodes;
  double objective_value;
  void solve_ocst_instance();
  bool is_feasible_solution();
  std::string get_algorithm_name();
  ```

### Classes and Structs
- **Style**: `PascalCase`
- **Language**: English only
- **Examples**:
  ```cpp
  class OCSTSolver;
  struct GraphInstance;
  class MetricsCollector;
  struct AlgorithmConfig;
  ```

### Constants and Enums
- **Style**: `UPPER_SNAKE_CASE`
- **Language**: English only
- **Examples**:
  ```cpp
  const int MAX_ITERATIONS = 1000;
  const double EPSILON = 1e-9;
  const std::string DEFAULT_CONFIG_FILE = "config.yaml";
  
  enum AlgorithmType {
      BRANCH_AND_BOUND_A,
      BRANCH_AND_BOUND_B,
      HEURISTIC_GREEDY
  };
  ```

### Files and Directories

#### Core Project Files
- **Style**: `snake_case`
- **Language**: English only
- **Examples**:
  ```
  ocst_solver.cpp
  instance_parser.h
  metrics_collector.cpp
  algorithm_branch_bound_formulation_a/
  ```

#### AI Tools Interface Files
- **Style**: `UPPER_SNAKE_CASE` (all uppercase with underscores)
- **Purpose**: Files specifically designed for AI tool interaction, not core project functionality
- **Language**: English only
- **Examples**:
  ```
  CODE_STANDARDS.md
  PROJECT_STRUCTURE.md
  AI_PROMPTS.md
  DEVELOPMENT_GUIDELINES.md
  ```

**Note**: `UPPER_SNAKE_CASE` files are meta-documentation meant for AI assistance tools and development workflow, while `snake_case` files are core project components.

## 3. File Structure

### Headers (.h)
```cpp
#ifndef OCST_SOLVER_H
#define OCST_SOLVER_H

// 1. System includes (alphabetically ordered)
#include <iostream>
#include <vector>
#include <string>

// 2. Third-party library includes (alphabetically ordered)
#include <gurobi_c++.h>
#include <yaml-cpp/yaml.h>

// 3. Local includes (alphabetically ordered)
#include "graph_instance.h"
#include "metrics_collector.h"

namespace ocst {
    class OCSTSolver {
        // Class content
    };
}

#endif // OCST_SOLVER_H
```

### Implementation (.cpp)
```cpp
#include "ocst_solver.h"

// Additional includes if needed
#include <algorithm>
#include <chrono>

// Implementation
namespace ocst {
    // Implementation code
}
```

## 4. Class Organization

### Element Order in Classes
```cpp
class OCSTSolver {
public:
    // 1. Constructors and destructors
    OCSTSolver();
    explicit OCSTSolver(const AlgorithmConfig& config);
    ~OCSTSolver();
    
    // 2. Main public methods
    Solution solve(const OCSTInstance& instance);
    void reset();
    
    // 3. Getters and Setters
    const MetricsCollector& get_metrics() const;
    void set_time_limit(double limit);

protected:
    // 4. Protected methods (if inheritance exists)
    virtual void initialize_model();

private:
    // 5. Private methods
    void add_constraints();
    void optimize_model();
    
    // 6. Private attributes (with trailing underscore)
    GRBModel model_;
    MetricsCollector metrics_;
    AlgorithmConfig config_;
};
```

## 5. Code Documentation

### Function Comments (Doxygen style)
```cpp
/**
 * @brief Solves the OCST problem using Branch and Bound algorithm
 * @param instance The OCST instance to solve
 * @param time_limit Time limit in seconds (default: 3600)
 * @return Solution The optimal solution found
 * @throws OCSTException If the instance is invalid or Gurobi errors occur
 */
Solution solve(const OCSTInstance& instance, double time_limit = 3600.0);
```

### Inline Comments
```cpp
// Initialize Gurobi model
GRBEnv env = GRBEnv();
GRBModel model = GRBModel(env);

// Create decision variables x_ij for each edge
for (int i = 0; i < num_nodes; ++i) {
    for (int j = i + 1; j < num_nodes; ++j) {
        // Only create variables for existing edges
        if (instance.has_edge(i, j)) {
            x_vars[i][j] = model.addVar(0.0, 1.0, edge_costs[i][j], GRB_BINARY);
        }
    }
}
```

### Section Comments
```cpp
//=============================================================================
// GUROBI MODEL INITIALIZATION
//=============================================================================

//-----------------------------------------------------------------------------
// Variable Creation
//-----------------------------------------------------------------------------
```

## 6. Error Handling

### Custom Exceptions
```cpp
class OCSTException : public std::exception {
private:
    std::string message_;
    
public:
    explicit OCSTException(const std::string& msg) : message_(msg) {}
    const char* what() const noexcept override { return message_.c_str(); }
};

// Usage
if (instance.num_nodes() <= 0) {
    throw OCSTException("Instance must have at least one node");
}
```

### Status Codes
```cpp
enum class SolutionStatus {
    OPTIMAL,
    TIME_LIMIT_REACHED,
    INFEASIBLE,
    UNBOUNDED,
    ERROR
};
```

## 7. Configuration (YAML)

### Standard Configuration Structure
```yaml
# config/algorithm_config.yaml
algorithm:
  name: "Branch and Bound A"
  time_limit: 3600.0
  mip_gap: 1e-4
  max_iterations: 10000

gurobi:
  threads: 4
  presolve: 2
  cuts: 1
  heuristics: 1
  method: 1

logging:
  level: "INFO"
  output_file: "logs/solver.log"
  console_output: true

output:
  save_solution: true
  save_metrics: true
  export_format: ["json", "csv"]
```

### Configuration Loading
```cpp
struct AlgorithmConfig {
    std::string algorithm_name = "Default";
    double time_limit = 3600.0;
    double mip_gap = 1e-4;
    int max_iterations = 10000;
    
    static AlgorithmConfig load_from_yaml(const std::string& filename);
    void save_to_yaml(const std::string& filename) const;
};
```

## 8. Code Formatting

### Indentation and Spacing
- **Indentation**: 4 spaces (no tabs)
- **Maximum line length**: 100 characters
- **Spaces**: Around operators and after commas

```cpp
// ✅ CORRECT
int result = a + b * c;
bool condition = (x > 0) && (y < 10);
function_call(param1, param2, param3);

// ❌ INCORRECT
int result=a+b*c;
bool condition=(x>0)&&(y<10);
function_call(param1,param2,param3);
```

### Braces and Blocks
```cpp
// ✅ CORRECT - Allman/BSD style
if (condition) 
{
    do_something();
    if (nested_condition) 
    {
        do_nested_action();
    }
}

// For short functions, same line is acceptable
void simple_function() { return value_; }
```

### Long Parameter Lists
```cpp
void long_function_name(const OCSTInstance& instance,
                       const AlgorithmConfig& config,
                       MetricsCollector* metrics,
                       const std::string& output_file);
```

## 9. Logging System

### Log Levels
```cpp
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3
};

// Standard usage - ALL MESSAGES IN ENGLISH
Logger::log(LogLevel::INFO, "Starting OCST solver for instance: " + instance_name);
Logger::log(LogLevel::DEBUG, "Created " + std::to_string(num_vars) + " variables");
Logger::log(LogLevel::WARNING, "Time limit approaching: " + std::to_string(remaining_time));
Logger::log(LogLevel::ERROR, "Gurobi optimization failed: " + error_message);
```

## 10. Testing

### Test Structure
```cpp
#include <gtest/gtest.h>
#include "ocst_solver.h"

class OCSTSolverTest : public ::testing::Test {
protected:
    void SetUp() override {
        solver_ = std::make_unique<OCSTSolver>(config_);
        small_instance_ = OCSTInstance::load_from_file("data/test/small_01.ocst");
    }
    
    std::unique_ptr<OCSTSolver> solver_;
    OCSTInstance small_instance_;
    AlgorithmConfig config_;
};

TEST_F(OCSTSolverTest, SolvesSmallInstanceOptimally) {
    Solution solution = solver_->solve(small_instance_);
    
    EXPECT_EQ(solution.status, SolutionStatus::OPTIMAL);
    EXPECT_GT(solution.objective_value, 0.0);
    EXPECT_EQ(solution.tree_edges.size(), small_instance_.num_nodes() - 1);
}
```

### Test Naming
```cpp
// Pattern: TestClass_MethodUnderTest_ExpectedBehavior
TEST_F(OCSTSolverTest, Solve_WithSmallInstance_ReturnsOptimalSolution);
TEST_F(OCSTSolverTest, Solve_WithTimeLimit_RespectsTimeConstraint);
TEST_F(OCSTSolverTest, Constructor_WithInvalidConfig_ThrowsException);
```

## 11. Configuration Files

### `.clang-format`
```yaml
BasedOnStyle: Google
IndentWidth: 4
ColumnLimit: 100
AccessModifierOffset: -4
BreakBeforeBraces: Allman
AllowShortFunctionsOnASingleLine: Empty
AllowShortIfStatementsOnASingleLine: false
```

### Compilation and Flags
```makefile
# Standard compilation flags
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g
LDFLAGS = -lgurobi_c++ -lgurobi120 -lpthread -lm

# For debug builds
DEBUG_FLAGS = -std=c++17 -Wall -Wextra -g -DDEBUG -fsanitize=address
```

---

## Summary of Fixed Standards

- ✅ **Language**: All code and documentation MUST be in English
- ✅ **Naming**: snake_case for variables/functions, PascalCase for classes
- ✅ **File naming**: snake_case for core files, UPPER_SNAKE_CASE for AI tools files
- ✅ **Indentation**: 4 spaces, maximum 100 characters per line  
- ✅ **Documentation**: Doxygen style for public functions
- ✅ **Configuration**: YAML as standard format
- ✅ **Errors**: Exceptions for critical errors, enums for states
- ✅ **Testing**: Google Test with consistent naming
- ✅ **Logging**: Standard level system with English messages
- ✅ **Headers**: Fixed order for includes and class elements

**These standards DO NOT change during project development.**
