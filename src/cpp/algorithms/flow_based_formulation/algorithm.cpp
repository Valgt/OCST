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

// Gurobi integration
#include <gurobi_c++.h>

//=============================================================================
// DATA STRUCTURES
//=============================================================================

/**
 * @brief Disjoint Set Union (DSU) data structure for MST algorithm
 */
class DSU {
private:
    std::vector<int> parent_;
    std::vector<int> size_;
    
public:
    DSU(int n) : parent_(n), size_(n, 1) {
        for (int i = 0; i < n; ++i) {
            parent_[i] = i;
        }
    }
    
    int find_set(int v) {
        if (v == parent_[v]) return v;
        return parent_[v] = find_set(parent_[v]);  // Path compression
    }
    
    void union_sets(int a, int b) {
        a = find_set(a);
        b = find_set(b);
        if (a != b) {
            if (size_[a] < size_[b]) std::swap(a, b);  // Union by size
            parent_[b] = a;
            size_[a] += size_[b];
        }
    }
    
    bool same_set(int a, int b) {
        return find_set(a) == find_set(b);
    }
};

/**
 * @brief Edge structure for MST algorithm
 */
struct MSTEdge {
    int u, v;
    double weight;
    int edge_index;  // Index in original edges array
    
    MSTEdge(int u, int v, double w, int idx) : u(u), v(v), weight(w), edge_index(idx) {}
    
    bool operator<(const MSTEdge& other) const {
        return weight < other.weight;
    }
};

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
// SEC CALLBACK FOR SUBTOUR ELIMINATION
//=============================================================================

/**
 * @brief SEC Callback for subtour elimination constraints
 */
class SECCallback : public GRBCallback {
private:
    const OCSTInstance& instance_;
    const std::vector<GRBVar>& x_vars_;
    int& lazy_constraints_count_;
    
public:
    SECCallback(const OCSTInstance& instance, const std::vector<GRBVar>& x_vars, int& lazy_count)
        : instance_(instance), x_vars_(x_vars), lazy_constraints_count_(lazy_count) {}
    
protected:
    void callback() override {
        if (where == GRB_CB_MIPSOL) {
            // Get current solution
            std::vector<double> x_vals(instance_.num_edges);
            for (int e = 0; e < instance_.num_edges; ++e) {
                x_vals[e] = getSolution(x_vars_[e]);
            }
            
            // Find connected components
            std::vector<int> component(instance_.num_nodes, -1);
            int num_components = 0;
            
            for (int v = 0; v < instance_.num_nodes; ++v) {
                if (component[v] == -1) {
                    dfs_component(v, num_components, component, x_vals);
                    num_components++;
                }
            }
            
            // Add cuts for components with size > 1
            for (int c = 0; c < num_components; ++c) {
                std::vector<int> component_nodes;
                for (int v = 0; v < instance_.num_nodes; ++v) {
                    if (component[v] == c) {
                        component_nodes.push_back(v);
                    }
                }
                
                if (component_nodes.size() > 1) {
                    add_lazy_constraints(component_nodes);
                }
            }
        }
    }
    
private:
    void dfs_component(int v, int comp_id, std::vector<int>& component, 
                      const std::vector<double>& x_vals) {
        component[v] = comp_id;
        
        for (int e = 0; e < instance_.num_edges; ++e) {
            const Edge& edge = instance_.edges[e];
            int neighbor = -1;
            
            if (edge.source == v) neighbor = edge.destination;
            else if (edge.destination == v) neighbor = edge.source;
            
            if (neighbor != -1 && component[neighbor] == -1 && x_vals[e] > 0.5) {
                dfs_component(neighbor, comp_id, component, x_vals);
            }
        }
    }
    
    void add_lazy_constraints(const std::vector<int>& component) {
        GRBLinExpr cut_expr = 0;
        int component_size = component.size();
        
        // Sum edges with BOTH endpoints in component
        for (int e = 0; e < instance_.num_edges; ++e) {
            const Edge& edge = instance_.edges[e];
            bool source_in_component = false;
            bool dest_in_component = false;
            
            for (int v : component) {
                if (edge.source == v) source_in_component = true;
                if (edge.destination == v) dest_in_component = true;
            }
            
            // Only add edge if BOTH endpoints are in component
            if (source_in_component && dest_in_component) {
                cut_expr += x_vars_[e];
            }
        }
        
        // Add cut: sum of edges in component <= |component| - 1
        addLazy(cut_expr <= component_size - 1);
        lazy_constraints_count_++;
    }
};

// FLOW-BASED FORMULATION SOLVER
//=============================================================================

/**
 * @brief Flow-based formulation solver for OCST problem
 */
class FlowBasedSolver 
{
private:
    const OCSTInstance& instance_;
    GRBEnv env_;
    GRBModel model_;
    
    // Variables para Flow-Based (formulación real según 4.25-4.35)
    std::vector<GRBVar> x_vars_;                    // x[edge] - binary (4.35)
    std::vector<std::vector<GRBVar>> f_vars_;       // f[origin][arc] - continuous (4.33)
    std::vector<std::vector<GRBVar>> y_vars_;       // y[origin][arc] - binary (4.34)
    
    // Métricas de seguimiento
    int lazy_constraints_count_;
    int cutting_planes_count_;
    
public:
    explicit FlowBasedSolver(const OCSTInstance& instance) 
        : instance_(instance), env_(GRBEnv()), model_(GRBModel(env_))
    {
        lazy_constraints_count_ = 0;
        cutting_planes_count_ = 0;
        
        // Configure Gurobi parameters as specified in pseudocode
        env_.set(GRB_IntParam_OutputFlag, 0);  // Disable console output
        model_.set(GRB_IntParam_LazyConstraints, 1);  // Enable lazy constraints
        model_.set(GRB_IntParam_PreCrush, 1);  // Ensure lazy constraints work with presolve
        model_.set(GRB_IntParam_Presolve, 0);  // Disable presolve for better cut separation
        model_.set(GRB_IntParam_Threads, 1);   // Single thread for reproducibility
        model_.set(GRB_IntParam_Cuts, 0);      // Disable default cuts to prioritize custom ones
    }
    
    /**
     * @brief Solves the OCST instance using flow-based formulation
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
            
            // Add constraints
            add_structural_constraints();
            add_flow_constraints();
            add_arborescence_constraints();
            add_coupling_constraints();
            
            // Set objective function
            set_objective();
            
            // Set initial solution using MST for warm-start
            std::vector<int> mst_edges = find_mst();
            set_initial_solution(mst_edges);
            
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
     * @brief Finds Minimum Spanning Tree using Kruskal's algorithm with DSU
     * @return Vector of edge indices that form the MST
     */
    std::vector<int> find_mst() 
    {
        std::vector<MSTEdge> edges;
        
        // Create MST edges from instance edges
        for (int e = 0; e < instance_.num_edges; ++e) {
            const Edge& edge = instance_.edges[e];
            edges.emplace_back(edge.source, edge.destination, edge.cost, e);
        }
        
        // Sort edges by weight (Kruskal's algorithm)
        std::sort(edges.begin(), edges.end());
        
        // Initialize DSU
        DSU dsu(instance_.num_nodes);
        std::vector<int> mst_edges;
        
        // Kruskal's algorithm
        for (const auto& edge : edges) {
            if (!dsu.same_set(edge.u, edge.v)) {
                dsu.union_sets(edge.u, edge.v);
                mst_edges.push_back(edge.edge_index);
                
                // MST has exactly n-1 edges
                if (mst_edges.size() == static_cast<size_t>(instance_.num_nodes - 1)) {
                    break;
                }
            }
        }
        
        return mst_edges;
    }
    
    /**
     * @brief Sets initial solution using MST
     * @param mst_edges Vector of edge indices that form the MST
     */
    void set_initial_solution(const std::vector<int>& mst_edges) 
    {
        // Set x variables to 1 for MST edges, 0 for others
        for (int e = 0; e < instance_.num_edges; ++e) {
            bool in_mst = std::find(mst_edges.begin(), mst_edges.end(), e) != mst_edges.end();
            x_vars_[e].set(GRB_DoubleAttr_Start, in_mst ? 1.0 : 0.0);
        }
        
        // For flow variables, we could set them based on MST paths, but for now
        // we let Gurobi compute them from the x variables
        // This provides a good warm-start for the branch-and-cut
    }
    void create_variables() 
    {
        // Crear variables x[edge] - binarias (4.35)
        x_vars_.resize(instance_.num_edges);
        for (int e = 0; e < instance_.num_edges; ++e) {
            const Edge& edge = instance_.edges[e];
            std::string var_name = "x_" + std::to_string(edge.source) + "_" + std::to_string(edge.destination);
            x_vars_[e] = model_.addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name);
        }
        
        // Crear variables f[origin][arc] y y[origin][arc] para cada origen
        f_vars_.resize(instance_.num_nodes);
        y_vars_.resize(instance_.num_nodes);
        
        for (int o = 0; o < instance_.num_nodes; ++o) {
            // Cada arista no dirigida se convierte en 2 arcos dirigidos
            f_vars_[o].resize(instance_.num_edges * 2);
            y_vars_[o].resize(instance_.num_edges * 2);
            
            for (int e = 0; e < instance_.num_edges; ++e) {
                const Edge& edge = instance_.edges[e];
                
                // Dirección forward (i -> j)
                std::string f_name_fwd = "f_" + std::to_string(o) + "_" + 
                                        std::to_string(edge.source) + "_" + std::to_string(edge.destination);
                std::string y_name_fwd = "y_" + std::to_string(o) + "_" + 
                                        std::to_string(edge.source) + "_" + std::to_string(edge.destination);
                
                f_vars_[o][2*e] = model_.addVar(0.0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, f_name_fwd);
                y_vars_[o][2*e] = model_.addVar(0.0, 1.0, 0.0, GRB_BINARY, y_name_fwd);
                
                // Dirección backward (j -> i)
                std::string f_name_bwd = "f_" + std::to_string(o) + "_" + 
                                        std::to_string(edge.destination) + "_" + std::to_string(edge.source);
                std::string y_name_bwd = "y_" + std::to_string(o) + "_" + 
                                        std::to_string(edge.destination) + "_" + std::to_string(edge.source);
                
                f_vars_[o][2*e + 1] = model_.addVar(0.0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, f_name_bwd);
                y_vars_[o][2*e + 1] = model_.addVar(0.0, 1.0, 0.0, GRB_BINARY, y_name_bwd);
            }
        }
        
        // Actualizar modelo después de crear variables
        model_.update();
    }
    
    /**
     * @brief Adds structural constraints (4.26-4.27): spanning tree constraints
     */
    void add_structural_constraints() 
    {
        // Restricción (4.26): sum of edges = n-1
        GRBLinExpr tree_constraint = 0;
        for (int e = 0; e < instance_.num_edges; ++e) {
            tree_constraint += x_vars_[e];
        }
        model_.addConstr(tree_constraint == instance_.num_nodes - 1, "tree_constraint");
        
        // Restricciones (4.27): subtour elimination constraints (SEC)
        // Para cada subconjunto S de V, |S| >= 2, sum_{ij in E(S)} x_ij <= |S| - 1
        // Implementamos SEC usando lazy constraints para eficiencia
        // Instanciar en stack para mejor gestión de memoria
        static SECCallback sec_callback(instance_, x_vars_, lazy_constraints_count_);
        model_.setCallback(&sec_callback);
    }
    
    /**
     * @brief Adds flow constraints (4.28-4.29): flow conservation per origin
     */
    void add_flow_constraints() 
    {
        // Calcular demanda total por origen
        std::vector<double> total_demand_from(instance_.num_nodes, 0.0);
        std::vector<std::vector<double>> demand_matrix(instance_.num_nodes, std::vector<double>(instance_.num_nodes, 0.0));
        
        for (const auto& req : instance_.requirements) {
            total_demand_from[req.origin] += req.weight;
            demand_matrix[req.origin][req.destination] += req.weight;  // Acumular demandas
        }
        
        // Restricciones (4.28): conservación de flujo para cada origen o y vértice j != o
        for (int o = 0; o < instance_.num_nodes; ++o) {
            for (int j = 0; j < instance_.num_nodes; ++j) {
                if (j == o) continue;  // Skip origin itself
                
                GRBLinExpr flow_in = 0;
                GRBLinExpr flow_out = 0;
                
                // Sumar flujo entrante a j desde todos los arcos ij
                for (int e = 0; e < instance_.num_edges; ++e) {
                    const Edge& edge = instance_.edges[e];
                    
                    // Arco ij -> j (forward)
                    if (edge.destination == j) {
                        flow_in += f_vars_[o][2*e];
                    }
                    // Arco ji -> j (backward) 
                    if (edge.source == j) {
                        flow_in += f_vars_[o][2*e + 1];
                    }
                    
                    // Sumar flujo saliente de j hacia todos los arcos jk
                    // Arco jk saliente (forward)
                    if (edge.source == j) {
                        flow_out += f_vars_[o][2*e];
                    }
                    // Arco kj saliente (backward)
                    if (edge.destination == j) {
                        flow_out += f_vars_[o][2*e + 1];
                    }
                }
                
                // Restricción: flow_in - flow_out = w_oj (demanda en j desde origen o)
                double demand_at_j = demand_matrix[o][j];
                std::string constr_name = "flow_conservation_" + std::to_string(o) + "_" + std::to_string(j);
                model_.addConstr(flow_in - flow_out == demand_at_j, constr_name);
            }
        }
        
        // Restricciones (4.29): flujo inicial desde cada origen o
        for (int o = 0; o < instance_.num_nodes; ++o) {
            GRBLinExpr initial_flow = 0;
            
            // Sumar flujo saliente desde origen o
            for (int e = 0; e < instance_.num_edges; ++e) {
                const Edge& edge = instance_.edges[e];
                
                // Arco o -> k (forward)
                if (edge.source == o) {
                    initial_flow += f_vars_[o][2*e];
                }
                // Arco k -> o (backward, pero saliente desde o)
                if (edge.destination == o) {
                    initial_flow += f_vars_[o][2*e + 1];
                }
            }
            
            // Restricción: flujo inicial = suma de demandas con origen o
            std::string constr_name = "initial_flow_" + std::to_string(o);
            model_.addConstr(initial_flow == total_demand_from[o], constr_name);
        }
        
        // Restricción crítica: flujo entrante al origen = 0 (evita ciclos en la raíz)
        for (int o = 0; o < instance_.num_nodes; ++o) {
            GRBLinExpr inflow_to_root = 0;
            
            // Sumar flujo entrante al origen o
            for (int e = 0; e < instance_.num_edges; ++e) {
                const Edge& edge = instance_.edges[e];
                
                // Arco k -> o (forward)
                if (edge.destination == o) {
                    inflow_to_root += f_vars_[o][2*e];
                }
                // Arco o -> k (backward, pero entrante a o)
                if (edge.source == o) {
                    inflow_to_root += f_vars_[o][2*e + 1];
                }
            }
            
            // Restricción: flujo entrante al origen = 0
            std::string constr_name = "root_inflow_zero_" + std::to_string(o);
            model_.addConstr(inflow_to_root == 0, constr_name);
        }
    }
    
    /**
     * @brief Adds arborescence constraints (4.31-4.32): arborescence properties per origin
     */
    void add_arborescence_constraints() 
    {
        // Restricciones (4.31): cada origen o debe tener exactamente n-1 arcos en su arborescencia
        for (int o = 0; o < instance_.num_nodes; ++o) {
            GRBLinExpr arborescence_size = 0;
            
            for (int e = 0; e < instance_.num_edges; ++e) {
                // Contar ambos arcos dirigidos de cada arista
                arborescence_size += y_vars_[o][2*e];      // forward arc
                arborescence_size += y_vars_[o][2*e + 1];  // backward arc
            }
            
            std::string constr_name = "arborescence_size_" + std::to_string(o);
            model_.addConstr(arborescence_size == instance_.num_nodes - 1, constr_name);
        }
        
        // Restricciones (4.32): y_o_ij + y_o_ji <= x_ij (acoplamiento arborescencia-arista)
        for (int o = 0; o < instance_.num_nodes; ++o) {
            for (int e = 0; e < instance_.num_edges; ++e) {
                GRBLinExpr arborescence_arcs = y_vars_[o][2*e] + y_vars_[o][2*e + 1];
                std::string constr_name = "arborescence_edge_" + std::to_string(o) + "_" + std::to_string(e);
                model_.addConstr(arborescence_arcs <= x_vars_[e], constr_name);
            }
        }
    }
    
    /**
     * @brief Adds coupling constraints (4.30): flow-arborescence coupling
     */
    void add_coupling_constraints() 
    {
        // Calcular suma de demandas por origen para Big-M dinámico
        std::vector<double> total_demand_from(instance_.num_nodes, 0.0);
        for (const auto& req : instance_.requirements) {
            total_demand_from[req.origin] += req.weight;
        }
        
        // Restricciones (4.30): f_o_ij + f_o_ji <= sumW * x_ij (acoplamiento flujo-arista)
        for (int o = 0; o < instance_.num_nodes; ++o) {
            double sumW = total_demand_from[o];
            if (sumW <= 0) continue;  // Skip origins with no demand
            
            for (int e = 0; e < instance_.num_edges; ++e) {
                const Edge& edge = instance_.edges[e];
                
                // Una sola desigualdad: f_o_ij + f_o_ji <= sumW * x_ij
                GRBLinExpr total_flow = f_vars_[o][2*e] + f_vars_[o][2*e + 1];
                std::string constr_name = "flow_edge_" + std::to_string(o) + "_" + 
                                         std::to_string(edge.source) + "_" + 
                                         std::to_string(edge.destination);
                model_.addConstr(total_flow <= sumW * x_vars_[e], constr_name);
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
        
        // Objetivo Flow-Based (4.25): min Σ_o Σ_ij c_ij * f_o_ij
        for (int o = 0; o < instance_.num_nodes; ++o) {
            for (int e = 0; e < instance_.num_edges; ++e) {
                const Edge& edge = instance_.edges[e];
                
                // Forward arc: c_ij * f_o_ij
                objective += edge.cost * f_vars_[o][2*e];
                
                // Backward arc: c_ji * f_o_ji (c_ji = c_ij)
                objective += edge.cost * f_vars_[o][2*e + 1];
            }
        }
        
        model_.setObjective(objective, GRB_MINIMIZE);
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
    file << std::fixed << std::setprecision(6) << result.objective_value << std::endl;
    
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
 * @brief Main function that solves an OCST instance using flow-based formulation
 * @param input_file Path to input instance file
 * @param output_csv Path to output CSV file for results
 * @param time_limit Time limit in seconds (default: 3600)
 * @param heuristics Gurobi heuristics level (default: 0.5)
 * @return SolutionResult containing all metrics
 */
SolutionResult solve_flow_based_instance(const std::string& input_file,
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
        
        // Solve using flow-based formulation
        FlowBasedSolver solver(instance);
        SolutionResult result = solver.solve(time_limit, heuristics);
        
        // Print results
        std::cout << "\n=== SOLUTION RESULTS ===" << std::endl;
        std::cout << "Status: " << (result.is_optimal ? "OPTIMAL" : "NON-OPTIMAL") << std::endl;
        std::cout << "Objective value: " << std::fixed << std::setprecision(6) << result.objective_value << std::endl;
        std::cout << "Runtime: " << result.runtime_seconds << " seconds" << std::endl;
        std::cout << "Nodes explored: " << result.num_nodes_explored << std::endl;
        std::cout << "MIP gap: " << result.mip_gap << "%" << std::endl;
        std::cout << "Selected edges: " << result.selected_edges.size() << std::endl;
        
        // Save to CSV if specified
        if (!output_csv.empty()) {
            std::ofstream csv_file(output_csv);
            csv_file << "instance,nodes,edges,requirements,probability,objective,runtime,gap,status,nodes_explored\n";
            csv_file << input_file << "," << instance.num_nodes << "," << instance.num_edges << ","
                    << instance.requirements.size() << "," << instance.probability << ","
                    << std::fixed << std::setprecision(6) << result.objective_value << "," << result.runtime_seconds << ","
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
    
    SolutionResult result = solve_flow_based_instance(input_file, output_csv, time_limit, heuristics);
    
    return result.is_optimal ? 0 : 1;
}