#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <queue>
#include <iomanip>
#include <cassert>
#include <memory>

// Gurobi integration
#include <gurobi_c++.h>

using namespace std;

// Network class for Edmonds-Karp algorithm (from expert implementation)
class Network {
public:
    Network(const int n, const vector<pair<double, pair<int, int>>> arcCapacity);
    
    /*Edmonds–Karp algorithm*/
    double MaxFlow(int s, int t);
    vector<int> GetIndexArcsMinCut(int s, int t);
    vector<int> GetVerticesMinCut(int s, int t);
    static vector<int> Complement(int n, vector<int> indexes);
    double CapCut(vector<int> indexes);
    
private:
    static const double kEps;
    static const double kInf;
    
    bool Bfs();
    double Mincap();
    void Update(double maxi);
    void InitMaxFlow(int s, int t);
    
    const int n_;
    const vector<pair<double, pair<int, int>>> arcCapacity_;
    
    int source_, sink_;
    vector<bool> visited_;
    vector<int> parent_;
    vector<unordered_map<int, double>> residual_;
};

const double Network::kInf = 1e10;
const double Network::kEps = 1e-7;

Network::Network(const int n, const vector<pair<double, pair<int, int>>> arcCapacity)
    : n_(n), arcCapacity_(arcCapacity) {
    this->visited_ = vector<bool>(n + 1, false);
    this->parent_ = vector<int>(n + 1, -1);
    this->residual_.resize(n);
    source_ = sink_ = -1;
}

bool Network::Bfs() {
    queue<int> q;
    q.push(source_);
    visited_[source_] = true;
    
    while (!q.empty()) {
        int node = q.front();
        q.pop();
        if (node == sink_) {
            return true;
        }
        for (auto it : residual_[node]) {
            if (it.second > kEps && !visited_[it.first]) {
                parent_[it.first] = node;
                visited_[it.first] = true;
                q.push(it.first);
            }
        }
    }
    return false;
}

double Network::Mincap() {
    double maxi = kInf;
    int cur = sink_;
    while (cur != source_) {
        maxi = min(maxi, residual_[parent_[cur]][cur]);
        cur = parent_[cur];
    }
    return maxi;
}

void Network::Update(double maxi) {
    int cur = sink_;
    while (cur != source_) {
        residual_[parent_[cur]][cur] -= maxi;
        residual_[cur][parent_[cur]] += maxi;
        cur = parent_[cur];
    }
}

void Network::InitMaxFlow(int s, int t) {
    this->source_ = s;
    this->sink_ = t;
    
    this->visited_ = vector<bool>(n_ + 1, false);
    this->parent_ = vector<int>(n_ + 1, -1);
    this->residual_.resize(n_);
    residual_.clear();
    residual_.resize(n_);
    
    for (auto it : arcCapacity_) {
        double cost = it.first;
        int i = it.second.first;
        int j = it.second.second;
        residual_[i][j] += cost;  // multiple arcs are allowed, they are combined
    }
}

double Network::MaxFlow(int s, int t) {
    InitMaxFlow(s, t);
    
    double max_flow = 0;
    
    while (Bfs()) {
        double mf = Mincap();
        Update(mf);
        if (mf < kEps) {
            break;
        }
        max_flow += mf;
        fill(visited_.begin(), visited_.end(), false);
        fill(parent_.begin(), parent_.end(), -1);
    }
    return max_flow;
}

vector<int> Network::GetIndexArcsMinCut(int s, int t) {
    double max_flow = MaxFlow(s, t);
    vector<int> index_arcs;
    double cap_min_cut = 0;
    for (size_t index = 0; index < arcCapacity_.size(); ++index) {
        int i = arcCapacity_[index].second.first;
        int j = arcCapacity_[index].second.second;
        if (visited_[i] and (!visited_[j])) {
            cap_min_cut += arcCapacity_[index].first;
            index_arcs.push_back(index);
        }
    }
    assert(fabs(cap_min_cut - max_flow) < kEps);  // max-flow min-cut theorem
    return index_arcs;
}

vector<int> Network::GetVerticesMinCut(int s, int t) {
    double max_flow = MaxFlow(s, t);
    vector<int> vertices;
    for (int i = 0; i < n_; ++i) {
        if (visited_[i]) {
            vertices.push_back(i);
        }
    }
    return vertices;
}

vector<int> Network::Complement(int n, vector<int> indexes) {
    vector<bool> inS(n, false);  // S is the min s-t cut, s in S
    for (auto it : indexes) {
        assert(it >= 0 and it < n);
        inS[it] = true;
    }
    vector<int> comp;
    for (int i = 0; i < n; ++i) {
        if (!inS[i]) {
            comp.push_back(i);
        }
    }
    assert(static_cast<int>(indexes.size() + comp.size()) == n);
    return comp;
}

double Network::CapCut(vector<int> indexes) {
    vector<bool> set_s(n_, false);
    for (auto it : indexes) {
        assert(it >= 0 and it < n_);
        set_s[it] = true;
    }
    
    double cap = 0;
    for (auto it : arcCapacity_) {
        int p = it.second.first;
        int q = it.second.second;
        
        if (set_s[p] and (!set_s[q])) {
            cap += it.first;
        }
    }
    return cap;
}

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
    const std::vector<std::vector<int>>& edge_lookup_;
    int& lazy_constraints_count_;
    int& cutting_planes_count_;
    
public:
    SECCallback(const OCSTInstance& instance, const std::vector<GRBVar>& x_vars, 
                const std::vector<std::vector<int>>& edge_lookup,
                int& lazy_count, int& cutting_count)
        : instance_(instance), x_vars_(x_vars), edge_lookup_(edge_lookup),
          lazy_constraints_count_(lazy_count), cutting_planes_count_(cutting_count) {}
    
protected:
    void callback() override {
        if (where == GRB_CB_MIPSOL) {
            // Lazy constraints for integer solutions
            add_lazy_constraints_integer();
        } else if (where == GRB_CB_MIPNODE) {
            // Fractional cuts for fractional solutions using Network class
            // Only add cuts when LP relaxation is optimal
            if (getIntInfo(GRB_CB_MIPNODE_STATUS) == GRB_OPTIMAL) {
                add_fractional_cuts();
            }
        }
    }
    
private:
    void add_lazy_constraints_integer() {
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
    
    void add_fractional_cuts() {
        try {
            // Get fractional solution from node relaxation
            std::vector<std::vector<double>> sol(instance_.num_nodes, std::vector<double>(instance_.num_nodes, 0.0));
            
            // Build symmetric matrix sol[i][j] from fractional x values
            for (int e = 0; e < instance_.num_edges; ++e) {
                double x_val = getNodeRel(x_vars_[e]);
                const Edge& edge = instance_.edges[e];
                sol[edge.source][edge.destination] = x_val;
                sol[edge.destination][edge.source] = x_val;
            }
            
            // Calculate deltaJ[j] = sum of outgoing fractions for each node j
            std::vector<double> deltaJ(instance_.num_nodes, 0.0);
            for (int j = 0; j < instance_.num_nodes; ++j) {
                for (int k = 0; k < instance_.num_nodes; ++k) {
                    deltaJ[j] += sol[j][k];
                }
            }
            
            // Build CSI network with fractional capacities and mapping
            std::vector<std::pair<double, std::pair<int, int>>> csi_capacities;
            std::vector<int> arc_to_edge;  // Maps directed arc index to edge index
            
            for (int e = 0; e < instance_.num_edges; ++e) {
                double x_val = getNodeRel(x_vars_[e]);
                if (x_val > 1e-6) {  // Only consider edges with significant flow
                    const Edge& edge = instance_.edges[e];
                    // Forward direction: u -> v
                    csi_capacities.push_back({x_val, {edge.source, edge.destination}});
                    arc_to_edge.push_back(e);
                    // Backward direction: v -> u  
                    csi_capacities.push_back({x_val, {edge.destination, edge.source}});
                    arc_to_edge.push_back(e);
                }
            }
            
            Network csi_network(instance_.num_nodes, csi_capacities);
            
            // Counter for cuts added in THIS invocation (for statistics/logging)
            int cuts_this_round = 0;
            
            // Process both configurations (rConfig = 0, 1) with fixed root
            for (int rConfig = 0; rConfig < 2; ++rConfig) {
                int rFixed = (rConfig == 0) ? 0 : instance_.num_nodes - 1;  // Fixed root
                
                // Only evaluate 2(n-1) pairs: (rFixed, t) for all t != rFixed
                for (int t = 0; t < instance_.num_nodes; ++t) {
                    if (t == rFixed) continue;
                        
                    // === CSI CUTS (Connectivity) ===
                    double maxFlowCSI = csi_network.MaxFlow(rFixed, t);
                    if (maxFlowCSI < 0.99) {  // Connectivity violation
                        std::vector<int> cut_arcs = csi_network.GetIndexArcsMinCut(rFixed, t);
                        add_csi_cut(cut_arcs, arc_to_edge);
                        cuts_this_round++;
                    }
                    
                    // === SEC CUTS (Subtour) ===
                    // Build SEC network with auxiliary arcs
                    std::vector<std::pair<double, std::pair<int, int>>> sec_capacities;
                    
                    // Add normalized original arcs: sol[i][j] * 0.5
                    for (int i = 0; i < instance_.num_nodes; ++i) {
                        for (int j = 0; j < instance_.num_nodes; ++j) {
                            if (i != j && sol[i][j] > 1e-6) {
                                sec_capacities.push_back({sol[i][j] * 0.5, {i, j}});
                            }
                        }
                    }
                    
                    // Add auxiliary arcs: (i,t) with capacity 1 for i != t
                    for (int i = 0; i < instance_.num_nodes; ++i) {
                        if (i != t) {
                            sec_capacities.push_back({1.0, {i, t}});
                        }
                    }
                    
                    // Add auxiliary arcs: (rFixed,i) with capacity deltaJ[i] * 0.5 for i != rFixed
                    for (int i = 0; i < instance_.num_nodes; ++i) {
                        if (i != rFixed) {
                            sec_capacities.push_back({deltaJ[i] * 0.5, {rFixed, i}});
                        }
                    }
                    
                    Network sec_network(instance_.num_nodes, sec_capacities);
                    double minCutSEC = sec_network.MaxFlow(rFixed, t);
                    
                    if (minCutSEC < instance_.num_nodes - 1e-6) {  // SEC violation
                        std::vector<int> S = sec_network.GetVerticesMinCut(rFixed, t);
                        add_sec_cut(S);
                        cuts_this_round++;
                    }
                    
                    // REMOVED: MAX_CUTS_PER_ROUND limit
                    // Rationale: Early return leaves unprocessed (s,t) pairs with potential 
                    // CSI/SEC violations, making the LP relaxation inconsistent. We must 
                    // process all 2(n-1) pairs to ensure correctness, even if it adds many cuts.
                }
            }
            
        } catch (const GRBException& e) {
            // If getNodeRel fails, skip fractional cuts for this node
            // This is expected behavior in some Gurobi contexts
        }
    }
    
    /**
     * @brief Adds CSI connectivity cut: sum of cut edges >= 1
     */
    void add_csi_cut(const std::vector<int>& cut_arcs, const std::vector<int>& arc_to_edge) {
        if (cut_arcs.empty()) return;
        
        GRBLinExpr cut_expr;
        for (int arc_idx : cut_arcs) {
            if (arc_idx >= 0 && arc_idx < static_cast<int>(arc_to_edge.size())) {
                int edge_idx = arc_to_edge[arc_idx];
                if (edge_idx >= 0 && edge_idx < instance_.num_edges) {
                    cut_expr += x_vars_[edge_idx];
                }
            }
        }
        
        if (cut_expr.size() > 0) {
            addCut(cut_expr >= 1.0);
            cutting_planes_count_++;
        }
    }
    
    /**
     * @brief Adds SEC subtour cut: sum of edges within subset S <= |S| - 1
     */
    void add_sec_cut(const std::vector<int>& S) {
        if (S.size() <= 1 || S.size() >= static_cast<size_t>(instance_.num_nodes - 1)) {
            return;  // Trivial cuts
        }
        
        GRBLinExpr cut_expr;
        
        // Sum edges with both endpoints in S using O(1) lookup
        for (size_t i = 0; i < S.size(); ++i) {
            for (size_t j = i + 1; j < S.size(); ++j) {
                int u = S[i];
                int v = S[j];
                
                // Use O(1) lookup instead of O(m) search
                int edge_idx = edge_lookup_[u][v];
                if (edge_idx >= 0) {
                    cut_expr += x_vars_[edge_idx];
                }
            }
        }
        
        // Add cut if we found edges within the subset
        if (cut_expr.size() > 0) {
            addCut(cut_expr <= static_cast<double>(S.size()) - 1.0);
            cutting_planes_count_++;
        }
    }
    
    /**
     * @brief Finds edge index for given pair of nodes using O(1) lookup
     */
    int find_edge_index(int u, int v) {
        if (u >= 0 && u < instance_.num_nodes && v >= 0 && v < instance_.num_nodes) {
            return edge_lookup_[u][v];
        }
        return -1;  // Invalid nodes or edge not found
    }
    
private:
    void dfs_component(int v, int comp_id, std::vector<int>& component, 
                      const std::vector<double>& x_vals) {
        component[v] = comp_id;
        
        // OPTIMIZATION: Use adjacency matrix to iterate only neighbors, not all edges
        // This reduces complexity from O(n·m) to O(m) per component
        for (int neighbor = 0; neighbor < instance_.num_nodes; ++neighbor) {
            if (neighbor == v) continue;
            
            int edge_idx = instance_.adjacency_matrix[v][neighbor];
            if (edge_idx != -1 && component[neighbor] == -1 && x_vals[edge_idx] > 0.5) {
                dfs_component(neighbor, comp_id, component, x_vals);
            }
        }
    }
    
    void add_lazy_constraints(const std::vector<int>& component) {
        GRBLinExpr cut_expr = 0;
        int component_size = component.size();
        
        // OPTIMIZATION: Use bool vector for O(1) membership check instead of O(n) iteration
        std::vector<bool> in_component(instance_.num_nodes, false);
        for (int v : component) {
            in_component[v] = true;
        }
        
        // OPTIMIZATION: Iterate only over edges incident to component vertices
        // Instead of checking all m edges, check only neighbors of component vertices
        // This reduces from O(n·m) to O(m) in dense graphs
        for (int u : component) {
            for (int v = 0; v < instance_.num_nodes; ++v) {
                if (u >= v) continue;  // Avoid double-counting undirected edges
                
                int edge_idx = instance_.adjacency_matrix[u][v];
                if (edge_idx != -1 && in_component[v]) {
                    // Both endpoints in component
                    cut_expr += x_vars_[edge_idx];
                }
            }
        }
        
        // Add cut: sum of edges in component <= |component| - 1
        addLazy(cut_expr <= component_size - 1);
        lazy_constraints_count_++;
    }
};

// FLOW-BASED RELAXED FORMULATION SOLVER
//=============================================================================

/**
 * @brief Flow-based relaxed formulation solver for OCST problem
 * This is the RFB formulation (Relaxation of Flow-Based)
 * Key difference: NO y variables (arborescence binary variables)
 */
class FlowBasedRelaxedSolver 
{
private:
    const OCSTInstance& instance_;
    GRBEnv env_;
    GRBModel model_;
    
    // Variables for Flow-Based Relaxed (formulation 4.36-4.43)
    std::vector<GRBVar> x_vars_;                    // x[edge] - binary (4.43)
    std::vector<std::vector<GRBVar>> f_vars_;       // f[origin][arc] - continuous (4.42)
    // NO y_vars_ in relaxed formulation!
    
    // Callback for SEC as member of solver
    std::unique_ptr<SECCallback> sec_callback_;
    
    // Edge lookup table for O(1) access
    std::vector<std::vector<int>> edge_lookup_;
    
    // Metrics tracking
    int lazy_constraints_count_;
    int cutting_planes_count_;
    
public:
    explicit FlowBasedRelaxedSolver(const OCSTInstance& instance) 
        : instance_(instance), env_(GRBEnv()), model_(GRBModel(env_)), sec_callback_(nullptr)
    {
        lazy_constraints_count_ = 0;
        cutting_planes_count_ = 0;
        
        // Initialize edge lookup table
        edge_lookup_.resize(instance_.num_nodes, std::vector<int>(instance_.num_nodes, -1));
        for (int e = 0; e < instance_.num_edges; ++e) {
            const Edge& edge = instance_.edges[e];
            edge_lookup_[edge.source][edge.destination] = e;
            edge_lookup_[edge.destination][edge.source] = e;
        }
        
        // Configure Gurobi parameters as specified in pseudocode
        env_.set(GRB_IntParam_OutputFlag, 0);  // Disable console output
        model_.set(GRB_IntParam_LazyConstraints, 1);  // Enable lazy constraints
        model_.set(GRB_IntParam_PreCrush, 1);  // Ensure lazy constraints work with presolve
        model_.set(GRB_IntParam_Presolve, 0);  // Disable presolve for better cut separation
        model_.set(GRB_IntParam_Threads, 1);   // Single thread for reproducibility
        model_.set(GRB_IntParam_Cuts, -1);     // Enable aggressive cuts to help tighten LP relaxation
    }
    
    /**
     * @brief Solves the OCST instance using flow-based relaxed formulation
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
     * @brief Sets initial solution using MST with complete warm-start
     * @param mst_edges Vector of edge indices that form the MST
     */
    void set_initial_solution(const std::vector<int>& mst_edges) 
    {
        // Set x variables to 1 for MST edges, 0 for others
        for (int e = 0; e < instance_.num_edges; ++e) {
            bool in_mst = std::find(mst_edges.begin(), mst_edges.end(), e) != mst_edges.end();
            x_vars_[e].set(GRB_DoubleAttr_Start, in_mst ? 1.0 : 0.0);
        }
        
        // Build MST adjacency with exact edge indices
        std::vector<std::vector<std::pair<int, int>>> mst_adj(instance_.num_nodes);
        std::vector<std::vector<int>> parent(instance_.num_nodes, std::vector<int>(instance_.num_nodes, -1));
        std::vector<std::vector<int>> parent_edge(instance_.num_nodes, std::vector<int>(instance_.num_nodes, -1));
        
        // Store both neighbor and exact edge index for each MST edge
        for (int e : mst_edges) {
            const Edge& edge = instance_.edges[e];
            mst_adj[edge.source].push_back({edge.destination, e});
            mst_adj[edge.destination].push_back({edge.source, e});
        }
        
        // Build parent structure using BFS from each node, storing edge indices
        for (int root = 0; root < instance_.num_nodes; ++root) {
            std::vector<bool> visited(instance_.num_nodes, false);
            std::queue<int> q;
            
            q.push(root);
            visited[root] = true;
            parent[root][root] = root;  // Root is its own parent
            parent_edge[root][root] = -1;  // Root has no parent edge
            
            while (!q.empty()) {
                int u = q.front();
                q.pop();
                
                for (const auto& neighbor_edge : mst_adj[u]) {
                    int v = neighbor_edge.first;
                    int edge_idx = neighbor_edge.second;
                    
                    if (!visited[v]) {
                        visited[v] = true;
                        parent[root][v] = u;
                        parent_edge[root][v] = edge_idx;  // Store exact edge index
                        q.push(v);
                    }
                }
            }
        }
        
        // OPTIMIZATION: Precompute demand matrix once [root][destination]
        std::vector<std::vector<double>> demand_matrix(instance_.num_nodes, 
                                                       std::vector<double>(instance_.num_nodes, 0.0));
        for (const auto& req : instance_.requirements) {
            demand_matrix[req.origin][req.destination] += req.weight;
        }
        
        // Set flow variables (f) based on MST arborescences
        // NO y variables in relaxed formulation!
        for (int root = 0; root < instance_.num_nodes; ++root) {
            // OPTIMIZATION: Build children list once per root
            std::vector<std::vector<int>> children(instance_.num_nodes);
            for (int i = 0; i < instance_.num_nodes; ++i) {
                if (i != root && parent[root][i] != -1) {
                    children[parent[root][i]].push_back(i);
                }
            }
            
            // OPTIMIZATION: Compute all subtree demands for this root in one DFS pass
            std::vector<double> subtree_demand_cache(instance_.num_nodes, -1.0);
            compute_all_subtree_demands(root, root, children, demand_matrix[root], subtree_demand_cache);
            
            // Set flows: f[root][index(parent[root][j], j)] = subtree_demand
            for (int j = 0; j < instance_.num_nodes; ++j) {
                if (j != root && parent[root][j] != -1) {
                    int parent_j = parent[root][j];
                    
                    // Use exact edge index from MST
                    int edge_idx = parent_edge[root][j];
                    if (edge_idx >= 0) {
                        // Determine correct orientation: parent_j -> j
                        const Edge& edge = instance_.edges[edge_idx];
                        int arc_index;
                        if (edge.source == parent_j && edge.destination == j) {
                            // Forward direction: parent_j -> j (2 * edge_idx)
                            arc_index = 2 * edge_idx;
                        } else if (edge.source == j && edge.destination == parent_j) {
                            // Backward direction: j -> parent_j (2 * edge_idx + 1)
                            arc_index = 2 * edge_idx + 1;
                        } else {
                            continue;  // Edge not found or invalid orientation
                        }
                        
                        // Use precomputed subtree demand from cache
                        double subtree_flow = subtree_demand_cache[j];
                        f_vars_[root][arc_index].set(GRB_DoubleAttr_Start, subtree_flow);
                    }
                }
            }
        }
    }
    
    /**
     * @brief Computes ALL subtree demands for a given root in one DFS pass
     * OPTIMIZATION: O(n) for all nodes instead of O(n²) with repeated calls
     * 
     * @param root The origin node
     * @param node Current node in DFS
     * @param children Children adjacency list (precomputed)
     * @param demand_row Demand vector from root to all destinations (precomputed)
     * @param cache Output cache of subtree demands
     */
    void compute_all_subtree_demands(int root, int node, 
                                     const std::vector<std::vector<int>>& children,
                                     const std::vector<double>& demand_row,
                                     std::vector<double>& cache) {
        if (cache[node] >= 0) return;  // Already computed
        
        double total_demand = 0.0;
        
        // Direct demand from root to this node
        total_demand += demand_row[node];
        
        // Recursive DFS: accumulate demands from all descendants
        for (int child : children[node]) {
            compute_all_subtree_demands(root, child, children, demand_row, cache);
            total_demand += cache[child];
        }
        
        cache[node] = total_demand;
    }
    
    /**
     * @brief OLD FUNCTION - kept for backward compatibility but no longer used in warm-start
     * @deprecated Use compute_all_subtree_demands for better performance
     */
    double calculate_subtree_demand_dfs(int root, int node, const std::vector<int>& parent,
                                       const std::vector<std::vector<int>>& children,
                                       std::vector<double>& subtree_demand_cache) {
        // Return cached value if already computed
        if (subtree_demand_cache[node] >= 0) {
            return subtree_demand_cache[node];
        }
        
        double total_demand = 0.0;
        
        // Add demand from root to this node
        for (const auto& req : instance_.requirements) {
            if (req.origin == root && req.destination == node) {
                total_demand += req.weight;
            }
        }
        
        // Add demands from root to all descendants (DFS recursion)
        for (int child : children[node]) {
            total_demand += calculate_subtree_demand_dfs(root, child, parent, children, subtree_demand_cache);
        }
        
        // Cache result
        subtree_demand_cache[node] = total_demand;
        return total_demand;
    }
    
    /**
     * @brief Wrapper for backward compatibility - builds children list and calls DFS version
     */
    double calculate_subtree_demand(int root, int node, const std::vector<int>& parent) {
        // Build adjacency list of children from parent array
        std::vector<std::vector<int>> children(instance_.num_nodes);
        for (int i = 0; i < instance_.num_nodes; ++i) {
            if (i != root && parent[i] != -1) {
                children[parent[i]].push_back(i);
            }
        }
        
        // Cache for memoization
        std::vector<double> subtree_demand_cache(instance_.num_nodes, -1.0);
        
        return calculate_subtree_demand_dfs(root, node, parent, children, subtree_demand_cache);
    }
    
    void create_variables() 
    {
        // Create variables x[edge] - binary (4.43)
        x_vars_.resize(instance_.num_edges);
        for (int e = 0; e < instance_.num_edges; ++e) {
            const Edge& edge = instance_.edges[e];
            std::string var_name = "x_" + std::to_string(edge.source) + "_" + std::to_string(edge.destination);
            x_vars_[e] = model_.addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name);
        }
        
        // Create variables f[origin][arc] for each origin - continuous (4.42)
        // NO y variables in relaxed formulation!
        f_vars_.resize(instance_.num_nodes);
        
        for (int o = 0; o < instance_.num_nodes; ++o) {
            // Each undirected edge becomes 2 directed arcs
            f_vars_[o].resize(instance_.num_edges * 2);
            
            for (int e = 0; e < instance_.num_edges; ++e) {
                const Edge& edge = instance_.edges[e];
                
                // Forward direction (i -> j)
                std::string f_name_fwd = "f_" + std::to_string(o) + "_" + 
                                        std::to_string(edge.source) + "_" + std::to_string(edge.destination);
                f_vars_[o][2*e] = model_.addVar(0.0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, f_name_fwd);
                
                // Backward direction (j -> i)
                std::string f_name_bwd = "f_" + std::to_string(o) + "_" + 
                                        std::to_string(edge.destination) + "_" + std::to_string(edge.source);
                f_vars_[o][2*e + 1] = model_.addVar(0.0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, f_name_bwd);
            }
        }
        
        // Update model after creating variables
        model_.update();
    }
    
    /**
     * @brief Adds structural constraints (4.37-4.38): spanning tree constraints
     */
    void add_structural_constraints() 
    {
        // Constraint (4.37): sum of edges = n-1
        GRBLinExpr tree_constraint = 0;
        for (int e = 0; e < instance_.num_edges; ++e) {
            tree_constraint += x_vars_[e];
        }
        model_.addConstr(tree_constraint == instance_.num_nodes - 1, "tree_constraint");
        
        // Constraints (4.38): subtour elimination constraints (SEC)
        // For each subset S of V, |S| >= 2, sum_{ij in E(S)} x_ij <= |S| - 1
        // Implemented using lazy constraints for efficiency
        sec_callback_ = std::make_unique<SECCallback>(instance_, x_vars_, edge_lookup_, lazy_constraints_count_, cutting_planes_count_);
        model_.setCallback(sec_callback_.get());
    }
    
    /**
     * @brief Adds flow constraints (4.39-4.40): flow conservation per origin
     */
    void add_flow_constraints() 
    {
        // Calculate total demand per origin
        std::vector<double> total_demand_from(instance_.num_nodes, 0.0);
        std::vector<std::vector<double>> demand_matrix(instance_.num_nodes, std::vector<double>(instance_.num_nodes, 0.0));
        
        for (const auto& req : instance_.requirements) {
            total_demand_from[req.origin] += req.weight;
            demand_matrix[req.origin][req.destination] += req.weight;  // Accumulate demands
        }
        
        // Constraints (4.39): flow conservation for each origin o and vertex j != o
        for (int o = 0; o < instance_.num_nodes; ++o) {
            for (int j = 0; j < instance_.num_nodes; ++j) {
                if (j == o) continue;  // Skip origin itself
                
                GRBLinExpr flow_in = 0;
                GRBLinExpr flow_out = 0;
                
                // Sum incoming flow to j from all arcs ij
                for (int e = 0; e < instance_.num_edges; ++e) {
                    const Edge& edge = instance_.edges[e];
                    
                    // Arc ij -> j (forward)
                    if (edge.destination == j) {
                        flow_in += f_vars_[o][2*e];
                    }
                    // Arc ji -> j (backward) 
                    if (edge.source == j) {
                        flow_in += f_vars_[o][2*e + 1];
                    }
                    
                    // Sum outgoing flow from j to all arcs jk
                    // Arc jk outgoing (forward)
                    if (edge.source == j) {
                        flow_out += f_vars_[o][2*e];
                    }
                    // Arc kj outgoing (backward)
                    if (edge.destination == j) {
                        flow_out += f_vars_[o][2*e + 1];
                    }
                }
                
                // Constraint: flow_in - flow_out = w_oj (demand at j from origin o)
                double demand_at_j = demand_matrix[o][j];
                std::string constr_name = "flow_conservation_" + std::to_string(o) + "_" + std::to_string(j);
                model_.addConstr(flow_in - flow_out == demand_at_j, constr_name);
            }
        }
        
        // Constraints (4.40): initial flow from each origin o
        for (int o = 0; o < instance_.num_nodes; ++o) {
            GRBLinExpr initial_flow = 0;
            
            // Sum outgoing flow from origin o
            for (int e = 0; e < instance_.num_edges; ++e) {
                const Edge& edge = instance_.edges[e];
                
                // Arc o -> k (forward)
                if (edge.source == o) {
                    initial_flow += f_vars_[o][2*e];
                }
                // Arc k -> o (backward, but outgoing from o)
                if (edge.destination == o) {
                    initial_flow += f_vars_[o][2*e + 1];
                }
            }
            
            // Constraint: initial flow = sum of demands with origin o
            std::string constr_name = "initial_flow_" + std::to_string(o);
            model_.addConstr(initial_flow == total_demand_from[o], constr_name);
        }
        
        // Critical constraint: incoming flow to origin = 0 (prevents cycles at root)
        for (int o = 0; o < instance_.num_nodes; ++o) {
            GRBLinExpr inflow_to_root = 0;
            
            // Sum incoming flow to origin o
            for (int e = 0; e < instance_.num_edges; ++e) {
                const Edge& edge = instance_.edges[e];
                
                // Arc k -> o (forward)
                if (edge.destination == o) {
                    inflow_to_root += f_vars_[o][2*e];
                }
                // Arc o -> k (backward, but incoming to o)
                if (edge.source == o) {
                    inflow_to_root += f_vars_[o][2*e + 1];
                }
            }
            
            // Constraint: incoming flow to origin = 0
            std::string constr_name = "root_inflow_zero_" + std::to_string(o);
            model_.addConstr(inflow_to_root == 0, constr_name);
        }
    }
    
    /**
     * @brief Adds coupling constraints (4.41): flow-edge coupling
     * This is the KEY difference from flow-based: direct coupling without y variables
     */
    void add_coupling_constraints() 
    {
        // Calculate sum of demands per origin for Big-M dynamic
        std::vector<double> total_demand_from(instance_.num_nodes, 0.0);
        for (const auto& req : instance_.requirements) {
            total_demand_from[req.origin] += req.weight;
        }
        
        // Constraints (4.41): f_o_ij + f_o_ji <= (sum W_o) * x_ij (flow-edge coupling)
        // CRITICAL: Apply for ALL origins o ∈ V, even when W_o = 0
        // When W_o = 0, the constraint becomes f_o_ij + f_o_ji <= 0, forcing both to 0
        for (int o = 0; o < instance_.num_nodes; ++o) {
            double sumW = total_demand_from[o];
            // NO skip: constraint applies to all origins as per formulation (4.41)
            
            for (int e = 0; e < instance_.num_edges; ++e) {
                const Edge& edge = instance_.edges[e];
                
                // One inequality: f_o_ij + f_o_ji <= sumW * x_ij
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
        
        // Objective Flow-Based Relaxed (4.36): min Σ_o Σ_ij c_ij * f_o_ij
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
        
        // CRITICAL FIX: Check SolCount before reading ObjVal to avoid exception
        // when TIME_LIMIT is reached without finding a feasible solution
        int sol_count = model_.get(GRB_IntAttr_SolCount);
        
        if (result.gurobi_status == GRB_OPTIMAL || 
            (result.gurobi_status == GRB_TIME_LIMIT && sol_count > 0)) {
            result.objective_value = model_.get(GRB_DoubleAttr_ObjVal);
            result.upper_bound = model_.get(GRB_DoubleAttr_ObjVal);
            
            // Extract selected edges
            for (int e = 0; e < instance_.num_edges; ++e) {
                if (x_vars_[e].get(GRB_DoubleAttr_X) > 0.5) {
                    result.selected_edges.push_back(e);
                }
            }
        } else if (result.gurobi_status == GRB_TIME_LIMIT && sol_count == 0) {
            // TIME_LIMIT reached without finding any feasible solution
            result.objective_value = -1.0;
            result.upper_bound = 0.0;
            std::cerr << "Warning: TIME_LIMIT reached without finding a feasible solution" << std::endl;
        }
        
        // Try to get bounds (available even without incumbent)
        if (result.gurobi_status == GRB_OPTIMAL || result.gurobi_status == GRB_TIME_LIMIT) {
            try {
                result.lower_bound = model_.get(GRB_DoubleAttr_ObjBound);
                if (result.upper_bound > 0) {
                    result.mip_gap = 100.0 * (result.upper_bound - result.lower_bound) / result.upper_bound;
                }
            } catch (GRBException& e) {
                // Bound not available
                if (sol_count > 0) {
                    result.lower_bound = result.objective_value;
                    result.mip_gap = 0.0;
                } else {
                    result.lower_bound = 0.0;
                    result.mip_gap = 100.0;
                }
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
 * @brief Main function that solves an OCST instance using flow-based relaxed formulation
 * @param input_file Path to input instance file
 * @param output_csv Path to output CSV file for results
 * @param time_limit Time limit in seconds (default: 3600)
 * @param heuristics Gurobi heuristics level (default: 0.5)
 * @return SolutionResult containing all metrics
 */
SolutionResult solve_flow_based_relaxed_instance(const std::string& input_file,
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
        
        // Solve using flow-based relaxed formulation
        FlowBasedRelaxedSolver solver(instance);
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
        std::cout << "Objective value: " << std::fixed << std::setprecision(6) << result.objective_value << std::endl;
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
    
    SolutionResult result = solve_flow_based_relaxed_instance(input_file, output_csv, time_limit, heuristics);
    
    return result.is_optimal ? 0 : 1;
}
