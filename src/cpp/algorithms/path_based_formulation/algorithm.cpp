#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <set>
#include <iomanip>
#include <limits>
#include <filesystem>
#include <queue>
#include <memory>
#include <stack>
#include <unordered_set>

// Unified instance loader (supports both legacy and JSON formats)
#include "instance_loader.h"

// Result serializer for JSON output
#include "result_serializer.h"

// Common solver interface (Workstream 2 - Simplified)
#include "formulation_solver.h"

// Warm start loader (Phase 2.0)
#include "warm_start_loader.h"

// UUID generation
#include "sole/sole.hpp"

// Gurobi integration
#include <gurobi_c++.h>

//=============================================================================
// DATA STRUCTURES
//=============================================================================

// Data structures are now in include/common_types.h
// Using aliases for backward compatibility with existing code
using Edge = ocst::common::Edge;
using Requirement = ocst::common::Requirement;
using OCSTInstance = ocst::common::OCSTInstance;
using ResultPayload = ocst::common::ResultPayload;
using ResultSerializer = ocst::common::ResultSerializer;
using SolverConfig = ocst::common::SolverConfig;

//=============================================================================
// MAX-FLOW MIN-CUT FOR FRACTIONAL SEC SEPARATION
//=============================================================================

/**
 * @brief Simple max-flow min-cut implementation for fractional SEC separation
 * CRITICAL FIX: Uses double precision to avoid truncation of small fractional values
 */
class MaxFlowMinCut
{
private:
    int num_nodes_;
    std::vector<std::vector<double>> capacity_;
    std::vector<std::vector<double>> residual_;
    
public:
    MaxFlowMinCut(int num_nodes) : num_nodes_(num_nodes)
    {
        capacity_.assign(num_nodes_, std::vector<double>(num_nodes_, 0.0));
        residual_.assign(num_nodes_, std::vector<double>(num_nodes_, 0.0));
    }
    
    void add_edge(int u, int v, double cap)
    {
        capacity_[u][v] = cap;
        residual_[u][v] = cap;
    }
    
    double max_flow(int source, int sink)
    {
        // Edmonds-Karp algorithm with double precision
        double max_flow = 0.0;
        
        while (true) {
            std::vector<int> parent(num_nodes_, -1);
            std::queue<int> q;
            q.push(source);
            parent[source] = source;
            
            while (!q.empty() && parent[sink] == -1) {
                int u = q.front();
                q.pop();
                
                for (int v = 0; v < num_nodes_; ++v) {
                    if (parent[v] == -1 && residual_[u][v] > 1e-9) {  // Use small epsilon for double comparison
                        parent[v] = u;
                        q.push(v);
                    }
                }
            }
            
            if (parent[sink] == -1) break;  // No augmenting path
            
            // Find bottleneck capacity
            double bottleneck = std::numeric_limits<double>::max();
            int v = sink;
            while (v != source) {
                int u = parent[v];
                bottleneck = std::min(bottleneck, residual_[u][v]);
                v = u;
            }
            
            // Update residual capacities
            v = sink;
            while (v != source) {
                int u = parent[v];
                residual_[u][v] -= bottleneck;
                residual_[v][u] += bottleneck;
                v = u;
            }
            
            max_flow += bottleneck;
        }
        
        return max_flow;
    }
    
    std::vector<int> get_min_cut(int source)
    {
        // Find reachable nodes from source in residual graph
        std::vector<bool> visited(num_nodes_, false);
        std::queue<int> q;
        q.push(source);
        visited[source] = true;
        
        while (!q.empty()) {
            int u = q.front();
            q.pop();
            
            for (int v = 0; v < num_nodes_; ++v) {
                if (!visited[v] && residual_[u][v] > 1e-9) {  // Use small epsilon for double comparison
                    visited[v] = true;
                    q.push(v);
                }
            }
        }
        
        std::vector<int> cut;
        for (int i = 0; i < num_nodes_; ++i) {
            if (visited[i]) {
                cut.push_back(i);
            }
        }
        
        return cut;
    }
    };

/**
 * @brief SEC Callback for Path-Based Formulation
 * Prevents subtours and disconnected components
 */
class SECCallback : public GRBCallback
{
private:
    const OCSTInstance& instance_;
    const std::vector<GRBVar>& x_vars_;
    int& lazy_constraints_count_;
    
public:
    SECCallback(const OCSTInstance& instance, const std::vector<GRBVar>& x_vars, int& lazy_count)
        : instance_(instance), x_vars_(x_vars), lazy_constraints_count_(lazy_count) {}
    
protected:
    void callback() override
    {
        if (where == GRB_CB_MIPSOL) {
            add_lazy_constraints();
        } else if (where == GRB_CB_MIPNODE) {
            add_fractional_cuts();
        }
    }
    
private:
    void add_lazy_constraints()
    {
        try {
            // Get current solution
            std::vector<double> x_sol(instance_.num_edges);
            for (int e = 0; e < instance_.num_edges; ++e) {
                x_sol[e] = getSolution(x_vars_[e]);
            }
            
            // CRITICAL FIX: Detect cycles first (like SECInteger::FindCycle)
            std::vector<int> cycle = find_cycle(x_sol);
            if (!cycle.empty()) {
                std::cout << "SEC Callback: Found cycle of size " << cycle.size() << ": ";
                for (int node : cycle) {
                    std::cout << node << " ";
                }
                std::cout << std::endl;
                
                // Verify the cycle actually exists in the solution
                if (verify_cycle_exists(cycle, x_sol)) {
                    std::cout << "Cycle verification: PASSED" << std::endl;
                    add_cycle_constraint(cycle);
                    return;  // Cycle constraint is stronger than component constraints
                } else {
                    std::cout << "Cycle verification: FAILED - false positive" << std::endl;
                }
            }
            
            // Find connected components using DFS
            std::vector<bool> visited(instance_.num_nodes, false);
            std::vector<std::vector<int>> components;
            
            for (int i = 0; i < instance_.num_nodes; ++i) {
                if (!visited[i]) {
                    std::vector<int> component;
                    dfs_component(i, visited, component, x_sol);
                    components.push_back(component);
                }
            }
            
            std::cout << "SEC Callback: Found " << components.size() << " components" << std::endl;
            
            // Add connectivity constraints for non-trivial components
            for (const auto& component : components) {
                std::cout << "Component size: " << component.size() << std::endl;
                if (component.size() > 1 && component.size() < static_cast<size_t>(instance_.num_nodes)) {
                    std::cout << "Adding connectivity constraint for component of size " << component.size() << std::endl;
                    add_connectivity_constraint(component);
                }
            }
            
        } catch (GRBException& e) {
            // If callback fails, continue without adding constraints
            std::cerr << "SEC Callback error: " << e.getMessage() << std::endl;
        }
    }
    
    void dfs_component(int node, std::vector<bool>& visited, std::vector<int>& component, 
                      const std::vector<double>& x_sol)
    {
        visited[node] = true;
        component.push_back(node);
        
        for (int j = 0; j < instance_.num_nodes; ++j) {
            if (!visited[j] && instance_.has_edge(node, j)) {
                int edge_idx = instance_.get_edge_index(node, j);
                if (edge_idx >= 0 && x_sol[edge_idx] > 0.5) {
                    dfs_component(j, visited, component, x_sol);
                }
            }
        }
    }
    
    void add_sec_constraint(const std::vector<int>& component)
    {
        GRBLinExpr cut_expr;
        
        // OPTIMIZATION: Use bool vector for O(1) membership check
        std::vector<bool> in_component(instance_.num_nodes, false);
        for (int v : component) {
            in_component[v] = true;
        }
        
        // OPTIMIZATION: Iterate only over edges incident to component vertices
        // Use adjacency matrix for O(1) edge lookup instead of O(m) iteration
        for (size_t i = 0; i < component.size(); ++i) {
            int u = component[i];
            for (size_t j = i + 1; j < component.size(); ++j) {
                int v = component[j];
                
                int edge_idx = instance_.get_edge_index(u, v);
                if (edge_idx >= 0) {
                    cut_expr += x_vars_[edge_idx];
                }
            }
        }
        
        // Add constraint: sum of edges in component <= |component| - 1
        if (cut_expr.size() > 0) {
            addLazy(cut_expr <= static_cast<double>(component.size()) - 1.0);
            lazy_constraints_count_++;
        }
    }
    
    /**
     * @brief Finds a cycle in the current solution (like SECInteger::FindCycle)
     */
    std::vector<int> find_cycle(const std::vector<double>& x_sol)
    {
        // Build adjacency list from active edges
        std::vector<std::vector<int>> adj(instance_.num_nodes);
        for (int e = 0; e < instance_.num_edges; ++e) {
            if (x_sol[e] > 0.5) {  // Edge is active
                const Edge& edge = instance_.edges[e];
                adj[edge.source].push_back(edge.destination);
                adj[edge.destination].push_back(edge.source);
            }
        }
        
        // DFS to find back edges (cycles)
        std::vector<bool> visited(instance_.num_nodes, false);
        std::vector<int> parent(instance_.num_nodes, -1);
        std::vector<int> cycle;
        
        for (int start = 0; start < instance_.num_nodes; ++start) {
            if (!visited[start]) {
                std::stack<int> stack;
                stack.push(start);
                visited[start] = true;
                
                while (!stack.empty()) {
                    int u = stack.top();
                    stack.pop();
                    
                    for (int v : adj[u]) {
                        if (!visited[v]) {
                            visited[v] = true;
                            parent[v] = u;
                            stack.push(v);
                        } else if (parent[u] != v) {
                            // Found a back edge - cycle detected
                            // CRITICAL FIX: Reconstruct the complete cycle correctly using LCA
                            std::vector<int> cycle = reconstruct_cycle(u, v, parent);
                            if (!cycle.empty()) {
                                return cycle;
                            }
                        }
                    }
                }
            }
        }
        
        return cycle;  // Empty if no cycle found
    }
    
    /**
     * @brief Reconstructs a cycle from back edge (u,v) using LCA
     * CRITICAL FIX: Always returns the full ordered cycle
     */
    std::vector<int> reconstruct_cycle(int u, int v, const std::vector<int>& parent)
    {
        // Trace ancestors of u and v up to the root
        std::vector<int> pathU, pathV;
        
        // Build path from u to root
        for (int cur = u; cur != -1; cur = parent[cur]) {
            pathU.push_back(cur);
        }
        
        // Build path from v to root
        for (int cur = v; cur != -1; cur = parent[cur]) {
            pathV.push_back(cur);
        }
        
        // Find the lowest common ancestor (LCA)
        int lca = -1;
        std::unordered_set<int> pathUSet(pathU.begin(), pathU.end());
        
        for (int node : pathV) {
            if (pathUSet.find(node) != pathUSet.end()) {
                lca = node;
                break;
            }
        }
        
        if (lca == -1) {
            return {};  // No common ancestor found
        }
        
        // Reconstruct cycle: u → ... → LCA → ... → v → u
        std::vector<int> cycle;
        
        // Path from u to LCA
        for (int cur = u; cur != -1; cur = parent[cur]) {
            cycle.push_back(cur);
            if (cur == lca) break;
        }
        
        // Path from v to LCA (in reverse, skipping LCA)
        std::vector<int> pathVToLCA;
        for (int cur = v; cur != -1; cur = parent[cur]) {
            pathVToLCA.push_back(cur);
            if (cur == lca) break;
        }
        
        // Add path from v to LCA in reverse (excluding LCA)
        for (int i = static_cast<int>(pathVToLCA.size()) - 2; i >= 0; --i) {
            cycle.push_back(pathVToLCA[i]);
        }
        
        return cycle;
    }
    
    /**
     * @brief Verifies if a cycle actually exists in the solution
     */
    bool verify_cycle_exists(const std::vector<int>& cycle, const std::vector<double>& x_sol)
    {
        if (cycle.size() < 3) return false;  // Need at least 3 nodes for a cycle
        
        // Check if consecutive nodes in the cycle are connected by active edges
        for (size_t i = 0; i < cycle.size(); ++i) {
            int u = cycle[i];
            int v = cycle[(i + 1) % cycle.size()];  // Next node in cycle
            
            if (!instance_.has_edge(u, v)) {
                std::cout << "Cycle verification: No edge between " << u << " and " << v << std::endl;
                return false;
            }
            
            int edge_idx = instance_.get_edge_index(u, v);
            if (edge_idx < 0 || x_sol[edge_idx] <= 0.5) {
                std::cout << "Cycle verification: Edge " << u << "-" << v << " not active (value: " << x_sol[edge_idx] << ")" << std::endl;
                return false;
            }
        }
        
        return true;
    }
    
    /**
     * @brief Adds cycle constraint: sum of edges along the ordered cycle <= |cycle| - 1
     * CRITICAL FIX: Only sum edges along the ordered cycle, not all pairs
     */
    void add_cycle_constraint(const std::vector<int>& cycle)
    {
        GRBLinExpr cut_expr = 0;
        
        // CRITICAL FIX: Only add edges along the ordered cycle
        for (size_t i = 0; i < cycle.size(); ++i) {
            int u = cycle[i];
            int v = cycle[(i + 1) % cycle.size()];  // Next node in cycle
            int edge_idx = instance_.get_edge_index(u, v);
            if (edge_idx >= 0) {
                cut_expr += x_vars_[edge_idx];
            }
        }
        
        // Add constraint: sum of edges along cycle <= |cycle| - 1
        if (cut_expr.size() > 0) {
            std::cout << "Adding cycle constraint: " << cut_expr.size() << " edges along cycle, cycle size " << cycle.size() << std::endl;
            addLazy(cut_expr <= static_cast<double>(cycle.size()) - 1.0);
            lazy_constraints_count_++;
        } else {
            std::cout << "WARNING: Cycle constraint has no edges!" << std::endl;
        }
    }
    
    /**
     * @brief Adds connectivity constraint: sum of edges crossing component boundary >= 1
     * CRITICAL FIX: Use boundary cut ∑_{i∈S,j∉S} x_{ij} ≥ 1
     */
    void add_connectivity_constraint(const std::vector<int>& component)
    {
        GRBLinExpr cut_expr;
        
        // Sum edges crossing the component boundary
        for (int u : component) {
            for (int v = 0; v < instance_.num_nodes; ++v) {
                // Check if v is outside the component
                bool v_in_component = false;
                for (int comp_node : component) {
                    if (comp_node == v) {
                        v_in_component = true;
                        break;
                    }
                }
                
                if (!v_in_component && instance_.has_edge(u, v)) {
                    int edge_idx = instance_.get_edge_index(u, v);
                    if (edge_idx >= 0) {
                        cut_expr += x_vars_[edge_idx];
                    }
                }
            }
        }
        
        // Add constraint: sum of boundary edges >= 1
        if (cut_expr.size() > 0) {
            addLazy(cut_expr >= 1.0);
            lazy_constraints_count_++;
        }
    }
    
    void add_fractional_cuts()
    {
        try {
            // Get fractional solution from node relaxation
            std::vector<double> x_sol(instance_.num_edges);
            for (int e = 0; e < instance_.num_edges; ++e) {
                x_sol[e] = getNodeRel(x_vars_[e]);
            }
            
            // CRITICAL FIX: Use max-flow min-cut separation for fractional components
            // This replicates the original SECSep behavior with double precision
            bool cuts_added = false;
            
            // Try all pairs (s,t) for min-cut separation
            for (int s = 0; s < instance_.num_nodes; ++s) {
                for (int t = s + 1; t < instance_.num_nodes; ++t) {
                    // Build residual network with fractional capacities
                    MaxFlowMinCut network(instance_.num_nodes);
                    
                    for (int e = 0; e < instance_.num_edges; ++e) {
                        const Edge& edge = instance_.edges[e];
                        // CRITICAL FIX: Use fractional values directly, no truncation
                        double cap = x_sol[e];
                        if (cap > 1e-9) {  // Only add edges with meaningful capacity
                            network.add_edge(edge.source, edge.destination, cap);
                            network.add_edge(edge.destination, edge.source, cap);  // Undirected
                        }
                    }
                    
                    // Find max-flow from s to t
                    double max_flow = network.max_flow(s, t);
                    
                    // If max-flow < 1.0, we have a violated cut
                    if (max_flow < 1.0 - 1e-9) {
                        std::vector<int> cut = network.get_min_cut(s);
                        
                        // Ensure cut is non-trivial
                        if (cut.size() > 1 && cut.size() < static_cast<size_t>(instance_.num_nodes)) {
                            std::cout << "Fractional cut: Found violated cut of size " << cut.size() 
                                      << " with max-flow " << max_flow << std::endl;
                            add_connectivity_constraint(cut);
                            cuts_added = true;
                        }
                    }
                }
            }
            
            if (!cuts_added) {
                std::cout << "Fractional cuts: No violated cuts found" << std::endl;
            }
            
        } catch (GRBException& e) {
            // If getNodeRel fails, skip fractional cuts for this node
            std::cerr << "Fractional cuts error: " << e.getMessage() << std::endl;
        }
    }
    
    void dfs_component_fractional(int node, std::vector<bool>& visited, std::vector<int>& component, 
                                 const std::vector<double>& x_sol)
    {
        visited[node] = true;
        component.push_back(node);
        
        for (int j = 0; j < instance_.num_nodes; ++j) {
            if (!visited[j] && instance_.has_edge(node, j)) {
                int edge_idx = instance_.get_edge_index(node, j);
                if (edge_idx >= 0 && x_sol[edge_idx] > 0.1) {  // Lower threshold for fractional
                    dfs_component_fractional(j, visited, component, x_sol);
                }
            }
        }
    }
};

//=============================================================================
// PATH-BASED FORMULATION SOLVER (STANDARDIZED - using FormulationSolver base)
//=============================================================================

/**
 * @brief Path-based formulation solver for OCST problem
 */
    class PathBasedSolver : public ocst::common::FormulationSolver
    {
    private:
        // Decision variables (formulation-specific)
        std::vector<GRBVar> x_vars_;     // Edge selection variables
        std::vector<std::vector<GRBVar>> y_vars_;  // Flow variables for each requirement
        
        // SEC Callback for subtour elimination
        std::unique_ptr<SECCallback> sec_callback_;

        // Warm start telemetry (Phase 2.0)
        std::vector<std::string> warm_starts_tried_;
        std::string warm_start_used_ = "none";
        std::vector<ocst::common::WarmStartTree> warm_start_data_;
        
    public:
        explicit PathBasedSolver(const OCSTInstance& instance) 
            : FormulationSolver(instance, "path_based", "2.0.0"),
              sec_callback_(nullptr)
    {
        // Configuration will be done in configure() override
    }

protected:
    /**
     * @brief Configure Gurobi environment (override)
     * 
     * Path-based formulation requires:
     * - Lazy constraints enabled for SEC callback
     * - Presolve disabled for better cut separation
     * - PreCrush enabled for lazy constraint compatibility
     */
    void configure(const ocst::common::SolverConfig& config) override {
        // Call base class configuration first
        FormulationSolver::configure(config);
        
        // Path-based specific configuration
        model_->set(GRB_IntParam_LazyConstraints, 1);  // Enable lazy constraints (SEC)
        model_->set(GRB_IntParam_PreCrush, 1);  // Ensure lazy constraints work with presolve
        
        // Override base class defaults for better cut separation
        model_->set(GRB_IntParam_Presolve, 0);  // Disable presolve
        model_->set(GRB_IntParam_Cuts, 0);      // Disable default cuts
    }
    
    /**
     * @brief Build complete optimization model (SIMPLIFIED - fusion of 3 old hooks)
     * 
     * Creates variables, adds constraints, sets objective, and configures callback.
     * This is the main "build" step in the simplified interface.
     */
    void build_model(const ocst::common::SolverConfig& config) override 
    {
        // ===================================================================
        // STEP 1: Create decision variables
        // ===================================================================
        
        // Create x variables (edge selection - binary)
        x_vars_.resize(instance_.num_edges);
        for (int e = 0; e < instance_.num_edges; ++e) {
            const Edge& edge = instance_.edges[e];
            std::string var_name = "x_" + std::to_string(edge.source) + "_" + std::to_string(edge.destination);
            x_vars_[e] = model_->addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name);
        }
        
        // Create y variables (flow variables - binary for unit flow)
        y_vars_.resize(instance_.requirements.size());
        for (int r = 0; r < static_cast<int>(instance_.requirements.size()); ++r) {
            y_vars_[r].resize(instance_.num_edges * 2);  // Each undirected edge becomes 2 directed arcs
            
            for (int e = 0; e < instance_.num_edges; ++e) {
                const Edge& edge = instance_.edges[e];
                
                // Forward direction (i -> j)
                std::string var_name_fwd = "y_" + std::to_string(r) + "_" + 
                                          std::to_string(edge.source) + "_" + std::to_string(edge.destination);
                y_vars_[r][2*e] = model_->addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name_fwd);
                
                // Backward direction (j -> i)
                std::string var_name_bwd = "y_" + std::to_string(r) + "_" + 
                                          std::to_string(edge.destination) + "_" + std::to_string(edge.source);
                y_vars_[r][2*e + 1] = model_->addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name_bwd);
            }
        }
        
        // ===================================================================
        // STEP 2: Add constraints
        // ===================================================================
        
        // Check graph connectivity
        if (!is_graph_connected()) {
            std::cout << "[" << formulation_name_ << "] Warning: Graph appears to be disconnected" << std::endl;
        }
        
        add_structural_constraints();
        add_flow_constraints();
        add_coupling_constraints();
        set_bounds();
        
        // ===================================================================
        // STEP 3: Set objective function
        // ===================================================================
        
        GRBLinExpr objective = 0;
        int included_requirements = 0;
        int skipped_requirements = 0;
        
    if (config.verbose) {
            std::cout << "[" << formulation_name_ << "] Setting objective function:" << std::endl;
        }
        
        for (int r = 0; r < static_cast<int>(instance_.requirements.size()); ++r) {
            const Requirement& req = instance_.requirements[r];

            // Skip artificial requirements (weight = 0)
            if (req.weight <= 0.0) {
                if (config.verbose) {
                    std::cout << "  SKIPPED req[" << r << "]: (" << req.origin << ", " << req.destination
                             << ") weight=" << req.weight << std::endl;
                }
                skipped_requirements++;
                continue;
            }

            if (config.verbose) {
                std::cout << "  INCLUDED req[" << r << "]: (" << req.origin << ", " << req.destination
                         << ") weight=" << req.weight << std::endl;
            }
            included_requirements++;

            for (int e = 0; e < instance_.num_edges; ++e) {
                const Edge& edge = instance_.edges[e];

                // Add cost for both directions of flow
                objective += req.weight * edge.cost * y_vars_[r][2*e];     // Forward direction
                objective += req.weight * edge.cost * y_vars_[r][2*e + 1]; // Backward direction
            }
        }

        if (config.verbose) {
            std::cout << "Objective function summary:" << std::endl;
            std::cout << "  Requirements included: " << included_requirements << std::endl;
            std::cout << "  Requirements skipped: " << skipped_requirements << std::endl;
            std::cout << "  Total requirements: " << instance_.requirements.size() << std::endl;
        }
        
        model_->setObjective(objective, GRB_MINIMIZE);
        
        // ===================================================================
        // STEP 4: Configure SEC callback for subtour elimination
        // ===================================================================
        
        // Set up lazy constraint callback (must be done before optimize())
        sec_callback_ = std::make_unique<SECCallback>(instance_, x_vars_, lazy_constraints_added_);
        model_->setCallback(sec_callback_.get());
        
        // ===================================================================
        // STEP 5: Warm-start (optional)
        // ===================================================================
        
        if (config.enable_warm_start) {
            if (config.verbose) {
                std::cout << "[" << formulation_name_ << "] Loading warm starts..." << std::endl;
            }
            apply_warm_starts(config);
        }
    }  // End of build_model()

private:
    /**
     * @brief Adds structural constraints: sum of edges = n-1 (spanning tree)
     * CRITICAL FIX: Add symmetry constraints to ensure bidirectional edge consistency
     */
    void add_structural_constraints() 
    {
        GRBLinExpr tree_constraint = 0;
        for (int e = 0; e < instance_.num_edges; ++e) {
            tree_constraint += x_vars_[e];
        }
        model_->addConstr(tree_constraint == instance_.num_nodes - 1, "tree_constraint");
        
        // TEMPORARILY DISABLED: Symmetry constraints causing infeasibility
        // TODO: Debug and fix symmetry constraints
        /*
        for (int e = 0; e < instance_.num_edges; ++e) {
            for (int r = 0; r < static_cast<int>(instance_.requirements.size()); ++r) {
                const Requirement& req = instance_.requirements[r];
                
                if (req.weight > 0.0) {
                    std::string symmetry_name = "symmetry_" + std::to_string(r) + "_" + std::to_string(e);
                    GRBLinExpr total_flow = y_vars_[r][2*e] + y_vars_[r][2*e + 1];
                    model_->addConstr(total_flow <= req.weight * x_vars_[e], symmetry_name);
                }
            }
        }
        */
    }
    
    /**
     * @brief Adds flow constraints for each requirement (path connectivity)
     */
    void add_flow_constraints() 
    {
        for (int r = 0; r < static_cast<int>(instance_.requirements.size()); ++r) {
            const Requirement& req = instance_.requirements[r];
            
            for (int node = 0; node < instance_.num_nodes; ++node) {
                GRBLinExpr flow_balance = 0;
                
                // Sum of incoming flows - Sum of outgoing flows
                for (int e = 0; e < instance_.num_edges; ++e) {
                    const Edge& edge = instance_.edges[e];
                    
                    if (edge.source == node) {
                        // Outgoing flow (node -> other)
                        flow_balance -= y_vars_[r][2*e];
                        // Incoming flow (other -> node, reverse direction)
                        flow_balance += y_vars_[r][2*e + 1];
                    }
                    if (edge.destination == node) {
                        // Outgoing flow (node -> other, reverse direction)
                        flow_balance -= y_vars_[r][2*e + 1];
                        // Incoming flow (other -> node)
                        flow_balance += y_vars_[r][2*e];
                    }
                }
                
                // Set balance constraint based on node type
                // TEMPORARILY REVERTED: Use unit flow for consistency with warm-start
                double rhs = 0.0;
                if (node == req.origin) {
                    rhs = -1.0;  // Source: outflow > inflow by 1
                } else if (node == req.destination) {
                    rhs = 1.0;   // Sink: inflow > outflow by 1  
                }
                // For intermediate nodes: inflow = outflow (rhs = 0)
                
                std::string constraint_name = "flow_" + std::to_string(r) + "_" + std::to_string(node);
                model_->addConstr(flow_balance == rhs, constraint_name);
            }
        }
    }
    
    /**
     * @brief Adds coupling constraints: flow can only use selected edges
     * CRITICAL FIX: Use weight-based coupling y ≤ w_r * x instead of y ≤ x
     */
    void add_coupling_constraints() 
    {
        for (int e = 0; e < instance_.num_edges; ++e) {
            const Edge& edge = instance_.edges[e];
            
            for (int r = 0; r < static_cast<int>(instance_.requirements.size()); ++r) {
                // Forward flow constraint: y_r_ij ≤ x_ij (TEMPORARILY REVERTED)
                // Note: req variable removed to avoid unused variable warning
                std::string constraint_name_fwd = "coupling_" + std::to_string(r) + "_" + 
                                                 std::to_string(edge.source) + "_" + std::to_string(edge.destination);
                model_->addConstr(y_vars_[r][2*e] <= x_vars_[e], constraint_name_fwd);
                
                // Backward flow constraint: y_r_ji ≤ x_ij (TEMPORARILY REVERTED)
                std::string constraint_name_bwd = "coupling_" + std::to_string(r) + "_" + 
                                                 std::to_string(edge.destination) + "_" + std::to_string(edge.source);
                model_->addConstr(y_vars_[r][2*e + 1] <= x_vars_[e], constraint_name_bwd);
            }
        }
    }
    
    /**
     * @brief Apply warm starts from precomputed trees (data/input/<idea>/<instance>.txt)
     */
    void apply_warm_starts(const ocst::common::SolverConfig& config)
    {
        warm_starts_tried_.clear();
        warm_start_data_.clear();
        warm_start_used_ = "none";
        const int num_starts = static_cast<int>(config.warm_start_ideas.size());
        if (num_starts <= 0) {
            return;
        }

        // Reserve slots for all MIP starts before populating them.
        model_->set(GRB_IntAttr_NumStart, num_starts);
        model_->update();

        for (size_t idx = 0; idx < config.warm_start_ideas.size(); ++idx) {
            const auto& idea = config.warm_start_ideas[idx];
            auto start_ts = std::chrono::steady_clock::now();
            if (config.logger) {
                ocst::common::WarmStartEvent evt{idea, "start", 0.0, std::chrono::system_clock::now()};
                config.logger->log_warm_start_event(evt);
            }

            ocst::common::WarmStartTree data;
            try {
                data = ocst::common::load_warm_start_tree(instance_, idea, config.instance_name);
            } catch (...) {
                if (config.logger) {
                    auto elapsed_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_ts).count();
                    ocst::common::WarmStartEvent evt{idea, "failure", elapsed_ms, std::chrono::system_clock::now()};
                    config.logger->log_warm_start_event(evt);
                }
                throw;
            }

            warm_starts_tried_.push_back(idea);
            warm_start_data_.push_back(data);

            // Select start slot and reset all variables for this start
            model_->set(GRB_IntParam_StartNumber, static_cast<int>(idx));
            for (auto& var : x_vars_) {
                var.set(GRB_DoubleAttr_Start, GRB_UNDEFINED);
            }
            for (auto& y_per_req : y_vars_) {
                for (auto& var : y_per_req) {
                    var.set(GRB_DoubleAttr_Start, GRB_UNDEFINED);
                }
            }

            // Initialize all x to 0 for this start
            for (auto& var : x_vars_) {
                var.set(GRB_DoubleAttr_Start, 0.0);
            }
            // Initialize all y to 0 for this start
            for (auto& y_per_req : y_vars_) {
                for (auto& var : y_per_req) {
                    var.set(GRB_DoubleAttr_Start, 0.0);
                }
            }
            // Set active edges to 1
            for (int e_idx : data.edge_indices) {
                if (e_idx >= 0 && e_idx < static_cast<int>(x_vars_.size())) {
                    x_vars_[e_idx].set(GRB_DoubleAttr_Start, 1.0);
                }
            }

            // Set flows along the unique tree path for each requirement
            std::vector<std::vector<int>> adj(instance_.num_nodes);
            for (const auto& e : data.tree_edges) {
                adj[e.first].push_back(e.second);
                adj[e.second].push_back(e.first);
            }

            for (int r = 0; r < static_cast<int>(instance_.requirements.size()); ++r) {
                const Requirement& req = instance_.requirements[r];
                int s = req.origin;
                int t = req.destination;

                // BFS to recover path in the tree
                std::vector<int> parent(instance_.num_nodes, -1);
                std::queue<int> q;
                q.push(s);
                parent[s] = s;
                while (!q.empty() && parent[t] == -1) {
                    int u = q.front();
                    q.pop();
                    for (int v : adj[u]) {
                        if (parent[v] == -1) {
                            parent[v] = u;
                            q.push(v);
                        }
                    }
                }

                if (parent[t] == -1) {
                    throw std::runtime_error("No path in warm start tree for requirement " + std::to_string(r));
                }

                // Reconstruct path t -> s
                int node = t;
                while (node != s) {
                    int p = parent[node];
                    int edge_idx = instance_.get_edge_index(node, p);
                    if (edge_idx < 0) {
                        throw std::runtime_error("Warm start edge missing in instance graph between " +
                                                 std::to_string(node) + " and " + std::to_string(p));
                    }

                    const Edge& edge = instance_.edges[edge_idx];
                    if (edge.source == p && edge.destination == node) {
                        y_vars_[r][2 * edge_idx].set(GRB_DoubleAttr_Start, 1.0);
                    } else if (edge.source == node && edge.destination == p) {
                        y_vars_[r][2 * edge_idx + 1].set(GRB_DoubleAttr_Start, 1.0);
                    } else {
                        // Edge orientation mismatched; set both directions as a fallback
                        y_vars_[r][2 * edge_idx].set(GRB_DoubleAttr_Start, 1.0);
                        y_vars_[r][2 * edge_idx + 1].set(GRB_DoubleAttr_Start, 1.0);
                    }

                    node = p;
                }
            }

            if (config.logger) {
                auto elapsed_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_ts).count();
                ocst::common::WarmStartEvent evt{idea, "success", elapsed_ms, std::chrono::system_clock::now()};
                config.logger->log_warm_start_event(evt);
            }
        }

        if (warm_starts_tried_.size() == 1) {
            warm_start_used_ = warm_starts_tried_.front();
        } else if (!warm_starts_tried_.empty()) {
            warm_start_used_ = "multiple";
        }
    }
    
    /**
     * @brief Finds MST using Kruskal's algorithm
     */
    std::vector<int> find_mst()
    {
        std::vector<int> mst_edges;
        
        // Sort edges by cost
        std::vector<std::pair<double, int>> sorted_edges;
        for (int e = 0; e < instance_.num_edges; ++e) {
            sorted_edges.push_back({instance_.edges[e].cost, e});
        }
        std::sort(sorted_edges.begin(), sorted_edges.end());
        
        // Union-Find data structure
        std::vector<int> parent(instance_.num_nodes);
        for (int i = 0; i < instance_.num_nodes; ++i) {
            parent[i] = i;
        }
        
        auto find = [&](int x) {
            while (parent[x] != x) {
                parent[x] = parent[parent[x]];  // Path compression
                x = parent[x];
            }
            return x;
        };
        
        auto unite = [&](int x, int y) {
            int px = find(x);
            int py = find(y);
            if (px != py) {
                parent[px] = py;
                return true;
            }
            return false;
        };
        
        // Kruskal's algorithm
        for (const auto& edge_pair : sorted_edges) {
            int e = edge_pair.second;
            const Edge& edge = instance_.edges[e];
            
            if (unite(edge.source, edge.destination)) {
                mst_edges.push_back(e);
                if (static_cast<int>(mst_edges.size()) == instance_.num_nodes - 1) {
                    break;
                }
            }
        }
        
        return mst_edges;
    }
    
    /**
     * @brief Finds path between two nodes in MST
     */
    std::vector<int> find_path_in_mst(int source, int dest, const std::vector<int>& mst_edges)
    {
        // Build MST adjacency list
        std::vector<std::vector<int>> mst_adj(instance_.num_nodes);
        for (int e : mst_edges) {
            const Edge& edge = instance_.edges[e];
            mst_adj[edge.source].push_back(edge.destination);
            mst_adj[edge.destination].push_back(edge.source);
        }
        
        // BFS to find path
        std::vector<int> parent(instance_.num_nodes, -1);
        std::queue<int> q;
        q.push(source);
        parent[source] = source;
        
        while (!q.empty()) {
            int u = q.front();
            q.pop();
            
            if (u == dest) break;
            
            for (int v : mst_adj[u]) {
                if (parent[v] == -1) {
                    parent[v] = u;
                    q.push(v);
                }
            }
        }
        
        // Reconstruct path
        std::vector<int> path;
        int current = dest;
        while (current != source) {
            path.push_back(current);
            current = parent[current];
        }
        path.push_back(source);
        std::reverse(path.begin(), path.end());
        
        return path;
    }
    
    /**
     * @brief Sets upper and lower bounds for better convergence
     */
    void set_bounds()
    {
        try {
            // Calculate MST cost as upper bound
            std::vector<int> mst_edges = find_mst();
            double mst_cost = 0.0;
            for (int e : mst_edges) {
                mst_cost += instance_.edges[e].cost;
            }
            
            // Calculate total communication cost using MST as upper bound
            double upper_bound = 0.0;
            for (const auto& req : instance_.requirements) {
                if (req.weight > 0.0) {
                    // Find path length in MST
                    std::vector<int> path = find_path_in_mst(req.origin, req.destination, mst_edges);
                    double path_cost = 0.0;
                    
                    for (size_t i = 0; i < path.size() - 1; ++i) {
                        int u = path[i];
                        int v = path[i + 1];
                        int edge_idx = instance_.get_edge_index(u, v);
                        if (edge_idx >= 0) {
                            path_cost += instance_.edges[edge_idx].cost;
                        }
                    }
                    
                    upper_bound += req.weight * path_cost;
                }
            }
            
            // Set upper bound (temporarily disabled for debugging)
            // model_.set(GRB_DoubleParam_Cutoff, upper_bound * 1.01);
            
            // Calculate lower bound using shortest paths
            double lower_bound = 0.0;
            for (const auto& req : instance_.requirements) {
                if (req.weight > 0.0) {
                    // Find shortest path cost using Floyd-Warshall approximation
                    double shortest_cost = find_shortest_path_cost(req.origin, req.destination);
                    lower_bound += req.weight * shortest_cost;
                }
            }
            
            // Set lower bound (this is informational, Gurobi will calculate its own bounds)
            std::cout << "Bounds set - Lower: " << lower_bound << ", Upper: " << upper_bound << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to set bounds: " << e.what() << std::endl;
        }
    }
    
    /**
     * @brief Finds shortest path cost between two nodes using Dijkstra-like approach
     */
    double find_shortest_path_cost(int source, int dest)
    {
        std::vector<double> dist(instance_.num_nodes, std::numeric_limits<double>::infinity());
        std::vector<bool> visited(instance_.num_nodes, false);
        
        dist[source] = 0.0;
        
        for (int i = 0; i < instance_.num_nodes; ++i) {
            int u = -1;
            double min_dist = std::numeric_limits<double>::infinity();
            
            for (int j = 0; j < instance_.num_nodes; ++j) {
                if (!visited[j] && dist[j] < min_dist) {
                    min_dist = dist[j];
                    u = j;
                }
            }
            
            if (u == -1 || u == dest) break;
            
            visited[u] = true;
            
            for (int v = 0; v < instance_.num_nodes; ++v) {
                if (instance_.has_edge(u, v)) {
                    int edge_idx = instance_.get_edge_index(u, v);
                    double edge_cost = instance_.edges[edge_idx].cost;
                    
                    if (dist[u] + edge_cost < dist[v]) {
                        dist[v] = dist[u] + edge_cost;
                    }
                }
            }
        }
        
        return dist[dest];
    }
    
    /**
     * @brief Extracts solution information from the optimized model
     */
    /**
     * @brief Lifecycle Hook 7: Extract solution and populate ResultPayload (override)
     */
    ocst::common::ResultPayload collect_results() override {
        ocst::common::ResultPayload payload;
        
        // Optimization status
        int gurobi_status = model_->get(GRB_IntAttr_Status);
        payload.optimization_status_code = convert_gurobi_status(gurobi_status);
        
        // Solution data
        bool has_solution = (gurobi_status == GRB_OPTIMAL || gurobi_status == GRB_TIME_LIMIT);
        payload.has_solution = has_solution;
        payload.has_bound = true;  // Gurobi siempre entrega cota dual cuando resuelve
        
        if (has_solution) {
            // Objective value
            payload.objective = model_->get(GRB_DoubleAttr_ObjVal);
            payload.primal_bound = payload.objective;
            
            // Extract selected edges and build tree structure
            for (int e = 0; e < instance_.num_edges; ++e) {
                if (x_vars_[e].get(GRB_DoubleAttr_X) > 0.5) {
                    const Edge& edge = instance_.edges[e];
                    payload.solution.tree_edges.push_back({edge.source, edge.destination});
                }
            }
            
            // Tree metrics
            payload.solution.tree_cost = payload.objective;
            payload.solution.is_spanning_tree = (payload.solution.tree_edges.size() == static_cast<size_t>(instance_.num_nodes - 1));
            payload.solution.is_connected = payload.solution.is_spanning_tree;  // Spanning tree implies connected
            
            // Dual bound
            try {
                payload.dual_bound = model_->get(GRB_DoubleAttr_ObjBound);
                payload.gap = payload.primal_bound - payload.dual_bound;
                if (std::abs(payload.primal_bound) > 1e-9) {
                    payload.gap_percent = 100.0 * (payload.primal_bound - payload.dual_bound) / payload.primal_bound;
                }
            } catch (GRBException&) {
                // Bound not available
                payload.dual_bound = payload.objective;
                payload.gap_percent = 0.0;
                payload.gap = 0.0;
            }
        } else {
            payload.objective = 0.0;
            payload.primal_bound = 0.0;
            try {
                payload.dual_bound = model_->get(GRB_DoubleAttr_ObjBound);
                payload.gap = 0.0;
                payload.gap_percent = 0.0;
            } catch (GRBException&) {
                payload.has_bound = false;
            }
        }

        // Warm start metadata
        if (!warm_starts_tried_.empty()) {
            std::ostringstream tried_ss;
            for (size_t i = 0; i < warm_starts_tried_.size(); ++i) {
                if (i > 0) tried_ss << ",";
                tried_ss << warm_starts_tried_[i];
            }
            payload.solver_metadata["warm_starts_tried"] = tried_ss.str();
            payload.solver_metadata["warm_start_used"] = warm_start_used_;
            if (!warm_start_data_.empty()) {
                payload.warm_start_path = warm_start_data_.front().path;
            }
        }

        // Runtime stats (wall clock ya se setea en solve; aquí añadimos CPU si aplica)
        try {
            payload.runtime_stats.cpu_seconds = model_->get(GRB_DoubleAttr_Runtime);
        } catch (...) {
            // Silencio
        }
        
        return payload;
    }
};

//=============================================================================
// MAIN SOLVING FUNCTION
//=============================================================================

/**
 * @brief Main function that solves an OCST instance using path-based formulation (STANDARDIZED)
 * @param input_file Path to input instance file (JSON format)
 * @param config_file Path to JSON config file (optional)
 * @param enable_logging Whether to enable structured logging (default: false)
 * @param output_dir Directory where to save results (default: experiments/results)
 * @param seed Random seed for reproducibility (default: 42)
 * @return ResultPayload with complete solution (STANDARDIZED - no legacy SolutionResult)
 */
ResultPayload solve_path_based_instance(const std::string& input_file,
                                       const std::string& config_file = "",
                                       bool enable_logging = false,
                                       const std::string& output_dir = "experiments/results",
                                       int seed = 42) 
{
    try {
        // Parse instance using unified loader (supports both legacy .ocstpin and JSON formats)
        std::cout << "Parsing instance: " << input_file << std::endl;
        OCSTInstance instance = ocst::common::load_instance(input_file);
        
        std::cout << "Instance stats: " << instance.num_nodes << " nodes, " 
                  << instance.num_edges << " edges, " << instance.requirements.size() 
                  << " requirements" << std::endl;
        
        // Compute instance basename (for warm starts and outputs)
        std::string instance_basename = input_file;
        size_t last_slash = instance_basename.find_last_of("/");
        if (last_slash != std::string::npos) {
            instance_basename = instance_basename.substr(last_slash + 1);
        }
        size_t last_dot = instance_basename.find_last_of(".");
        if (last_dot != std::string::npos) {
            instance_basename = instance_basename.substr(0, last_dot);
        }

        // Solve using path-based formulation with new interface
        PathBasedSolver solver(instance);

        // Configure solver
        SolverConfig config;

        // Load config from file if provided
        if (!config_file.empty()) {
            bool config_loaded = config.common_config.load_from_file(config_file);
            if (!config_loaded) {
                std::cerr << "Warning: Failed to load config file: " << config_file << std::endl;
            }
        }

        // Override config with loaded values
        config.time_limit_seconds = config.common_config.get_time_limit();
        config.mip_gap = config.common_config.get_mip_gap();
        config.threads = config.common_config.get_threads();
        config.verbose = config.common_config.get_output_flag();
        config.heuristics_level = 0.5;  // Keep default for now
        config.warm_start_ideas = config.common_config.get_warm_starts();
        config.enable_warm_start = !config.warm_start_ideas.empty();

        // Set seed for reproducibility
        config.seed = seed;
        config.instance_name = instance_basename;

        // Create logger if enabled
        if (enable_logging) {
            std::string log_base = "experiments/logs/solver_" + std::to_string(time(nullptr));
            config.logger = std::make_unique<ocst::common::StructuredLogger>(log_base);
        }
        
        // Solve and get ResultPayload (STANDARDIZED - no legacy conversion)
        ResultPayload payload = solver.solve(config);
        
        // Print results (using ResultPayload directly)
        std::cout << "\n=== SOLUTION RESULTS ===" << std::endl;
        
        // Status reporting
        std::string status_str = optimization_status_to_string(payload.optimization_status_code);
        std::cout << "Status: " << status_str << std::endl;
        std::cout << "Objective value: " << std::fixed << std::setprecision(0) << payload.objective << std::endl;
        std::cout << "Runtime: " << payload.runtime_stats.wall_clock_seconds << " seconds" << std::endl;
        std::cout << "Nodes explored: " << payload.runtime_stats.solver_nodes << std::endl;
        std::cout << "MIP gap: " << payload.gap_percent << "%" << std::endl;
        
        // Extract metrics from solver_metadata
        auto it_lazy = payload.solver_metadata.find("lazy_constraints");
        int lazy_added = (it_lazy != payload.solver_metadata.end()) ? std::stoi(it_lazy->second) : 0;
        auto it_cuts = payload.solver_metadata.find("cutting_planes");
        int cuts_added = (it_cuts != payload.solver_metadata.end()) ? std::stoi(it_cuts->second) : 0;
        auto it_ws_used = payload.solver_metadata.find("warm_start_used");
        std::string warm_used = (it_ws_used != payload.solver_metadata.end()) ? it_ws_used->second : "none";
        
        std::cout << "Lazy constraints added: " << lazy_added << std::endl;
        std::cout << "Cutting planes added: " << cuts_added << std::endl;
        std::cout << "Selected edges: " << payload.solution.tree_edges.size() << std::endl;
        std::cout << "Warm start used: " << warm_used << std::endl;
        
        // Generate output filename based on input file
        // Write JSON solution using ResultPayload (modern format)
        std::filesystem::path json_solution_file = std::filesystem::path(output_dir) / (instance_basename + ".results.json");
        std::filesystem::create_directories(json_solution_file.parent_path());
        
        // Populate instance metadata in payload
        payload.instance_name = instance_basename;
        payload.instance_tags = {};  // TODO: Load from JSON instance
        
        // Generate run_uuid (required field)
        payload.run_uuid = sole::uuid4().str();
        
        // Write to file using ResultSerializer
        try {
            ResultSerializer::write_to_file(payload, json_solution_file.string(), true);
            std::cout << "JSON solution written to: " << json_solution_file << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Warning: Could not write JSON solution: " << e.what() << std::endl;
        }
        
        return payload;  // STANDARDIZED: return ResultPayload directly
        
    } catch (const std::exception& e) {
        std::cerr << "Error parsing instance: " << e.what() << std::endl;
        ResultPayload error_payload;
        error_payload.optimization_status_code = ocst::common::OptimizationStatus::ERROR;
        error_payload.has_solution = false;
        return error_payload;
    }
}

//=============================================================================
// MAIN FUNCTION FOR TESTING
//=============================================================================

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " --instance <instance_name> [--seed <seed>] [--output-dir <dir>] [--config <config.json>] [--enable-logging] [--<param>=<value> ...]" << std::endl;
        std::cout << "Examples:" << std::endl;
        std::cout << "  " << argv[0] << " --instance ocstpin0" << std::endl;
        std::cout << "  " << argv[0] << " --instance ocstpin0 --seed 42 --config config.json --enable-logging" << std::endl;
        std::cout << "  " << argv[0] << " --instance ocstpin0 --time_limit=1800 --mip_gap=0.001" << std::endl;
        return 1;
    }

    std::string instance_name = "";
    int seed = 42;  // Default seed
    std::string output_dir = "experiments/results";
    std::string config_file = "";
    bool enable_logging = false;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--instance" && i + 1 < argc) {
            instance_name = argv[++i];
        } else if (arg == "--seed" && i + 1 < argc) {
            seed = std::stoi(argv[++i]);
        } else if (arg == "--output-dir" && i + 1 < argc) {
            output_dir = argv[++i];
        } else if (arg == "--config" && i + 1 < argc) {
            config_file = argv[++i];
        } else if (arg == "--enable-logging") {
            enable_logging = true;
        } else if (arg.find("--") == 0 && arg.find("=") != std::string::npos) {
            // Config parameter as --key=value
            // These will be handled by the config system
            continue;  // Just consume the argument
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            return 1;
        }
    }

    if (instance_name.empty()) {
        std::cerr << "Error: --instance parameter is required" << std::endl;
        return 1;
    }

    // Build full path to instance file
    std::string input_file = "data/input/" + instance_name + ".json";

    // STANDARDIZED: use ResultPayload instead of legacy SolutionResult
    ResultPayload result = solve_path_based_instance(input_file, config_file, enable_logging, output_dir, seed);

    return (result.optimization_status_code == ocst::common::OptimizationStatus::OPTIMAL) ? 0 : 1;
}
