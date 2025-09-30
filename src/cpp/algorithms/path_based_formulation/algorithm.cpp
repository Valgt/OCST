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

// Gurobi integration
#include <gurobi_c++.h>

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
    
    /**
     * @brief Adds artificial connectivity requirements as per path-based formulation
     * Ensures connectivity by adding (root, v) requirements with weight 0
     */
    void add_artificial_connectivity_requirements(int root_node = 0) 
    {
        std::set<std::pair<int, int>> existing_reqs;
        
        // Track existing requirements to avoid duplicates
        for (const auto& req : requirements) {
            existing_reqs.insert({req.origin, req.destination});
            existing_reqs.insert({req.destination, req.origin}); // Undirected
        }
        
        // Add (root, v) requirements with weight 0 if they don't exist
        for (int v = 0; v < num_nodes; ++v) {
            if (v != root_node) {
                std::pair<int, int> req_pair = {root_node, v};
                if (existing_reqs.find(req_pair) == existing_reqs.end()) {
                    add_requirement(root_node, v, 0.0);
                    std::cout << "Added artificial requirement: (" << root_node << ", " << v << ") with weight 0" << std::endl;
                }
            }
        }
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
    
public:
    explicit PathBasedSolver(const OCSTInstance& instance) 
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
            
            // Add constraints
            add_structural_constraints();
            add_flow_constraints();
            add_coupling_constraints();
            
            // Set objective function
            set_objective();
            
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
                
                // Forward direction (i -> j)
                std::string var_name_fwd = "y_" + std::to_string(r) + "_" + 
                                          std::to_string(edge.source) + "_" + std::to_string(edge.destination);
                y_vars_[r][2*e] = model_.addVar(0.0, 1.0, 0.0, GRB_CONTINUOUS, var_name_fwd);
                
                // Backward direction (j -> i)
                std::string var_name_bwd = "y_" + std::to_string(r) + "_" + 
                                          std::to_string(edge.destination) + "_" + std::to_string(edge.source);
                y_vars_[r][2*e + 1] = model_.addVar(0.0, 1.0, 0.0, GRB_CONTINUOUS, var_name_bwd);
            }
        }
    }
    
    /**
     * @brief Adds structural constraints: sum of edges = n-1 (spanning tree)
     */
    void add_structural_constraints() 
    {
        GRBLinExpr tree_constraint = 0;
        for (int e = 0; e < instance_.num_edges; ++e) {
            tree_constraint += x_vars_[e];
        }
        model_.addConstr(tree_constraint == instance_.num_nodes - 1, "tree_constraint");
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
     */
    void add_coupling_constraints() 
    {
        for (int e = 0; e < instance_.num_edges; ++e) {
            const Edge& edge = instance_.edges[e];
            
            for (int r = 0; r < static_cast<int>(instance_.requirements.size()); ++r) {
                // Forward flow constraint: y_r_ij <= x_ij
                std::string constraint_name_fwd = "coupling_" + std::to_string(r) + "_" + 
                                                 std::to_string(edge.source) + "_" + std::to_string(edge.destination);
                model_.addConstr(y_vars_[r][2*e] <= x_vars_[e], constraint_name_fwd);
                
                // Backward flow constraint: y_r_ji <= x_ij  
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
        
        for (int r = 0; r < static_cast<int>(instance_.requirements.size()); ++r) {
            const Requirement& req = instance_.requirements[r];
            
            // Skip artificial requirements (weight = 0)
            if (req.weight <= 0.0) {
                continue;
            }
            
            for (int e = 0; e < instance_.num_edges; ++e) {
                const Edge& edge = instance_.edges[e];
                
                // Add cost for both directions of flow
                objective += req.weight * edge.cost * y_vars_[r][2*e];     // Forward direction
                objective += req.weight * edge.cost * y_vars_[r][2*e + 1]; // Backward direction
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
    
    // Add artificial connectivity requirements as per path-based formulation
    std::cout << "Adding artificial connectivity requirements..." << std::endl;
    instance.add_artificial_connectivity_requirements(0);  // Use node 0 as root
    
    return instance;
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
        std::cout << "Status: " << (result.is_optimal ? "OPTIMAL" : "NON-OPTIMAL") << std::endl;
        std::cout << "Objective value: " << result.objective_value << std::endl;
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
                    << result.objective_value << "," << result.runtime_seconds << ","
                    << result.mip_gap << "," << result.gurobi_status << "," << result.num_nodes_explored << "\n";
            csv_file.close();
            std::cout << "Results saved to: " << output_csv << std::endl;
        }
        
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