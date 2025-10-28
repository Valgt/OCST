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
#include <set>

// Gurobi integration
#include <gurobi_c++.h>

using namespace std;

//=============================================================================
// BASIC DATA STRUCTURES
//=============================================================================

struct Edge {
    int source;
    int destination;
    double cost;
    
    Edge(int s, int d, double c) : source(s), destination(d), cost(c) {}
};

struct Requirement {
    int origin;
    int destination;
    double weight;
    
    Requirement(int o, int d, double w) : origin(o), destination(d), weight(w) {}
};

struct OCSTInstance {
    int num_nodes;
    int num_edges;
    double probability;
    std::vector<Edge> edges;
    std::vector<Requirement> requirements;
    
    // Derived data structures
    std::vector<std::vector<int>> adjacency_matrix;  // -1 if no edge, edge_index otherwise
    std::vector<std::vector<int>> adjacency_list;    // neighbors for each node
    
    OCSTInstance(int n, double prob) : num_nodes(n), probability(prob) {
        num_edges = 0;
        adjacency_matrix = std::vector<std::vector<int>>(n, std::vector<int>(n, -1));
        adjacency_list.resize(n);
    }
    
    void add_edge(int source, int dest, double cost) {
        edges.emplace_back(source, dest, cost);
        
        // Update adjacency matrix (undirected graph)
        adjacency_matrix[source][dest] = num_edges;
        adjacency_matrix[dest][source] = num_edges;
        
        // Update adjacency list
        adjacency_list[source].push_back(dest);
        adjacency_list[dest].push_back(source);
        
        num_edges++;
    }
    
    void add_requirement(int origin, int dest, double weight) {
        requirements.emplace_back(origin, dest, weight);
    }
    
    bool has_edge(int i, int j) const {
        return adjacency_matrix[i][j] != -1;
    }
    
    int get_edge_index(int i, int j) const {
        return adjacency_matrix[i][j];
    }
};

struct SolutionResult {
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
    
    std::vector<int> selected_edges;
    
    SolutionResult() {
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
// HELPER: MST CONSTRUCTION USING KRUSKAL
//=============================================================================

struct UnionFind {
    vector<int> parent, rank_uf;
    
    UnionFind(int n) : parent(n), rank_uf(n, 0) {
        for (int i = 0; i < n; ++i) {
            parent[i] = i;
        }
    }
    
    int find(int x) {
        if (parent[x] != x) {
            parent[x] = find(parent[x]);
        }
        return parent[x];
    }
    
    bool unite(int x, int y) {
        int px = find(x);
        int py = find(y);
        if (px == py) return false;
        
        if (rank_uf[px] < rank_uf[py]) {
            parent[px] = py;
        } else if (rank_uf[px] > rank_uf[py]) {
            parent[py] = px;
        } else {
            parent[py] = px;
            rank_uf[px]++;
        }
        return true;
    }
};

vector<int> compute_mst(const OCSTInstance& instance) {
    // Kruskal's algorithm
    vector<pair<double, int>> sorted_edges;
    for (int e = 0; e < instance.num_edges; ++e) {
        sorted_edges.push_back({instance.edges[e].cost, e});
    }
    sort(sorted_edges.begin(), sorted_edges.end());
    
    UnionFind uf(instance.num_nodes);
    vector<int> mst_edges;
    
    for (const auto& [cost, edge_idx] : sorted_edges) {
        const Edge& edge = instance.edges[edge_idx];
        if (uf.unite(edge.source, edge.destination)) {
            mst_edges.push_back(edge_idx);
            if (mst_edges.size() == instance.num_nodes - 1) {
                break;
            }
        }
    }
    
    return mst_edges;
}

//=============================================================================
// CONNECTIVITY CHECK
//=============================================================================

bool is_connected(const OCSTInstance& instance) {
    if (instance.num_nodes == 0) return true;
    
    vector<bool> visited(instance.num_nodes, false);
    queue<int> q;
    q.push(0);
    visited[0] = true;
    int count = 1;
    
    while (!q.empty()) {
        int u = q.front();
        q.pop();
        
        for (int v : instance.adjacency_list[u]) {
            if (!visited[v]) {
                visited[v] = true;
                q.push(v);
                count++;
            }
        }
    }
    
    return count == instance.num_nodes;
}

//=============================================================================
// ROOTED TREE SOLVER
//=============================================================================

class RootedTreeSolver {
private:
    const OCSTInstance& instance_;
    GRBEnv env_;
    GRBModel model_;
    
    int root_;  // Fixed root (default 0)
    
    // Variables
    std::vector<GRBVar> x_vars_;      // Edge selection (undirected)
    std::vector<GRBVar> a_vars_;      // Arc orientation (2*m, directed)
    std::vector<GRBVar> d_vars_;      // Depth from root
    std::vector<std::vector<GRBVar>> psi_vars_;   // Routes from root to k: [k][arc]
    std::vector<std::vector<GRBVar>> desc_vars_;  // Descendant indicators: [k][node]
    std::vector<std::vector<GRBVar>> split_vars_; // Split indicators: [pair_idx][node]
    std::vector<GRBVar> g_vars_;      // Load per arc
    
    // Demand pairs with positive weight
    std::vector<pair<int,int>> demand_pairs_;
    std::unordered_map<string, int> pair_index_;
    std::vector<double> demand_weights_;
    
public:
    RootedTreeSolver(const OCSTInstance& instance, int root = 0) 
        : instance_(instance), env_(GRBEnv()), model_(GRBModel(env_)), root_(root) {
        env_.set(GRB_IntParam_OutputFlag, 0);
        model_.set(GRB_IntParam_Threads, 1);
        model_.set(GRB_IntParam_Presolve, 0);
        model_.set(GRB_IntParam_Cuts, 0);
    }
    
    void set_time_limit(double seconds) {
        model_.set(GRB_DoubleParam_TimeLimit, seconds);
    }
    
    void set_heuristics(double level) {
        model_.set(GRB_DoubleParam_Heuristics, level);
    }
    
    SolutionResult solve() {
        auto start_time = chrono::high_resolution_clock::now();
        
        try {
            // Build demand pairs
            cout << "Building demand pairs..." << endl;
            build_demand_pairs();
            cout << "  Found " << demand_pairs_.size() << " unique demand pairs" << endl;
            
            // Create variables
            cout << "Creating variables..." << endl;
            create_variables();
            cout << "  Variables created" << endl;
            
            // Add constraints
            cout << "Adding structural constraints..." << endl;
            add_structural_constraints();
            cout << "Adding route constraints..." << endl;
            add_route_constraints();
            cout << "Adding split constraints..." << endl;
            add_split_constraints();
            cout << "Adding load constraints..." << endl;
            add_load_constraints();
            cout << "  All constraints added" << endl;
            
            // Set objective
            cout << "Setting objective..." << endl;
            set_objective();
            
            // Warm start with MST
            cout << "Computing warm start..." << endl;
            warm_start();
            
            // Optimize
            cout << "Optimizing model..." << endl;
            model_.optimize();
            
        } catch (GRBException& e) {
            cerr << "Gurobi Error: " << e.getMessage() << endl;
            cerr << "Error code: " << e.getErrorCode() << endl;
            throw;
        }
        
        auto end_time = chrono::high_resolution_clock::now();
        chrono::duration<double> elapsed = end_time - start_time;
        
        // Extract solution
        return extract_solution(elapsed.count());
    }
    
private:
    void build_demand_pairs() {
        demand_pairs_.clear();
        pair_index_.clear();
        demand_weights_.clear();
        
        for (const auto& req : instance_.requirements) {
            string key = to_string(req.origin) + "," + to_string(req.destination);
            if (pair_index_.find(key) == pair_index_.end()) {
                int idx = demand_pairs_.size();
                pair_index_[key] = idx;
                demand_pairs_.push_back({req.origin, req.destination});
                demand_weights_.push_back(req.weight);
            } else {
                demand_weights_[pair_index_[key]] += req.weight;
            }
        }
    }
    
    void create_variables() {
        // x variables: edge selection (m variables)
        x_vars_.reserve(instance_.num_edges);
        for (int e = 0; e < instance_.num_edges; ++e) {
            x_vars_.push_back(model_.addVar(0.0, 1.0, 0.0, GRB_BINARY, "x_" + to_string(e)));
        }
        
        // a variables: arc orientation (2*m variables, one per direction)
        a_vars_.reserve(2 * instance_.num_edges);
        for (int e = 0; e < instance_.num_edges; ++e) {
            const Edge& edge = instance_.edges[e];
            a_vars_.push_back(model_.addVar(0.0, 1.0, 0.0, GRB_BINARY, 
                "a_" + to_string(edge.source) + "_" + to_string(edge.destination)));
            a_vars_.push_back(model_.addVar(0.0, 1.0, 0.0, GRB_BINARY, 
                "a_" + to_string(edge.destination) + "_" + to_string(edge.source)));
        }
        
        // d variables: depth (n variables)
        d_vars_.reserve(instance_.num_nodes);
        for (int v = 0; v < instance_.num_nodes; ++v) {
            d_vars_.push_back(model_.addVar(0.0, instance_.num_nodes - 1, 0.0, GRB_INTEGER, "d_" + to_string(v)));
        }
        
        // psi variables: routes [k][arc] for k != root
        psi_vars_.resize(instance_.num_nodes);
        for (int k = 0; k < instance_.num_nodes; ++k) {
            if (k == root_) continue;
            psi_vars_[k].reserve(2 * instance_.num_edges);
            for (int e = 0; e < instance_.num_edges; ++e) {
                const Edge& edge = instance_.edges[e];
                psi_vars_[k].push_back(model_.addVar(0.0, 1.0, 0.0, GRB_BINARY, 
                    "psi_" + to_string(k) + "_" + to_string(edge.source) + "_" + to_string(edge.destination)));
                psi_vars_[k].push_back(model_.addVar(0.0, 1.0, 0.0, GRB_BINARY, 
                    "psi_" + to_string(k) + "_" + to_string(edge.destination) + "_" + to_string(edge.source)));
            }
        }
        
        // desc variables: descendant [k][node] for ALL k (including root for split constraints)
        desc_vars_.resize(instance_.num_nodes);
        for (int k = 0; k < instance_.num_nodes; ++k) {
            desc_vars_[k].reserve(instance_.num_nodes);
            for (int v = 0; v < instance_.num_nodes; ++v) {
                desc_vars_[k].push_back(model_.addVar(0.0, 1.0, 0.0, GRB_BINARY, 
                    "desc_" + to_string(k) + "_" + to_string(v)));
            }
        }
        
        // split variables: [pair_idx][node] for each demand pair and each node != root
        split_vars_.resize(demand_pairs_.size());
        for (size_t p = 0; p < demand_pairs_.size(); ++p) {
            split_vars_[p].reserve(instance_.num_nodes);
            for (int v = 0; v < instance_.num_nodes; ++v) {
                if (v == root_) {
                    split_vars_[p].push_back(GRBVar());  // Dummy for root
                } else {
                    split_vars_[p].push_back(model_.addVar(0.0, 1.0, 0.0, GRB_BINARY, 
                        "split_" + to_string(p) + "_" + to_string(v)));
                }
            }
        }
        
        // g variables: load per arc (2*m variables)
        g_vars_.reserve(2 * instance_.num_edges);
        for (int e = 0; e < instance_.num_edges; ++e) {
            const Edge& edge = instance_.edges[e];
            g_vars_.push_back(model_.addVar(0.0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, 
                "g_" + to_string(edge.source) + "_" + to_string(edge.destination)));
            g_vars_.push_back(model_.addVar(0.0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, 
                "g_" + to_string(edge.destination) + "_" + to_string(edge.source)));
        }
        
        model_.update();
    }
    
    void add_structural_constraints() {
        // Constraint: sum of x = n-1 (tree property)
        GRBLinExpr tree_constraint = 0;
        for (int e = 0; e < instance_.num_edges; ++e) {
            tree_constraint += x_vars_[e];
        }
        model_.addConstr(tree_constraint == instance_.num_nodes - 1, "tree_edges");
        
        // Constraint: a_uv + a_vu = x_e (orientation consistency)
        for (int e = 0; e < instance_.num_edges; ++e) {
            model_.addConstr(a_vars_[2*e] + a_vars_[2*e+1] == x_vars_[e], 
                "orient_" + to_string(e));
        }
        
        // Constraint: unique parent for each non-root node
        for (int v = 0; v < instance_.num_nodes; ++v) {
            if (v == root_) continue;
            
            GRBLinExpr parent_sum = 0;
            for (int u : instance_.adjacency_list[v]) {
                int edge_idx = instance_.get_edge_index(u, v);
                int arc_idx = 2 * edge_idx;
                if (instance_.edges[edge_idx].source == u && instance_.edges[edge_idx].destination == v) {
                    parent_sum += a_vars_[arc_idx];
                } else {
                    parent_sum += a_vars_[arc_idx + 1];
                }
            }
            model_.addConstr(parent_sum == 1, "parent_" + to_string(v));
        }
        
        // Constraint: d[root] = 0
        model_.addConstr(d_vars_[root_] == 0, "root_depth");
        
        // Constraint: depth ordering d[v] >= d[u] + 1 - M*(1 - a_uv)
        double M = instance_.num_nodes;
        for (int e = 0; e < instance_.num_edges; ++e) {
            const Edge& edge = instance_.edges[e];
            int u = edge.source;
            int v = edge.destination;
            
            // Forward: u -> v
            model_.addConstr(d_vars_[v] >= d_vars_[u] + 1 - M * (1 - a_vars_[2*e]), 
                "depth_" + to_string(u) + "_" + to_string(v));
            
            // Backward: v -> u
            model_.addConstr(d_vars_[u] >= d_vars_[v] + 1 - M * (1 - a_vars_[2*e+1]), 
                "depth_" + to_string(v) + "_" + to_string(u));
        }
    }
    
    void add_route_constraints() {
        // For root: only root itself is in its "subtree"
        // desc[root][root] = 1, desc[root][v] = 0 for v != root
        for (int v = 0; v < instance_.num_nodes; ++v) {
            if (v == root_) {
                model_.addConstr(desc_vars_[root_][v] == 1, 
                    "desc_root_self");
            } else {
                model_.addConstr(desc_vars_[root_][v] == 0, 
                    "desc_root_other_" + to_string(v));
            }
        }
        
        // For each destination k != root, create flow from root to k
        for (int k = 0; k < instance_.num_nodes; ++k) {
            if (k == root_) continue;
            
            // Flow conservation for each node v
            // Standard convention: outflow - inflow = +1 at root, = -1 at dest, = 0 elsewhere
            for (int v = 0; v < instance_.num_nodes; ++v) {
                GRBLinExpr inflow = 0;
                GRBLinExpr outflow = 0;
                
                for (int u : instance_.adjacency_list[v]) {
                    int edge_idx = instance_.get_edge_index(u, v);
                    int arc_idx = 2 * edge_idx;
                    
                    const Edge& edge = instance_.edges[edge_idx];
                    if (edge.source == u && edge.destination == v) {
                        inflow += psi_vars_[k][arc_idx];
                        outflow += psi_vars_[k][arc_idx + 1];
                    } else {
                        inflow += psi_vars_[k][arc_idx + 1];
                        outflow += psi_vars_[k][arc_idx];
                    }
                }
                
                // Root is source (+1), destination k is sink (-1), others are 0
                double demand = (v == root_) ? 1.0 : ((v == k) ? -1.0 : 0.0);
                model_.addConstr(outflow - inflow == demand, 
                    "flow_" + to_string(k) + "_" + to_string(v));
            }
            
            // Constraint: psi can only use active arcs
            for (int e = 0; e < instance_.num_edges; ++e) {
                model_.addConstr(psi_vars_[k][2*e] <= a_vars_[2*e], 
                    "psi_arc_" + to_string(k) + "_" + to_string(e) + "_fwd");
                model_.addConstr(psi_vars_[k][2*e+1] <= a_vars_[2*e+1], 
                    "psi_arc_" + to_string(k) + "_" + to_string(e) + "_bwd");
            }
            
            // Constraint: desc[k][v] = sum of psi[k] entering v (for v != root)
            for (int v = 0; v < instance_.num_nodes; ++v) {
                if (v == root_) {
                    model_.addConstr(desc_vars_[k][v] == 1, 
                        "desc_root_" + to_string(k));
                } else {
                    GRBLinExpr inflow = 0;
                    for (int u : instance_.adjacency_list[v]) {
                        int edge_idx = instance_.get_edge_index(u, v);
                        int arc_idx = 2 * edge_idx;
                        
                        const Edge& edge = instance_.edges[edge_idx];
                        if (edge.source == u && edge.destination == v) {
                            inflow += psi_vars_[k][arc_idx];
                        } else {
                            inflow += psi_vars_[k][arc_idx + 1];
                        }
                    }
                    model_.addConstr(desc_vars_[k][v] == inflow, 
                        "desc_" + to_string(k) + "_" + to_string(v));
                }
            }
        }
    }
    
    void add_split_constraints() {
        // For each demand pair (s,t) and each node v != root
        for (size_t p = 0; p < demand_pairs_.size(); ++p) {
            int s = demand_pairs_[p].first;
            int t = demand_pairs_[p].second;
            
            for (int v = 0; v < instance_.num_nodes; ++v) {
                if (v == root_) continue;
                
                // Linearization of split[p][v] = XOR(desc[s][v], desc[t][v])
                // split = 1 iff exactly one of desc[s][v] or desc[t][v] is 1
                
                // split >= desc[s][v] - desc[t][v]
                model_.addConstr(split_vars_[p][v] >= desc_vars_[s][v] - desc_vars_[t][v], 
                    "split1_" + to_string(p) + "_" + to_string(v));
                
                // split >= desc[t][v] - desc[s][v]
                model_.addConstr(split_vars_[p][v] >= desc_vars_[t][v] - desc_vars_[s][v], 
                    "split2_" + to_string(p) + "_" + to_string(v));
                
                // split <= desc[s][v] + desc[t][v]
                model_.addConstr(split_vars_[p][v] <= desc_vars_[s][v] + desc_vars_[t][v], 
                    "split3_" + to_string(p) + "_" + to_string(v));
                
                // split <= 2 - (desc[s][v] + desc[t][v])
                model_.addConstr(split_vars_[p][v] <= 2 - (desc_vars_[s][v] + desc_vars_[t][v]), 
                    "split4_" + to_string(p) + "_" + to_string(v));
                
                // split <= a[parent(v) -> v]
                // We need to link split to the incoming arc
                GRBLinExpr parent_arc = 0;
                for (int u : instance_.adjacency_list[v]) {
                    int edge_idx = instance_.get_edge_index(u, v);
                    int arc_idx = 2 * edge_idx;
                    
                    const Edge& edge = instance_.edges[edge_idx];
                    if (edge.source == u && edge.destination == v) {
                        parent_arc += a_vars_[arc_idx];
                    } else {
                        parent_arc += a_vars_[arc_idx + 1];
                    }
                }
                model_.addConstr(split_vars_[p][v] <= parent_arc, 
                    "split5_" + to_string(p) + "_" + to_string(v));
            }
        }
    }
    
    void add_load_constraints() {
        // Compute Big-M as total demand
        double M = 0.0;
        for (double w : demand_weights_) {
            M += w;
        }
        
        // For each arc (u,v), couple load with orientation using Big-M
        for (int e = 0; e < instance_.num_edges; ++e) {
            const Edge& edge = instance_.edges[e];
            int u = edge.source;
            int v = edge.destination;
            
            // Load for arc u -> v (demand crossing when u is parent of v)
            GRBLinExpr load_uv = 0;
            if (v != root_) {
                for (size_t p = 0; p < demand_pairs_.size(); ++p) {
                    load_uv += demand_weights_[p] * split_vars_[p][v];
                }
            }
            
            // Big-M constraints: g_uv is active only when a_uv = 1
            // g_uv <= M * a_uv
            model_.addConstr(g_vars_[2*e] <= M * a_vars_[2*e], 
                "g_upper_active_" + to_string(u) + "_" + to_string(v));
            
            // g_uv >= load_uv - M * (1 - a_uv)
            model_.addConstr(g_vars_[2*e] >= load_uv - M * (1 - a_vars_[2*e]), 
                "g_lower_when_active_" + to_string(u) + "_" + to_string(v));
            
            // g_uv <= load_uv + M * (1 - a_uv)
            model_.addConstr(g_vars_[2*e] <= load_uv + M * (1 - a_vars_[2*e]), 
                "g_upper_when_active_" + to_string(u) + "_" + to_string(v));
            
            // Load for arc v -> u (demand crossing when v is parent of u)
            GRBLinExpr load_vu = 0;
            if (u != root_) {
                for (size_t p = 0; p < demand_pairs_.size(); ++p) {
                    load_vu += demand_weights_[p] * split_vars_[p][u];
                }
            }
            
            // Big-M constraints: g_vu is active only when a_vu = 1
            // g_vu <= M * a_vu
            model_.addConstr(g_vars_[2*e+1] <= M * a_vars_[2*e+1], 
                "g_upper_active_" + to_string(v) + "_" + to_string(u));
            
            // g_vu >= load_vu - M * (1 - a_vu)
            model_.addConstr(g_vars_[2*e+1] >= load_vu - M * (1 - a_vars_[2*e+1]), 
                "g_lower_when_active_" + to_string(v) + "_" + to_string(u));
            
            // g_vu <= load_vu + M * (1 - a_vu)
            model_.addConstr(g_vars_[2*e+1] <= load_vu + M * (1 - a_vars_[2*e+1]), 
                "g_upper_when_active_" + to_string(v) + "_" + to_string(u));
        }
    }
    
    void set_objective() {
        // Objective: sum of cost * g for all directed arcs
        // Only one orientation per edge will be active (parent->child)
        GRBLinExpr obj = 0;
        for (int e = 0; e < instance_.num_edges; ++e) {
            double cost = instance_.edges[e].cost;
            // Each edge contributes only once (the active orientation)
            obj += cost * g_vars_[2*e];      // u -> v
            obj += cost * g_vars_[2*e+1];    // v -> u
        }
        model_.setObjective(obj, GRB_MINIMIZE);
    }
    
    void warm_start() {
        // Compute MST
        vector<int> mst_edges = compute_mst(instance_);
        
        // Build MST adjacency
        vector<vector<int>> mst_adj(instance_.num_nodes);
        for (int e : mst_edges) {
            const Edge& edge = instance_.edges[e];
            mst_adj[edge.source].push_back(edge.destination);
            mst_adj[edge.destination].push_back(edge.source);
        }
        
        // Orient MST from root using BFS
        vector<int> parent(instance_.num_nodes, -1);
        vector<int> depth(instance_.num_nodes, 0);
        queue<int> q;
        q.push(root_);
        parent[root_] = root_;
        
        while (!q.empty()) {
            int u = q.front();
            q.pop();
            
            for (int v : mst_adj[u]) {
                if (parent[v] == -1) {
                    parent[v] = u;
                    depth[v] = depth[u] + 1;
                    q.push(v);
                }
            }
        }
        
        // Set x, a, d variables
        for (int e = 0; e < instance_.num_edges; ++e) {
            bool in_mst = find(mst_edges.begin(), mst_edges.end(), e) != mst_edges.end();
            x_vars_[e].set(GRB_DoubleAttr_Start, in_mst ? 1.0 : 0.0);
            
            if (in_mst) {
                const Edge& edge = instance_.edges[e];
                int u = edge.source;
                int v = edge.destination;
                
                if (parent[v] == u) {
                    a_vars_[2*e].set(GRB_DoubleAttr_Start, 1.0);
                    a_vars_[2*e+1].set(GRB_DoubleAttr_Start, 0.0);
                } else {
                    a_vars_[2*e].set(GRB_DoubleAttr_Start, 0.0);
                    a_vars_[2*e+1].set(GRB_DoubleAttr_Start, 1.0);
                }
            } else {
                a_vars_[2*e].set(GRB_DoubleAttr_Start, 0.0);
                a_vars_[2*e+1].set(GRB_DoubleAttr_Start, 0.0);
            }
        }
        
        for (int v = 0; v < instance_.num_nodes; ++v) {
            d_vars_[v].set(GRB_DoubleAttr_Start, depth[v]);
        }
        
        // Set desc variables (simplified - mark paths in MST)
        // For root: only root itself is in its subtree
        for (int v = 0; v < instance_.num_nodes; ++v) {
            desc_vars_[root_][v].set(GRB_DoubleAttr_Start, (v == root_) ? 1.0 : 0.0);
        }
        
        // For other nodes k: mark path from root to k
        for (int k = 0; k < instance_.num_nodes; ++k) {
            if (k == root_) continue;
            
            // Mark path from root to k
            set<int> path_nodes;
            int curr = k;
            while (curr != root_) {
                path_nodes.insert(curr);
                curr = parent[curr];
            }
            path_nodes.insert(root_);
            
            for (int v = 0; v < instance_.num_nodes; ++v) {
                desc_vars_[k][v].set(GRB_DoubleAttr_Start, 
                    path_nodes.count(v) ? 1.0 : 0.0);
            }
        }
        
        model_.update();
    }
    
    SolutionResult extract_solution(double runtime) {
        SolutionResult result;
        result.runtime_seconds = runtime;
        result.gurobi_status = model_.get(GRB_IntAttr_Status);
        result.num_nodes_explored = static_cast<long long>(model_.get(GRB_DoubleAttr_NodeCount));
        
        if (model_.get(GRB_IntAttr_SolCount) > 0) {
            result.objective_value = model_.get(GRB_DoubleAttr_ObjVal);
            result.upper_bound = model_.get(GRB_DoubleAttr_ObjVal);
            result.lower_bound = model_.get(GRB_DoubleAttr_ObjBound);
            result.mip_gap = model_.get(GRB_DoubleAttr_MIPGap) * 100.0;
            result.is_optimal = (result.gurobi_status == GRB_OPTIMAL);
            
            // Extract selected edges
            for (int e = 0; e < instance_.num_edges; ++e) {
                if (x_vars_[e].get(GRB_DoubleAttr_X) > 0.5) {
                    result.selected_edges.push_back(e);
                }
            }
        } else {
            result.objective_value = -1.0;
            result.upper_bound = 0.0;
            result.lower_bound = model_.get(GRB_DoubleAttr_ObjBound);
        }
        
        return result;
    }
};

//=============================================================================
// INPUT PARSING
//=============================================================================

OCSTInstance parse_instance(istream& input) {
    int n, m;
    double prob;
    input >> n >> m >> prob;
    
    OCSTInstance instance(n, prob);
    
    for (int i = 0; i < m; ++i) {
        int u, v;
        double cost;
        input >> u >> v >> cost;
        instance.add_edge(u, v, cost);
    }
    
    int num_requirements;
    input >> num_requirements;
    
    for (int i = 0; i < num_requirements; ++i) {
        int origin, dest;
        double weight;
        input >> origin >> dest >> weight;
        instance.add_requirement(origin, dest, weight);
    }
    
    return instance;
}

//=============================================================================
// CSV OUTPUT
//=============================================================================

void write_csv_result(const string& filename, const OCSTInstance& instance, 
                     const SolutionResult& result, double time_limit, double heuristics) {
    ofstream out(filename);
    if (!out.is_open()) {
        cerr << "Error: Cannot open output file " << filename << endl;
        return;
    }
    
    // Write header
    out << "instance_name,instance_group,algorithm_name,algorithm_description,";
    out << "num_nodes,num_edges,num_requirements,probability,";
    out << "optimal_known,best_found,is_optimal,runtime_seconds,";
    out << "nodes_explored,lazy_constraints,cutting_planes,";
    out << "mip_gap_percent,gurobi_status,time_limit,heuristics_level,algorithm_version\n";
    
    // Extract instance name from some context (simplified)
    string instance_name = "unknown";
    string instance_group = "unknown";
    
    // Write data
    out << instance_name << ","
        << instance_group << ","
        << "rooted_tree_based_formulation,"
        << "Rooted Tree-Based MILP Formulation,"
        << instance.num_nodes << ","
        << instance.num_edges << ","
        << instance.requirements.size() << ","
        << fixed << setprecision(1) << instance.probability << ","
        << fixed << setprecision(0) << result.objective_value << ","
        << fixed << setprecision(0) << result.objective_value << ","
        << (result.is_optimal ? "True" : "False") << ","
        << fixed << setprecision(6) << result.runtime_seconds << ","
        << result.num_nodes_explored << ","
        << result.lazy_constraints_added << ","
        << result.cutting_planes_added << ","
        << fixed << setprecision(4) << result.mip_gap << ","
        << result.gurobi_status << ","
        << fixed << setprecision(1) << time_limit << ","
        << fixed << setprecision(1) << heuristics << ","
        << "1.0\n";
    
    out.close();
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <input_file> [output_csv] [time_limit] [heuristics]" << endl;
        return 1;
    }
    
    string input_file = argv[1];
    string output_file = (argc >= 3) ? argv[2] : "result.csv";
    double time_limit = (argc >= 4) ? stod(argv[3]) : 3600.0;
    double heuristics = (argc >= 5) ? stod(argv[4]) : 0.05;
    
    // Parse instance
    ifstream input(input_file);
    if (!input.is_open()) {
        cerr << "Error: Cannot open input file " << input_file << endl;
        return 1;
    }
    
    OCSTInstance instance = parse_instance(input);
    input.close();
    
    cout << "Instance: " << instance.num_nodes << " nodes, " 
         << instance.num_edges << " edges, "
         << instance.requirements.size() << " requirements" << endl;
    
    // Check connectivity
    if (!is_connected(instance)) {
        cerr << "Error: Graph is not connected" << endl;
        SolutionResult failure_result;
        failure_result.objective_value = -1.0;
        write_csv_result(output_file, instance, failure_result, time_limit, heuristics);
        return 1;
    }
    
    // Solve
    RootedTreeSolver solver(instance, 0);
    solver.set_time_limit(time_limit);
    solver.set_heuristics(heuristics);
    
    cout << "Solving with Rooted Tree-Based formulation..." << endl;
    SolutionResult result = solver.solve();
    
    // Report
    string status_str;
    if (result.gurobi_status == GRB_OPTIMAL) {
        status_str = "OPTIMAL";
    } else if (result.gurobi_status == GRB_TIME_LIMIT) {
        status_str = result.objective_value > 0 ? "TIME_LIMIT (feasible)" : "TIME_LIMIT (no solution)";
    } else if (result.gurobi_status == GRB_INFEASIBLE) {
        status_str = "INFEASIBLE";
    } else {
        status_str = "OTHER (" + to_string(result.gurobi_status) + ")";
    }
    
    cout << "Status: " << status_str << endl;
    cout << "Objective: " << fixed << setprecision(2) << result.objective_value << endl;
    cout << "Runtime: " << fixed << setprecision(3) << result.runtime_seconds << " seconds" << endl;
    cout << "Nodes explored: " << result.num_nodes_explored << endl;
    cout << "MIP Gap: " << fixed << setprecision(4) << result.mip_gap << "%" << endl;
    
    // Write CSV
    write_csv_result(output_file, instance, result, time_limit, heuristics);
    cout << "Results written to " << output_file << endl;
    
    return 0;
}
