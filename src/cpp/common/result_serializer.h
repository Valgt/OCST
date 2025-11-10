#ifndef OCST_COMMON_RESULT_SERIALIZER_H
#define OCST_COMMON_RESULT_SERIALIZER_H

#include <string>
#include <vector>
#include <map>
#include <utility>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <cmath>

#include <nlohmann/json.hpp>

namespace ocst {
namespace common {

/**
 * @brief Optimization status codes aligned with result.schema.v1.json.
 */
enum class OptimizationStatus {
    OPTIMAL,
    TIME_LIMIT,
    INFEASIBLE,
    UNBOUNDED,
    SUBOPTIMAL,
    INTERRUPTED,
    ERROR
};

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
 * @brief Runtime statistics captured from the solver.
 */
struct RuntimeStats {
    double wall_clock_seconds = 0.0;
    double cpu_seconds = 0.0;
    int solver_nodes = 0;
    int solver_iterations = 0;
    std::string termination_reason;
};

/**
 * @brief Edge inside the solution tree.
 */
struct TreeEdge {
    int source;
    int destination;
};

/**
 * @brief Solution tree information stored in the JSON payload.
 */
struct SolutionTree {
    std::vector<TreeEdge> tree_edges;
    double tree_cost = 0.0;
    bool is_spanning_tree = false;
    bool is_connected = false;
};

/**
 * @brief Metadata required for reproducibility.
 */
struct Reproducibility {
    std::string git_commit;
    bool git_dirty = false;
    int seed = 0;
    std::string timestamp;
};

/**
 * @brief Payload structure serialized to JSON.
 *
 * This struct captures every field defined in schemas/result.schema.v1.json.
 */
struct ResultPayload {
    // Schema identification
    std::string schema_version = "1.0";
    std::string run_uuid;

    // Instance information
    std::string instance_name;
    std::vector<std::string> instance_tags;

    // Solver information
    std::string solver_id;
    std::string solver_version;
    std::string formulation;

    // Configuration
    std::string config_digest;
    std::map<std::string, std::string> config_params;

    // Optimization status
    OptimizationStatus optimization_status_code = OptimizationStatus::ERROR;
    std::string optimization_status_description;
    bool has_solution = false;
    bool has_bound = false;

    // Result metrics
    double objective = 0.0;
    double primal_bound = 0.0;
    double dual_bound = 0.0;
    double gap = 0.0;
    double gap_percent = 0.0;
    double best_solution_time = 0.0;

    // Runtime metrics
    RuntimeStats runtime_stats;

    // Solution tree
    SolutionTree solution;

    // Reproducibility info
    Reproducibility reproducibility;

    // Artifact locations
    std::string log_file_path;
    std::string result_file_path;
    std::string warm_start_path;

    // Formulation-specific metadata (string key/value for now)
    std::map<std::string, std::string> solver_metadata;

    /**
     * @brief Basic validation guard before serialization.
     * @return Pair (is_valid, error_message)
     */
    std::pair<bool, std::string> validate() const {
        if (run_uuid.empty()) {
            return {false, "run_uuid is required"};
        }
        if (instance_name.empty()) {
            return {false, "instance_name is required"};
        }
        if (solver_id.empty()) {
            return {false, "solver_id is required"};
        }

        if (has_solution) {
            if (solution.tree_edges.empty()) {
                return {false, "has_solution is true but solution.tree_edges is empty"};
            }
            if (std::abs(solution.tree_cost - objective) > 1e-6) {
                return {false, "solution.tree_cost does not match objective"};
            }
        } else {
            if (!solution.tree_edges.empty()) {
                return {false, "has_solution is false but solution.tree_edges is not empty"};
            }
        }

        if (optimization_status_code == OptimizationStatus::INFEASIBLE && has_solution) {
            return {false, "INFEASIBLE status but has_solution is true"};
        }

        return {true, ""};
    }
};

/**
 * @brief Serializer that emits standardized JSON documents.
 *
 * This is the only header that depends on nlohmann::json for Phase 1.5.
 */
class ResultSerializer final {
public:
    ResultSerializer() = delete;

    static nlohmann::json to_json(const ResultPayload& payload) {
        const auto validation = payload.validate();
        if (!validation.first) {
            throw std::invalid_argument("Invalid ResultPayload: " + validation.second);
        }

        nlohmann::json json_result;
        json_result["schema_version"] = payload.schema_version;
        json_result["run_uuid"] = payload.run_uuid;

        json_result["instance"] = {
            {"name", payload.instance_name},
            {"tags", payload.instance_tags}
        };

        json_result["solver"] = {
            {"id", payload.solver_id},
            {"version", payload.solver_version},
            {"formulation", payload.formulation}
        };

        nlohmann::json config_json;
        config_json["digest"] = payload.config_digest;
        for (const auto& entry : payload.config_params) {
            config_json[entry.first] = entry.second;
        }
        json_result["config"] = std::move(config_json);

        json_result["optimization_status"] = {
            {"code", optimization_status_to_string(payload.optimization_status_code)},
            {"description", payload.optimization_status_description},
            {"has_solution", payload.has_solution},
            {"has_bound", payload.has_bound}
        };

        json_result["results"] = {
            {"objective", payload.objective},
            {"primal_bound", payload.primal_bound},
            {"dual_bound", payload.dual_bound},
            {"gap", payload.gap},
            {"gap_percent", payload.gap_percent},
            {"best_solution_time", payload.best_solution_time}
        };

        json_result["runtime"] = {
            {"wall_clock_seconds", payload.runtime_stats.wall_clock_seconds},
            {"cpu_seconds", payload.runtime_stats.cpu_seconds},
            {"solver_nodes", payload.runtime_stats.solver_nodes},
            {"solver_iterations", payload.runtime_stats.solver_iterations},
            {"termination_reason", payload.runtime_stats.termination_reason}
        };

        if (payload.has_solution) {
            nlohmann::json solution_json;
            nlohmann::json edges_json = nlohmann::json::array();
            for (const auto& edge : payload.solution.tree_edges) {
                edges_json.push_back({
                    {"source", edge.source},
                    {"destination", edge.destination}
                });
            }
            solution_json["tree_edges"] = std::move(edges_json);
            solution_json["tree_cost"] = payload.solution.tree_cost;
            solution_json["is_spanning_tree"] = payload.solution.is_spanning_tree;
            solution_json["is_connected"] = payload.solution.is_connected;
            json_result["solution"] = std::move(solution_json);
        }

        json_result["reproducibility"] = {
            {"git_commit", payload.reproducibility.git_commit},
            {"git_dirty", payload.reproducibility.git_dirty},
            {"seed", payload.reproducibility.seed},
            {"timestamp", payload.reproducibility.timestamp}
        };

        if (!payload.log_file_path.empty() ||
            !payload.result_file_path.empty() ||
            !payload.warm_start_path.empty()) {
            nlohmann::json artifacts_json;
            if (!payload.log_file_path.empty()) {
                artifacts_json["log_file"] = payload.log_file_path;
            }
            if (!payload.result_file_path.empty()) {
                artifacts_json["result_file"] = payload.result_file_path;
            }
            if (!payload.warm_start_path.empty()) {
                artifacts_json["warm_start"] = payload.warm_start_path;
            }
            json_result["artifacts"] = std::move(artifacts_json);
        }

        if (!payload.solver_metadata.empty()) {
            nlohmann::json metadata_json;
            for (const auto& entry : payload.solver_metadata) {
                metadata_json[entry.first] = entry.second;
            }
            json_result["solver_metadata"] = std::move(metadata_json);
        }

        return json_result;
    }

    static void write_to_file(const ResultPayload& payload,
                              const std::string& filepath,
                              bool pretty = true) {
        const auto json_result = to_json(payload);

        const std::filesystem::path output_path(filepath);
        const auto parent = output_path.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }

        std::ofstream file(output_path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open result file for writing: " + filepath);
        }

        if (pretty) {
            file << json_result.dump(2);
        } else {
            file << json_result.dump();
        }
    }
};

} // namespace common
} // namespace ocst

#endif // OCST_COMMON_RESULT_SERIALIZER_H

