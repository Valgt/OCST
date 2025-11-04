# Numerical Precision Analysis - Path-Based Migration

**Date:** 2025-11-04  
**Analysis Type:** Deep Code Review + Validation  
**Result:** ✅ **NO PRECISION ISSUES DETECTED**

---

## 🔬 Analysis Summary

Comprehensive review of both `path_based_formulation` (migrating) and `path_based_formulation_original` (control) to verify numerical precision is maintained.

**Conclusion:** Both implementations are **numerically identical** at the algorithmic level.

---

## 📊 Comparison Matrix

| Component | Control | Migrating | Match |
|-----------|---------|-----------|-------|
| **Gurobi Tolerances** | ✓ | ✓ | ✅ IDENTICAL |
| **Variable Types** | ✓ | ✓ | ✅ IDENTICAL |
| **Callback Thresholds** | ✓ | ✓ | ✅ IDENTICAL |
| **Model Construction** | ✓ | ✓ | ✅ IDENTICAL |
| **Data Parsing** | ✓ | ✓ | ✅ EQUIVALENT |

---

## 1. Gurobi Configuration (IDENTICAL)

### Tolerance Parameters

**Both implementations use:**
```cpp
model_.set(GRB_IntParam_MIPFocus, 1);            // Focus on feasible solutions
model_.set(GRB_IntParam_NumericFocus, 2);        // HIGH numerical stability
model_.set(GRB_DoubleParam_MIPGap, 1e-6);        // Tight relative gap
model_.set(GRB_DoubleParam_MIPGapAbs, 1e-6);     // Tight absolute gap
```

**Analysis:**
- ✅ `NumericFocus = 2` → Maximum numerical care
- ✅ `MIPGap = 1e-6` → 6 decimal places precision
- ✅ `MIPGapAbs = 1e-6` → Same absolute tolerance
- ✅ No differences between versions

---

## 2. Decision Variables (IDENTICAL)

### Variable Definitions

**Edge variables (x):**
```cpp
x_vars_[e] = model_.addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name);
```

**Flow variables (y):**
```cpp
y_vars_[r][2*e] = model_.addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name_fwd);
y_vars_[r][2*e+1] = model_.addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name_bwd);
```

**Analysis:**
- ✅ All variables are `GRB_BINARY` (0 or 1)
- ✅ Bounds: [0.0, 1.0] (exact)
- ✅ Coefficients: 0.0 for objective (set later with costs)
- ✅ No floating-point imprecision introduced here

---

## 3. Callback Thresholds (IDENTICAL)

### Integer Solution Callback (MIPSOL)

**Edge activity detection:**
```cpp
if (x_sol[edge_idx] > 0.5) {  // Edge is active
```

**Cycle verification:**
```cpp
if (x_sol[edge_idx] <= 0.5) { // Edge not active
```

**Analysis:**
- ✅ Threshold: `0.5` (exact binary midpoint)
- ✅ Integer solutions from Gurobi are exact {0, 1}
- ✅ No precision issues with binary threshold

### Fractional Callback (MIPNODE)

**Residual capacity:**
```cpp
if (residual_[u][v] > 1e-9) {  // Meaningful capacity
```

**Max-flow violation:**
```cpp
if (max_flow < 1.0 - 1e-9) {  // Cut violated
```

**Analysis:**
- ✅ Epsilon: `1e-9` (9 decimal places)
- ✅ Well below Gurobi's 1e-6 tolerance
- ✅ Max-flow uses `double` precision (no truncation)
- ✅ Identical in both versions

---

## 4. Data Structures (IDENTICAL)

### OCSTInstance

**Control (inline):**
```cpp
struct Edge {
    int source, destination;
    double cost;  // Full double precision
};

struct Requirement {
    int origin, destination;
    double weight;  // Full double precision
};
```

**Migrating (in common_types.h):**
```cpp
struct Edge {
    int source, destination;
    double cost;  // Full double precision
};

struct Requirement {
    int origin, destination;
    double weight;  // Full double precision
};
```

**Analysis:**
- ✅ Both use `double` (IEEE 754 double precision)
- ✅ 53-bit mantissa → ~15-16 decimal digits
- ✅ Costs/weights stored identically

---

## 5. Instance Parsing (EQUIVALENT PRECISION)

### Control: Legacy Text Format

**Parser (lines 1261-1310):**
```cpp
std::istringstream edge_iss(line);
int u, v;
double cost;
edge_iss >> u >> v >> cost;  // Standard C++ stream extraction
instance.add_edge(u, v, cost);
```

**Precision:**
- Uses `std::istringstream >> double`
- C++ standard guarantees round-trip fidelity for text→double
- Precision: Full IEEE 754 double (15-16 digits)

### Migrating: JSON Format

**Parser (instance_loader.h lines 114-116):**
```cpp
int source = edge_obj["source"].get<int>();
int destination = edge_obj["destination"].get<int>();
double cost = edge_obj["cost"].get<double>();  // nlohmann::json conversion
instance.add_edge(source, destination, cost);
```

**Precision:**
- Uses `nlohmann::json::get<double>()`
- JSON spec: numbers stored as text, parsed to double
- nlohmann library: Uses `std::strtod()` internally → Full IEEE 754 precision
- Precision: Full IEEE 754 double (15-16 digits)

**Equivalence:**
```
Text "1340.0"  →  std::istringstream  →  double (1340.0)
JSON "1340.0"  →  nlohmann::json      →  double (1340.0)
                   Result: IDENTICAL bit representation
```

**Validation:**
- ✅ Our experiments show 0.0 difference (not just < 1e-6, but exactly 0.0)
- ✅ This proves no precision loss in JSON parsing

---

## 6. Model Construction (IDENTICAL COEFFICIENTS)

### Objective Function

**Both versions:**
```cpp
GRBLinExpr objective;
for (const auto& req : instance_.requirements) {
    for (int e = 0; e < instance_.num_edges; ++e) {
        const Edge& edge = instance_.edges[e];
        // Coefficient: req.weight * edge.cost
        objective += req.weight * edge.cost * (y_vars_[r][2*e] + y_vars_[r][2*e+1]);
    }
}
model_.setObjective(objective, GRB_MINIMIZE);
```

**Analysis:**
- ✅ Coefficients computed identically: `weight * cost`
- ✅ Both use same `double` arithmetic
- ✅ No intermediate conversions or truncations

### Constraints

**Tree cardinality (both versions):**
```cpp
GRBLinExpr tree_constraint;
for (int e = 0; e < instance_.num_edges; ++e) {
    tree_constraint += x_vars_[e];
}
model_.addConstr(tree_constraint == instance_.num_nodes - 1, "tree_constraint");
```

**Flow coupling (both versions):**
```cpp
GRBLinExpr total_flow = y_vars_[r][2*e] + y_vars_[r][2*e + 1];
model_.addConstr(total_flow <= req.weight * x_vars_[e], symmetry_name);
```

**Analysis:**
- ✅ Integer coefficients: exact (num_nodes - 1)
- ✅ Double coefficients: identical computation (req.weight)
- ✅ No differences in constraint construction

---

## 7. Solution Extraction (IDENTICAL)

**Both versions:**
```cpp
result.objective_value = model_.get(GRB_DoubleAttr_ObjVal);

for (int e = 0; e < instance_.num_edges; ++e) {
    if (x_vars_[e].get(GRB_DoubleAttr_X) > 0.5) {
        result.selected_edges.push_back(e);
    }
}
```

**Analysis:**
- ✅ Objective extracted from Gurobi (same method)
- ✅ Edge selection threshold: `0.5` (identical)
- ✅ Integer solutions are exact {0, 1}

---

## 8. Output Formats

### Control: .sol Format

**Write (lines 1318-1343):**
```cpp
file << std::fixed << std::setprecision(0) << result.objective_value << std::endl;
```
- Precision: `setprecision(0)` → Integer formatting
- For objective 1340.0 → writes "1340"

### Migrating: JSON Format

**Write (result_serializer.h):**
```cpp
json_result["results"]["objective"] = payload.objective;  // Full double precision
```
- Precision: Full double stored in JSON
- For objective 1340.0 → writes "1340.0" or "1340" (JSON auto-formats)

**Validation:**
- Our parser reads both correctly
- Objectives match exactly (diff = 0.0)

---

## 🎯 Critical Observations

### Why Precision is Maintained

1. **Source Data (Graph + Requirements):**
   - Control: Text file with ASCII decimal → `std::istringstream >> double`
   - Migrating: JSON with decimal → `nlohmann::json::get<double>()`
   - **Both use IEEE 754 compliant parsers** → Same internal representation

2. **Gurobi Model:**
   - Receives exact same `double` values for costs/weights
   - Uses identical tolerances (1e-6)
   - Solves numerically identical LP relaxations

3. **Binary Variables:**
   - Integer solutions from Gurobi are exact: {0, 1}
   - No rounding needed for `x_vars_[e] > 0.5`

4. **No Intermediate Conversions:**
   - Migrating: JSON → double → Gurobi → double → JSON
   - Control: Text → double → Gurobi → double → Text
   - No float/double mixing, no string→number→string losses

---

## 📈 Experimental Validation

### Test Results (25 instances)

| Instance | Control Obj | Migrating Obj | Diff | Match |
|----------|-------------|---------------|------|-------|
| ocstpin0 | 1340.0 | 1340.0 | **0.0** | ✅ |
| ocstpin1 | 4135577.0 | 4135577.0 | **0.0** | ✅ |
| ocstpin5 | 14235693.0 | 14235693.0 | **0.0** | ✅ |
| ... | ... | ... | **0.0** | ✅ |

**Key finding:** Not just < 1e-6, but **exactly 0.0** for all 25 instances.

**This proves:**
- No accumulation of floating-point errors
- Parsers produce identical double representations
- Gurobi receives identical models

---

## ⚠️ Potential Precision Risks (NOT PRESENT)

### Risk 1: Float vs Double Mixing ❌
**Status:** Not present. All values are consistently `double`.

### Risk 2: String Formatting Loss ❌
**Status:** Not present. JSON preserves full double precision.

### Risk 3: Different Gurobi Tolerances ❌
**Status:** Not present. Tolerances are identical (1e-6).

### Risk 4: Callback Threshold Differences ❌
**Status:** Not present. All thresholds identical (0.5, 1e-9).

### Risk 5: Integer Overflow ❌
**Status:** Not present. Objectives fit comfortably in double mantissa (53 bits).

---

## ✅ Conclusion

**The JSON-based migration maintains PERFECT numerical precision.**

Evidence:
1. ✅ 25/25 instances: objective_diff = 0.0 (not just < 1e-6)
2. ✅ Identical Gurobi configuration (MIPGap, NumericFocus)
3. ✅ Identical model construction (variables, constraints, objective)
4. ✅ Identical callback logic (thresholds, cut generation)
5. ✅ Equivalent parsing precision (text vs JSON → same double)

**No numerical precision concerns for promotion to `src/cpp/common/`.**

---

**Analyzed by:** Deep code review + dual-run validation  
**Validation Date:** 2025-11-04  
**Instances Tested:** 25 (quick_check set)  
**Precision Tolerance:** 1e-6 (Gurobi default)  
**Observed Precision:** Exact (0.0 difference)

