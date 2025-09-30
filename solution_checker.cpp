#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <set>
#include <map>
#include <queue>
#include <iomanip>
#include <algorithm>
#include <cmath>

//=============================================================================
// DATA STRUCTURES
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

struct Instance {
    int num_nodes;
    std::vector<Edge> edges;
    std::vector<Requirement> requirements;
    double probability;
    
    // Edge lookup map for fast access
    std::map<std::pair<int,int>, double> edge_costs;
    
    void add_edge(int s, int d, double cost) {
        edges.emplace_back(s, d, cost);
        edge_costs[{s, d}] = cost;
        edge_costs[{d, s}] = cost;  // Undirected graph
    }
    
    void add_requirement(int o, int d, double w) {
        requirements.emplace_back(o, d, w);
    }
    
    double get_edge_cost(int u, int v) const {
        auto it = edge_costs.find({u, v});
        return (it != edge_costs.end()) ? it->second : -1.0;  // -1 means no edge
    }
};

struct Solution {
    double claimed_objective;
    int num_nodes;
    std::vector<std::pair<int, int>> selected_edges;
};

//=============================================================================
// PARSING FUNCTIONS
//=============================================================================

Instance parse_instance(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open instance file: " + filename);
    }
    
    Instance instance;
    std::string line;
    
    // Parse header
    std::getline(file, line);
    std::istringstream iss(line);
    int m;
    iss >> instance.num_nodes >> m >> instance.probability;
    
    std::cout << "Instance: " << instance.num_nodes << " nodes, " << m << " edges, prob=" << instance.probability << std::endl;
    
    // Parse edges
    for (int i = 0; i < m; ++i) {
        std::getline(file, line);
        std::istringstream edge_iss(line);
        int u, v;
        double cost;
        edge_iss >> u >> v >> cost;
        instance.add_edge(u, v, cost);
    }
    
    // Parse requirements
    std::getline(file, line);
    int num_reqs = std::stoi(line);
    
    for (int i = 0; i < num_reqs; ++i) {
        std::getline(file, line);
        std::istringstream req_iss(line);
        int origin, dest;
        double weight;
        req_iss >> origin >> dest >> weight;
        instance.add_requirement(origin, dest, weight);
    }
    
    std::cout << "Parsed " << instance.edges.size() << " edges and " << instance.requirements.size() << " requirements" << std::endl;
    
    file.close();
    return instance;
}

Solution parse_solution(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open solution file: " + filename);
    }
    
    Solution solution;
    std::string line;
    
    // Line 1: Objective value
    std::getline(file, line);
    solution.claimed_objective = std::stod(line);
    
    // Line 2: Number of nodes
    std::getline(file, line);
    solution.num_nodes = std::stoi(line);
    
    // Remaining lines: Selected edges
    while (std::getline(file, line) && !line.empty()) {
        std::istringstream iss(line);
        int u, v;
        if (iss >> u >> v) {
            solution.selected_edges.emplace_back(u, v);
        }
    }
    
    std::cout << "Solution claims objective: " << std::fixed << std::setprecision(0) << solution.claimed_objective << std::endl;
    std::cout << "Solution has " << solution.selected_edges.size() << " edges for " << solution.num_nodes << " nodes" << std::endl;
    
    file.close();
    return solution;
}

//=============================================================================
// VALIDATION FUNCTIONS
//=============================================================================

bool is_connected_tree(const std::vector<std::pair<int, int>>& edges, int num_nodes) {
    // Check if we have exactly n-1 edges
    if (static_cast<int>(edges.size()) != num_nodes - 1) {
        std::cout << "❌ Not a tree: has " << edges.size() << " edges, expected " << (num_nodes - 1) << std::endl;
        return false;
    }
    
    // Build adjacency list
    std::vector<std::vector<int>> adj(num_nodes);
    for (const auto& edge : edges) {
        adj[edge.first].push_back(edge.second);
        adj[edge.second].push_back(edge.first);
    }
    
    // BFS/DFS to check connectivity
    std::vector<bool> visited(num_nodes, false);
    std::queue<int> q;
    q.push(0);
    visited[0] = true;
    int visited_count = 1;
    
    while (!q.empty()) {
        int node = q.front();
        q.pop();
        
        for (int neighbor : adj[node]) {
            if (!visited[neighbor]) {
                visited[neighbor] = true;
                visited_count++;
                q.push(neighbor);
            }
        }
    }
    
    if (visited_count != num_nodes) {
        std::cout << "❌ Tree is not connected: visited " << visited_count << " of " << num_nodes << " nodes" << std::endl;
        return false;
    }
    
    std::cout << "✅ Valid tree: " << edges.size() << " edges, all " << num_nodes << " nodes connected" << std::endl;
    return true;
}

std::vector<int> find_path(const std::vector<std::vector<int>>& adj, int start, int end, int num_nodes) {
    if (start == end) {
        return {start};
    }
    
    std::vector<int> parent(num_nodes, -1);
    std::vector<bool> visited(num_nodes, false);
    std::queue<int> q;
    
    q.push(start);
    visited[start] = true;
    
    while (!q.empty()) {
        int node = q.front();
        q.pop();
        
        if (node == end) {
            // Reconstruct path
            std::vector<int> path;
            int current = end;
            while (current != -1) {
                path.push_back(current);
                current = parent[current];
            }
            std::reverse(path.begin(), path.end());
            return path;
        }
        
        for (int neighbor : adj[node]) {
            if (!visited[neighbor]) {
                visited[neighbor] = true;
                parent[neighbor] = node;
                q.push(neighbor);
            }
        }
    }
    
    return {};  // No path found
}

double calculate_objective(const Instance& instance, const Solution& solution) {
    std::cout << "\n=== OBJECTIVE CALCULATION ===" << std::endl;
    
    // Build adjacency list from solution
    std::vector<std::vector<int>> adj(instance.num_nodes);
    for (const auto& edge : solution.selected_edges) {
        adj[edge.first].push_back(edge.second);
        adj[edge.second].push_back(edge.first);
    }
    
    double total_objective = 0.0;
    
    std::cout << "Requirements analysis:" << std::endl;
    
    for (size_t r = 0; r < instance.requirements.size(); ++r) {
        const Requirement& req = instance.requirements[r];
        
        // Skip artificial requirements (weight = 0)
        if (req.weight <= 0.0) {
            std::cout << "  req[" << r << "]: (" << req.origin << ", " << req.destination 
                     << ") weight=" << req.weight << " → SKIPPED (artificial)" << std::endl;
            continue;
        }
        
        // Find path in the tree
        std::vector<int> path = find_path(adj, req.origin, req.destination, instance.num_nodes);
        
        if (path.empty()) {
            std::cout << "  req[" << r << "]: (" << req.origin << ", " << req.destination 
                     << ") weight=" << req.weight << " → ❌ NO PATH FOUND!" << std::endl;
            continue;
        }
        
        // Calculate path cost
        double path_cost = 0.0;
        std::cout << "  req[" << r << "]: (" << req.origin << ", " << req.destination 
                 << ") weight=" << req.weight << std::endl;
        std::cout << "    Path: ";
        
        for (size_t i = 0; i < path.size(); ++i) {
            std::cout << path[i];
            if (i + 1 < path.size()) {
                std::cout << " → ";
                double edge_cost = instance.get_edge_cost(path[i], path[i + 1]);
                if (edge_cost < 0) {
                    std::cout << "\n❌ Edge (" << path[i] << ", " << path[i + 1] << ") not found in instance!" << std::endl;
                    return -1.0;
                }
                path_cost += edge_cost;
            }
        }
        
        double req_contribution = req.weight * path_cost;
        total_objective += req_contribution;
        
        std::cout << std::endl;
        std::cout << "    Path cost: " << std::fixed << std::setprecision(0) << path_cost << std::endl;
        std::cout << "    Contribution: " << req.weight << " × " << path_cost << " = " << req_contribution << std::endl;
    }
    
    return total_objective;
}

//=============================================================================
// MAIN CHECKER FUNCTION
//=============================================================================

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <instance_file> <solution_file>" << std::endl;
        std::cout << "Example: " << argv[0] << " data/input/test_instances/ocstpin1 data/output/test_instances/complete_ocstpin1.sol" << std::endl;
        return 1;
    }
    
    std::string instance_file = argv[1];
    std::string solution_file = argv[2];
    
    try {
        std::cout << "🔍 OCST Solution Checker" << std::endl;
        std::cout << "========================" << std::endl;
        std::cout << "Instance: " << instance_file << std::endl;
        std::cout << "Solution: " << solution_file << std::endl;
        std::cout << std::endl;
        
        // Parse files
        Instance instance = parse_instance(instance_file);
        Solution solution = parse_solution(solution_file);
        
        std::cout << std::endl;
        
        // Validate tree structure
        bool is_valid_tree = is_connected_tree(solution.selected_edges, solution.num_nodes);
        if (!is_valid_tree) {
            std::cout << "\n❌ SOLUTION REJECTED: Invalid tree structure" << std::endl;
            return 1;
        }
        
        // Calculate actual objective
        double calculated_objective = calculate_objective(instance, solution);
        
        if (calculated_objective < 0) {
            std::cout << "\n❌ SOLUTION REJECTED: Error in objective calculation" << std::endl;
            return 1;
        }
        
        // Compare results
        std::cout << "\n=== FINAL VALIDATION ===" << std::endl;
        std::cout << "Claimed objective:    " << std::fixed << std::setprecision(0) << solution.claimed_objective << std::endl;
        std::cout << "Calculated objective: " << std::fixed << std::setprecision(0) << calculated_objective << std::endl;
        
        double difference = std::abs(calculated_objective - solution.claimed_objective);
        std::cout << "Difference:           " << std::fixed << std::setprecision(0) << difference << std::endl;
        
        const double TOLERANCE = 1e-6;
        if (difference <= TOLERANCE) {
            std::cout << "\n✅ SOLUTION VALID: Objectives match!" << std::endl;
            return 0;
        } else {
            std::cout << "\n❌ SOLUTION INVALID: Objective mismatch!" << std::endl;
            std::cout << "Tolerance: " << TOLERANCE << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ ERROR: " << e.what() << std::endl;
        return 1;
    }
}
