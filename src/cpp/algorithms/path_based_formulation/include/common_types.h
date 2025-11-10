#ifndef OCST_PATH_BASED_COMMON_TYPES_H
#define OCST_PATH_BASED_COMMON_TYPES_H

#include <vector>
#include <string>
#include <unordered_map>
#include <cmath>
#include <stdexcept>

/**
 * @file common_types.h
 * @brief Consolidated data structures for OCST instances
 * 
 * This header consolidates Edge, Requirement, and OCSTInstance definitions
 * as part of Phase 1.5 standardization. These structures will eventually
 * be promoted to src/cpp/common/ once all formulations adopt them.
 */

namespace ocst {
namespace path_based {

/**
 * @brief Represents a graph edge with source, destination and cost
 */
struct Edge {
    int source;
    int destination;
    double cost;
    
    Edge(int s, int d, double c) : source(s), destination(d), cost(c) {}
    
    // Equality operator for comparisons
    bool operator==(const Edge& other) const {
        return source == other.source && 
               destination == other.destination && 
               std::abs(cost - other.cost) < 1e-9;
    }
};

/**
 * @brief Represents a demand requirement between origin and destination
 */
struct Requirement {
    int origin;
    int destination;
    double weight;  // Also called "demand" in some contexts
    
    Requirement(int o, int d, double w) : origin(o), destination(d), weight(w) {}
    
    // Equality operator for comparisons
    bool operator==(const Requirement& other) const {
        return origin == other.origin && 
               destination == other.destination && 
               std::abs(weight - other.weight) < 1e-9;
    }
};

/**
 * @brief OCST problem instance data
 * 
 * Stores graph structure (nodes, edges, costs) and communication requirements.
 * Maintains derived data structures (adjacency matrix, edge index map) for
 * efficient access during optimization.
 * 
 * Graph and requirements are immutable once constructed.
 */
struct OCSTInstance {
    int num_nodes;
    int num_edges;
    double probability;  // Graph density parameter (legacy field)
    std::vector<Edge> edges;
    std::vector<Requirement> requirements;
    
    // Derived data structures for optimization
    // adjacency_matrix[i][j] = edge index if edge exists, -1 otherwise
    std::vector<std::vector<int>> adjacency_matrix;
    
    // edge_index_map["i,j"] -> edge index (for bidirectional lookup)
    std::unordered_map<std::string, int> edge_index_map;
    
    /**
     * @brief Constructor
     * @param n Number of nodes
     * @param prob Probability/density parameter (legacy, default 0.0)
     */
    explicit OCSTInstance(int n, double prob = 0.0) 
        : num_nodes(n), num_edges(0), probability(prob) {
        adjacency_matrix = std::vector<std::vector<int>>(n, std::vector<int>(n, -1));
    }
    
    /**
     * @brief Add an edge to the instance
     * Updates adjacency matrix and edge index map
     */
    void add_edge(int source, int dest, double cost) {
        if (source < 0 || source >= num_nodes || dest < 0 || dest >= num_nodes) {
            throw std::out_of_range("Edge node index out of bounds");
        }
        
        edges.emplace_back(source, dest, cost);
        
        // Update adjacency matrix (undirected graph)
        adjacency_matrix[source][dest] = num_edges;
        adjacency_matrix[dest][source] = num_edges;
        
        // Update edge index map
        std::string key1 = std::to_string(source) + "," + std::to_string(dest);
        std::string key2 = std::to_string(dest) + "," + std::to_string(source);
        edge_index_map[key1] = num_edges;
        edge_index_map[key2] = num_edges;
        
        num_edges++;
    }
    
    /**
     * @brief Add a communication requirement
     */
    void add_requirement(int origin, int dest, double weight) {
        if (origin < 0 || origin >= num_nodes || dest < 0 || dest >= num_nodes) {
            throw std::out_of_range("Requirement node index out of bounds");
        }
        requirements.emplace_back(origin, dest, weight);
    }
    
    /**
     * @brief Check if edge (i,j) exists
     */
    bool has_edge(int i, int j) const {
        if (i < 0 || i >= num_nodes || j < 0 || j >= num_nodes) return false;
        return adjacency_matrix[i][j] != -1;
    }
    
    /**
     * @brief Get edge index for edge (i,j), or -1 if not found
     */
    int get_edge_index(int i, int j) const {
        if (i < 0 || i >= num_nodes || j < 0 || j >= num_nodes) return -1;
        return adjacency_matrix[i][j];
    }
    
    /**
     * @brief Validate instance consistency
     * @return Pair (is_valid, error_message)
     */
    std::pair<bool, std::string> validate() const {
        if (num_nodes <= 0) {
            return {false, "num_nodes must be positive"};
        }
        if (num_edges != static_cast<int>(edges.size())) {
            return {false, "num_edges mismatch: expected " + std::to_string(edges.size()) + 
                    ", got " + std::to_string(num_edges)};
        }
        
        // Check edge indices are within bounds
        for (size_t i = 0; i < edges.size(); ++i) {
            const auto& edge = edges[i];
            if (edge.source < 0 || edge.source >= num_nodes ||
                edge.destination < 0 || edge.destination >= num_nodes) {
                return {false, "Edge " + std::to_string(i) + " has invalid node indices"};
            }
        }
        
        // Check requirement indices are within bounds
        for (size_t i = 0; i < requirements.size(); ++i) {
            const auto& req = requirements[i];
            if (req.origin < 0 || req.origin >= num_nodes ||
                req.destination < 0 || req.destination >= num_nodes) {
                return {false, "Requirement " + std::to_string(i) + " has invalid node indices"};
            }
        }
        
        // Check adjacency matrix consistency
        for (size_t i = 0; i < edges.size(); ++i) {
            const auto& edge = edges[i];
            int expected_idx = static_cast<int>(i);
            if (adjacency_matrix[edge.source][edge.destination] != expected_idx) {
                return {false, "Adjacency matrix inconsistent for edge " + std::to_string(i)};
            }
        }
        
        return {true, ""};
    }
};

} // namespace path_based
} // namespace ocst

#endif // OCST_PATH_BASED_COMMON_TYPES_H

