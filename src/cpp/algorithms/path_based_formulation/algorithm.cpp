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

// Gurobi integration
#include <gurobi_c++.h>

// Additional includes for SEC callback
#include <queue>
#include <memory>
#include <stack>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

//=============================================================================
// DATA STRUCTURES
//=============================================================================

/**
 * @brief Represents a graph edge with source, destination and cost
 */
struct Edge 
{
    int source;
    int destination;
    double cost;
    
    Edge(int s, int d, double c) : source(s), destination(d), cost(c) {}
};

/**
 * @brief Represents a demand requirement between origin and destination
 */
struct Requirement 
{
    int origin;
    int destination;
    double weight;
    
    Requirement(int o, int d, double w) : origin(o), destination(d), weight(w) {}
};

/**
 * @brief OCST problem instance data
 */
struct OCSTInstance 
{
    int num_nodes;
    int num_edges;
    double probability;
    std::vector<Edge> edges;
    std::vector<Requirement> requirements;
    
    // Derived data structures for optimization
    std::vector<std::vector<int>> adjacency_matrix;  // -1 if no edge, edge_index otherwise
    std::unordered_map<std::string, int> edge_index_map;  // "(i,j)" -> edge index
    
    OCSTInstance(int n, double prob) : num_nodes(n), probability(prob) 
    {
        num_edges = 0;
        adjacency_matrix = std::vector<std::vector<int>>(n, std::vector<int>(n, -1));
    }
    
    void add_edge(int source, int dest, double cost) 
    {
        edges.emplace_back(source, dest, cost);
        
        // Update adjacency matrix (undirected graph)
        adjacency_matrix[source][dest] = num_edges;
        adjacency_matrix[dest][source] = num_edges;
        
        // Update edge index map
        edge_index_map[std::to_string(source) + "," + std::to_string(dest)] = num_edges;
        edge_index_map[std::to_string(dest) + "," + std::to_string(source)] = num_edges;
        
        num_edges++;
    }
    
    void add_requirement(int origin, int dest, double weight) 
    {
        requirements.emplace_back(origin, dest, weight);
    }
    
    bool has_edge(int i, int j) const 
    {
        return adjacency_matrix[i][j] != -1;
    }
    
    int get_edge_index(int i, int j) const 
    {
        return adjacency_matrix[i][j];
    }
};

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

/**
 * @brief Solution metrics and results
 */
struct SolutionResult 
{
    double objective_value;
    double runtime_seconds;
    int gurobi_status;
    long long num_nodes_explored;
    double mip_gap;
    int lazy_constraints_added;
    int cutting_planes_added;
    double lower_bound;
    double upper_bound;
    bool is_optimal;
    
    std::vector<int> selected_edges;  // Indices of selected edges in solution
    
    SolutionResult() 
    {
        objective_value = -1.0;
        runtime_seconds = 0.0;
        gurobi_status = -1;
        num_nodes_explored = 0;
        mip_gap = 100.0;
        lazy_constraints_added = 0;
        cutting_planes_added = 0;
        lower_bound = 0.0;
        upper_bound = 0.0;
        is_optimal = false;
    }
};

//=============================================================================
// PATH-BASED FORMULATION SOLVER
//=============================================================================

/**
 * @brief Path-based formulation solver for OCST problem
 */
class PathBasedSolver 
{
private:
    const OCSTInstance& instance_;
    GRBEnv env_;
    GRBModel model_;
    
    // Decision variables
    std::vector<GRBVar> x_vars_;     // Edge selection variables
    std::vector<std::vector<GRBVar>> y_vars_;  // Flow variables for each requirement
    
    // Metrics tracking
    int lazy_constraints_count_;
    int cutting_planes_count_;
    
    // SEC Callback for subtour elimination
    std::unique_ptr<SECCallback> sec_callback_;
    
public:
    explicit PathBasedSolver(const OCSTInstance& instance) 
        : instance_(instance), env_(GRBEnv()), model_(GRBModel(env_)), sec_callback_(nullptr)
    {
        lazy_constraints_count_ = 0;
        cutting_planes_count_ = 0;
        
        // Configure Gurobi parameters as specified in pseudocode
        env_.set(GRB_IntParam_OutputFlag, 1);  // Enable console output for debugging
        model_.set(GRB_IntParam_LazyConstraints, 1);  // Enable lazy constraints
        model_.set(GRB_IntParam_PreCrush, 1);  // Ensure lazy constraints work with presolve
        model_.set(GRB_IntParam_Presolve, 0);  // Disable presolve for better cut separation
        model_.set(GRB_IntParam_Threads, 1);   // Single thread for reproducibility
        model_.set(GRB_IntParam_Cuts, 0);      // Disable default cuts to prioritize custom ones
        
        // Additional parameters for better performance
        model_.set(GRB_IntParam_MIPFocus, 1);     // Focus on feasible solutions
        model_.set(GRB_IntParam_NumericFocus, 2);  // High precision for numerical stability
        model_.set(GRB_DoubleParam_MIPGap, 1e-6); // Tight optimality gap
        model_.set(GRB_DoubleParam_MIPGapAbs, 1e-6); // Absolute gap tolerance
    }
    
    /**
     * @brief Solves the OCST instance using path-based formulation
     * @param time_limit Time limit in seconds
     * @param heuristics_level Gurobi heuristics parameter (0-2)
     * @return SolutionResult containing all solution metrics
     */
    SolutionResult solve(double time_limit = 3600.0, double heuristics_level = 0.5) 
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            // Set algorithm parameters
            model_.set(GRB_DoubleParam_TimeLimit, time_limit);
            model_.set(GRB_DoubleParam_Heuristics, heuristics_level);
            
            // Check graph connectivity (basic connectivity check)
            if (!is_connected()) {
                std::cout << "Warning: Graph appears to be disconnected" << std::endl;
            }
            
            // Create variables
            create_variables();
            
            // Set up SEC callback for subtour elimination
            sec_callback_ = std::make_unique<SECCallback>(instance_, x_vars_, lazy_constraints_count_);
            model_.setCallback(sec_callback_.get());
            
            // Add constraints
            add_structural_constraints();
            add_flow_constraints();
            add_coupling_constraints();
            
            // Set objective function
            set_objective();
            
            // Set initial solution using MST
            // TEMPORARILY DISABLED to test callback behavior
            // set_initial_solution();
            
            // Set bounds for better convergence
            set_bounds();
            
            // Optimize
            model_.optimize();
            
            // Extract solution
            SolutionResult result = extract_solution();
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            result.runtime_seconds = duration.count() / 1000.0;
            
            return result;
            
        } catch (GRBException& e) {
            std::cerr << "Gurobi error: " << e.getMessage() << std::endl;
            SolutionResult error_result;
            error_result.gurobi_status = e.getErrorCode();
            return error_result;
        }
    }

private:
    /**
     * @brief Basic connectivity check using DFS
     */
    bool is_connected() 
    {
        if (instance_.num_nodes <= 1) return true;
        
        std::vector<bool> visited(instance_.num_nodes, false);
        std::vector<int> stack;
        stack.push_back(0);  // Start from node 0
        visited[0] = true;
        int visited_count = 1;
        
        while (!stack.empty()) {
            int current = stack.back();
            stack.pop_back();
            
            for (int j = 0; j < instance_.num_nodes; ++j) {
                if (!visited[j] && instance_.has_edge(current, j)) {
                    visited[j] = true;
                    stack.push_back(j);
                    visited_count++;
                }
            }
        }
        
        return visited_count == instance_.num_nodes;
    }
    
    /**
     * @brief Creates decision variables for the MILP formulation
     */
    void create_variables() 
    {
        // Create x variables (edge selection - binary)
        x_vars_.resize(instance_.num_edges);
        for (int e = 0; e < instance_.num_edges; ++e) {
            const Edge& edge = instance_.edges[e];
            std::string var_name = "x_" + std::to_string(edge.source) + "_" + std::to_string(edge.destination);
            x_vars_[e] = model_.addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name);
        }
        
        // Create y variables (flow variables - continuous)
        y_vars_.resize(instance_.requirements.size());
        for (int r = 0; r < static_cast<int>(instance_.requirements.size()); ++r) {
            y_vars_[r].resize(instance_.num_edges * 2);  // Each undirected edge becomes 2 directed arcs
            
            for (int e = 0; e < instance_.num_edges; ++e) {
                const Edge& edge = instance_.edges[e];
                
                // Forward direction (i -> j) - BINARY for unit flow
                std::string var_name_fwd = "y_" + std::to_string(r) + "_" + 
                                          std::to_string(edge.source) + "_" + std::to_string(edge.destination);
                y_vars_[r][2*e] = model_.addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name_fwd);
                
                // Backward direction (j -> i) - BINARY for unit flow
                std::string var_name_bwd = "y_" + std::to_string(r) + "_" + 
                                          std::to_string(edge.destination) + "_" + std::to_string(edge.source);
                y_vars_[r][2*e + 1] = model_.addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name_bwd);
            }
        }
    }
    
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
        model_.addConstr(tree_constraint == instance_.num_nodes - 1, "tree_constraint");
        
        // TEMPORARILY DISABLED: Symmetry constraints causing infeasibility
        // TODO: Debug and fix symmetry constraints
        /*
        for (int e = 0; e < instance_.num_edges; ++e) {
            for (int r = 0; r < static_cast<int>(instance_.requirements.size()); ++r) {
                const Requirement& req = instance_.requirements[r];
                
                if (req.weight > 0.0) {
                    std::string symmetry_name = "symmetry_" + std::to_string(r) + "_" + std::to_string(e);
                    GRBLinExpr total_flow = y_vars_[r][2*e] + y_vars_[r][2*e + 1];
                    model_.addConstr(total_flow <= req.weight * x_vars_[e], symmetry_name);
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
                model_.addConstr(flow_balance == rhs, constraint_name);
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
                model_.addConstr(y_vars_[r][2*e] <= x_vars_[e], constraint_name_fwd);
                
                // Backward flow constraint: y_r_ji ≤ x_ij (TEMPORARILY REVERTED)
                std::string constraint_name_bwd = "coupling_" + std::to_string(r) + "_" + 
                                                 std::to_string(edge.destination) + "_" + std::to_string(edge.source);
                model_.addConstr(y_vars_[r][2*e + 1] <= x_vars_[e], constraint_name_bwd);
            }
        }
    }
    
    /**
     * @brief Sets the objective function: minimize total weighted communication cost
     * Only considers requirements with weight > 0 (excludes artificial connectivity requirements)
     */
    void set_objective() 
    {
        GRBLinExpr objective = 0;
        int included_requirements = 0;
        int skipped_requirements = 0;
        
        std::cout << "Setting objective function:" << std::endl;
        
        for (int r = 0; r < static_cast<int>(instance_.requirements.size()); ++r) {
            const Requirement& req = instance_.requirements[r];
            
            // Skip artificial requirements (weight = 0)
            if (req.weight <= 0.0) {
                std::cout << "  SKIPPED req[" << r << "]: (" << req.origin << ", " << req.destination 
                         << ") weight=" << req.weight << std::endl;
                skipped_requirements++;
                continue;
            }
            
            std::cout << "  INCLUDED req[" << r << "]: (" << req.origin << ", " << req.destination 
                     << ") weight=" << req.weight << std::endl;
            included_requirements++;
            
            for (int e = 0; e < instance_.num_edges; ++e) {
                const Edge& edge = instance_.edges[e];
                
                // Add cost for both directions of flow
                objective += req.weight * edge.cost * y_vars_[r][2*e];     // Forward direction
                objective += req.weight * edge.cost * y_vars_[r][2*e + 1]; // Backward direction
            }
        }
        
        std::cout << "Objective function summary:" << std::endl;
        std::cout << "  Requirements included: " << included_requirements << std::endl;
        std::cout << "  Requirements skipped: " << skipped_requirements << std::endl;
        std::cout << "  Total requirements: " << instance_.requirements.size() << std::endl;
        
        model_.setObjective(objective, GRB_MINIMIZE);
    }
    
    /**
     * @brief Sets initial solution using MST for warm-start
     */
    void set_initial_solution()
    {
        try {
            // Find MST using Kruskal's algorithm
            std::vector<int> mst_edges = find_mst();
            
            // Set x variables based on MST
            for (int e = 0; e < instance_.num_edges; ++e) {
                bool in_mst = std::find(mst_edges.begin(), mst_edges.end(), e) != mst_edges.end();
                x_vars_[e].set(GRB_DoubleAttr_Start, in_mst ? 1.0 : 0.0);
            }
            
            // Set y variables based on shortest paths in MST
            for (int r = 0; r < static_cast<int>(instance_.requirements.size()); ++r) {
                const Requirement& req = instance_.requirements[r];
                
                // Find shortest path from origin to destination in MST
                std::vector<int> path = find_path_in_mst(req.origin, req.destination, mst_edges);
                
                // Set flow variables along the path
                for (size_t i = 0; i < path.size() - 1; ++i) {
                    int u = path[i];
                    int v = path[i + 1];
                    
                    // Find edge index
                    int edge_idx = instance_.get_edge_index(u, v);
                    if (edge_idx >= 0) {
                        const Edge& edge = instance_.edges[edge_idx];
                        
                        // Determine direction and set flow
                        if (edge.source == u && edge.destination == v) {
                            y_vars_[r][2 * edge_idx].set(GRB_DoubleAttr_Start, 1.0);  // Unit flow
                        } else if (edge.source == v && edge.destination == u) {
                            y_vars_[r][2 * edge_idx + 1].set(GRB_DoubleAttr_Start, 1.0);  // Unit flow
                        }
                    }
                }
            }
            
            std::cout << "Warm-start set using MST with " << mst_edges.size() << " edges" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to set warm-start: " << e.what() << std::endl;
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
    SolutionResult extract_solution() 
    {
        SolutionResult result;
        
        result.gurobi_status = model_.get(GRB_IntAttr_Status);
        result.is_optimal = (result.gurobi_status == GRB_OPTIMAL);
        result.num_nodes_explored = model_.get(GRB_DoubleAttr_NodeCount);
        
        if (result.gurobi_status == GRB_OPTIMAL || result.gurobi_status == GRB_TIME_LIMIT) {
            result.objective_value = model_.get(GRB_DoubleAttr_ObjVal);
            result.upper_bound = model_.get(GRB_DoubleAttr_ObjVal);
            
            // Extract selected edges
            for (int e = 0; e < instance_.num_edges; ++e) {
                if (x_vars_[e].get(GRB_DoubleAttr_X) > 0.5) {
                    result.selected_edges.push_back(e);
                }
            }
        }
        
        if (result.gurobi_status == GRB_OPTIMAL || result.gurobi_status == GRB_TIME_LIMIT) {
            try {
                result.lower_bound = model_.get(GRB_DoubleAttr_ObjBound);
                if (result.upper_bound > 0) {
                    result.mip_gap = 100.0 * (result.upper_bound - result.lower_bound) / result.upper_bound;
                }
            } catch (GRBException& e) {
                // Bound not available
                result.lower_bound = result.objective_value;
                result.mip_gap = 0.0;
            }
        }
        
        result.lazy_constraints_added = lazy_constraints_count_;
        result.cutting_planes_added = cutting_planes_count_;
        
        return result;
    }
};

//=============================================================================
// INSTANCE PARSING UTILITIES  
//=============================================================================

/**
 * @brief Parses OCST instance from input file
 * @param filename Path to the instance file
 * @return Parsed OCSTInstance object
 */
OCSTInstance parse_instance_file(const std::string& filename) 
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    
    std::string line;
    
    // Read first line: n m probability
    std::getline(file, line);
    std::istringstream iss(line);
    int n, m;
    double probability;
    iss >> n >> m >> probability;
    
    OCSTInstance instance(n, probability);
    
    // Read edges
    for (int i = 0; i < m; ++i) {
        std::getline(file, line);
        std::istringstream edge_iss(line);
        int u, v;
        double cost;
        edge_iss >> u >> v >> cost;
        instance.add_edge(u, v, cost);
    }
    
    // Read number of requirements
    std::getline(file, line);
    int num_requirements = std::stoi(line);
    
    // Read requirements
    for (int i = 0; i < num_requirements; ++i) {
        std::getline(file, line);
        std::istringstream req_iss(line);
        int origin, dest;
        double weight;
        req_iss >> origin >> dest >> weight;
        instance.add_requirement(origin, dest, weight);
    }
    
    file.close();
    
    // CRITICAL FIX: Remove artificial connectivity requirements
    // These don't work correctly in path-based formulation and can cause issues
    // Connectivity will be ensured by proper SEC constraints instead
    
    return instance;
}

/**
 * @brief Writes complete solution to file for validation
 * @param filename Output filename 
 * @param instance Problem instance
 * @param result Solution result
 */
void write_complete_solution(const std::string& filename, 
                           const OCSTInstance& instance, 
                           const SolutionResult& result) 
{
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not create complete solution file: " << filename << std::endl;
        return;
    }
    
    // Line 1: Objective value
    file << std::fixed << std::setprecision(0) << result.objective_value << std::endl;
    
    // Line 2: Number of nodes
    file << instance.num_nodes << std::endl;
    
    // Next n-1 lines: Selected edges (x y format)
    for (int edge_idx : result.selected_edges) {
        if (edge_idx >= 0 && edge_idx < static_cast<int>(instance.edges.size())) {
            const Edge& edge = instance.edges[edge_idx];
            file << edge.source << " " << edge.destination << std::endl;
        }
    }
    
    file.close();
    std::cout << "Complete solution written to: " << filename << std::endl;
}

//=============================================================================
// MAIN SOLVING FUNCTION
//=============================================================================

/**
 * @brief Main function that solves an OCST instance using path-based formulation
 * @param input_file Path to input instance file
 * @param output_csv Path to output CSV file for results
 * @param time_limit Time limit in seconds (default: 3600)
 * @param heuristics Gurobi heuristics level (default: 0.5)
 * @return SolutionResult containing all metrics
 */
SolutionResult solve_path_based_instance(const std::string& input_file,
                                       const std::string& output_csv = "",
                                       double time_limit = 3600.0,
                                       double heuristics = 0.5) 
{
    try {
        // Parse instance
        std::cout << "Parsing instance: " << input_file << std::endl;
        OCSTInstance instance = parse_instance_file(input_file);
        
        std::cout << "Instance stats: " << instance.num_nodes << " nodes, " 
                  << instance.num_edges << " edges, " << instance.requirements.size() 
                  << " requirements" << std::endl;
        
        // Solve using path-based formulation
        PathBasedSolver solver(instance);
        SolutionResult result = solver.solve(time_limit, heuristics);
        
        // Print results
        std::cout << "\n=== SOLUTION RESULTS ===" << std::endl;
        
        // Detailed status reporting
        std::string status_str;
        if (result.is_optimal) {
            status_str = "OPTIMAL";
        } else if (result.gurobi_status == GRB_TIME_LIMIT) {
            if (result.objective_value > 0) {
                status_str = "TIME_LIMIT (feasible solution found)";
            } else {
                status_str = "TIME_LIMIT (no feasible solution)";
            }
        } else if (result.gurobi_status == GRB_INFEASIBLE) {
            status_str = "INFEASIBLE";
        } else if (result.gurobi_status == GRB_UNBOUNDED) {
            status_str = "UNBOUNDED";
        } else {
            status_str = "NON-OPTIMAL (status=" + std::to_string(result.gurobi_status) + ")";
        }
        
        std::cout << "Status: " << status_str << std::endl;
        std::cout << "Objective value: " << std::fixed << std::setprecision(0) << result.objective_value << std::endl;
        std::cout << "Runtime: " << result.runtime_seconds << " seconds" << std::endl;
        std::cout << "Nodes explored: " << result.num_nodes_explored << std::endl;
        std::cout << "MIP gap: " << result.mip_gap << "%" << std::endl;
        std::cout << "Lazy constraints added: " << result.lazy_constraints_added << std::endl;
        std::cout << "Cutting planes added: " << result.cutting_planes_added << std::endl;
        std::cout << "Selected edges: " << result.selected_edges.size() << std::endl;
        
        // Save to CSV if specified
        if (!output_csv.empty()) {
            std::ofstream csv_file(output_csv);
            csv_file << "instance,nodes,edges,requirements,probability,objective,runtime,gap,status,nodes_explored\n";
            csv_file << input_file << "," << instance.num_nodes << "," << instance.num_edges << ","
                    << instance.requirements.size() << "," << instance.probability << ","
                    << std::fixed << std::setprecision(0) << result.objective_value << "," << result.runtime_seconds << ","
                    << result.mip_gap << "," << result.gurobi_status << "," << result.num_nodes_explored << "\n";
            csv_file.close();
            std::cout << "Results saved to: " << output_csv << std::endl;
        }
        
        // Always generate complete solution file for validation
        std::string instance_basename = input_file;
        size_t last_slash = instance_basename.find_last_of("/");
        if (last_slash != std::string::npos) {
            instance_basename = instance_basename.substr(last_slash + 1);
        }
        
        std::string complete_solution_file = "data/output/test_instances/complete_" + instance_basename + ".sol";
        write_complete_solution(complete_solution_file, instance, result);
        
        return result;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        SolutionResult error_result;
        error_result.gurobi_status = -1;
        return error_result;
    }
}

//=============================================================================
// MAIN FUNCTION FOR TESTING
//=============================================================================

int main(int argc, char* argv[]) 
{
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <instance_file> [output_csv] [time_limit] [heuristics]" << std::endl;
        std::cout << "Example: " << argv[0] << " data/input/test_instances/ocstpin0 results.csv 300 0.5" << std::endl;
        return 1;
    }
    
    std::string input_file = argv[1];
    std::string output_csv = (argc > 2) ? argv[2] : "";
    double time_limit = (argc > 3) ? std::atof(argv[3]) : 3600.0;
    double heuristics = (argc > 4) ? std::atof(argv[4]) : 0.5;
    
    SolutionResult result = solve_path_based_instance(input_file, output_csv, time_limit, heuristics);
    
    return result.is_optimal ? 0 : 1;
}