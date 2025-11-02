#ifndef OCST_COMMON_RESULT_PAYLOAD_H
#define OCST_COMMON_RESULT_PAYLOAD_H

#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace ocst {
namespace common {

/**
 * @file result_payload.h
 * @brief Result payload structure for OCST solver outputs
 * 
 * This structure captures all metadata and results from a solver run.
 * It is serialized to JSON using nlohmann::json.
 */

/**
 * @brief Optimization status codes
 */
enum class OptimizationStatus {
    OPTIMAL,        // Solution found and proven optimal
    TIME_LIMIT,     // Time limit reached (may have feasible solution)
    INFEASIBLE,     // Problem is infeasible
    UNBOUNDED,      // Problem is unbounded
    SUBOPTIMAL,     // Feasible solution but not proven optimal
    INTERRUPTED,    // Interrupted by user/system
    ERROR           // Error during optimization
};

/**
 * @brief Convert OptimizationStatus to string
 */
inline std::string optimization_status_to_string(OptimizationStatus status) {
    switch (status) {
        case OptimizationStatus::OPTIMAL: return "OPTIMAL";
        case OptimizationStatus::TIME_LIMIT: return "TIME_LIMIT";
        case OptimizationStatus::INFEASIBLE: return "INFEASIBLE";
        case OptimizationStatus::UNBOUNDED: return "UNBOUNDED";
        case OptimizationStatus::SUBOPTIMAL: return "SUBOPTIMAL";
        case OptimizationStatus::INTERRUPTED: return "INTERRUPTED";
        case OptimizationStatus::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Runtime statistics
 */
struct RuntimeStats {
    double wall_clock_seconds = 0.0;
    double cpu_seconds = 0.0;
    int solver_nodes = 0;
    int solver_iterations = 0;
    std::string termination_reason;
};

/**
 * @brief Tree edge (for solution representation)
 */
struct TreeEdge {
    int source;
    int destination;
    
    TreeEdge(int s, int d) : source(s), destination(d) {}
};

/**
 * @brief Solution tree structure
 */
struct Solution {
    std::vector<TreeEdge> tree_edges;
    double tree_cost = 0.0;
    bool is_spanning_tree = false;
    bool is_connected = false;
};

/**
 * @brief Reproducibility metadata
 */
struct Reproducibility {
    std::string git_commit;
    bool git_dirty = false;
    int seed = 0;
    std::string timestamp;
    
    /**
     * @brief Generate ISO 8601 timestamp
     */
    static std::string generate_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    }
};

/**
 * @brief Result payload structure (POD-like for JSON serialization)
 * 
 * All mandatory fields must be populated before serialization.
 */
struct ResultPayload {
    // Schema and identification
    std::string schema_version = "1.0";
    std::string run_uuid;  // UUIDv4
    
    // Instance information
    std::string instance_name;
    std::vector<std::string> instance_tags;
    
    // Solver information
    std::string solver_id;
    std::string solver_version;
    std::string formulation;
    
    // Configuration
    std::string config_digest;  // SHA256 hash of config
    std::map<std::string, std::string> config_params;  // Key-value pairs
    
    // Optimization status
    OptimizationStatus optimization_status_code;
    std::string optimization_status_description;
    bool has_solution = false;
    bool has_bound = false;
    
    // Results
    double objective = 0.0;
    double primal_bound = 0.0;
    double dual_bound = 0.0;
    double gap = 0.0;
    double gap_percent = 0.0;
    double best_solution_time = 0.0;
    
    // Runtime statistics
    RuntimeStats runtime_stats;
    
    // Solution (only if has_solution == true)
    Solution solution;
    
    // Reproducibility
    Reproducibility reproducibility;
    
    // Artifacts
    std::string log_file_path;
    std::string result_file_path;
    std::string warm_start_path;  // Optional, can be empty
    
    // Solver-specific metadata (flexible JSON object)
    std::map<std::string, std::string> solver_metadata;
    
    /**
     * @brief Validate payload consistency
     * @return pair<is_valid, error_message>
     */
    std::pair<bool, std::string> validate() const {
        // Check required fields
        if (run_uuid.empty()) {
            return {false, "run_uuid is required"};
        }
        if (instance_name.empty()) {
            return {false, "instance_name is required"};
        }
        if (solver_id.empty()) {
            return {false, "solver_id is required"};
        }
        
        // Validate status consistency
        if (has_solution && solution.tree_edges.empty()) {
            return {false, "has_solution is true but solution.tree_edges is empty"};
        }
        if (!has_solution && !solution.tree_edges.empty()) {
            return {false, "has_solution is false but solution.tree_edges is not empty"};
        }
        
        // Validate solution cost consistency
        if (has_solution) {
            if (std::abs(solution.tree_cost - objective) > 1e-6) {
                return {false, "solution.tree_cost does not match objective"};
            }
        }
        
        // Validate status codes
        if (optimization_status_code == OptimizationStatus::INFEASIBLE && has_solution) {
            return {false, "INFEASIBLE status but has_solution is true"};
        }
        
        return {true, ""};
    }
};

} // namespace common
} // namespace ocst

#endif // OCST_COMMON_RESULT_PAYLOAD_H

