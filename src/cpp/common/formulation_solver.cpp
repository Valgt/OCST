/**
 * @file formulation_solver.cpp
 * @brief Implementation of simplified FormulationSolver base class
 * 
 * Moves implementations from header to source file (proper C++ practice).
 * 
 * @author Phase 1.5 Standardization Team
 * @date 2025-11-10
 */

#include "formulation_solver.h"
#include "build_info.h"
#include <queue>
#include <iostream>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace ocst {
namespace common {

//=============================================================================
// CONSTRUCTOR / DESTRUCTOR
//=============================================================================

FormulationSolver::FormulationSolver(const OCSTInstance& instance,
                                     const std::string& name,
                                     const std::string& version)
    : instance_(instance),
      formulation_name_(name),
      formulation_version_(version)
{
    // Lightweight constructor - actual setup happens in configure()
}

//=============================================================================
// MAIN SOLVE METHOD (Template Method Pattern)
//=============================================================================

ResultPayload FormulationSolver::solve(const SolverConfig& config) {
    solve_start_time_ = std::chrono::high_resolution_clock::now();

    try {
        // LIFECYCLE HOOK 1: Configure
        if (config.verbose) {
            std::cout << "[" << formulation_name_ << "] Configuring solver..." << std::endl;
        }
        configure(config);
        
        // LIFECYCLE HOOK 2: Build model (variables + constraints + objective)
        if (config.verbose) {
            std::cout << "[" << formulation_name_ << "] Building model..." << std::endl;
        }
        build_model(config);
        
        // OPTIMIZE (not a hook - always the same)
        if (config.verbose) {
            std::cout << "[" << formulation_name_ << "] Optimizing (time limit: " 
                      << config.time_limit_seconds << "s)..." << std::endl;
        }
        model_->optimize();
        
        solve_end_time_ = std::chrono::high_resolution_clock::now();
        
        // LIFECYCLE HOOK 3: Collect results
        if (config.verbose) {
            std::cout << "[" << formulation_name_ << "] Collecting results..." << std::endl;
        }
        ResultPayload result = collect_results();

        // Runtime metrics
        const double wall_clock_seconds = std::chrono::duration<double>(solve_end_time_ - solve_start_time_).count();
        result.runtime_stats.wall_clock_seconds = wall_clock_seconds;
        try {
            result.runtime_stats.solver_nodes = static_cast<int>(model_->get(GRB_DoubleAttr_NodeCount));
        } catch (...) {
            result.runtime_stats.solver_nodes = 0;
        }
        try {
            result.runtime_stats.solver_iterations = static_cast<int>(model_->get(GRB_DoubleAttr_IterCount));
        } catch (...) {
            result.runtime_stats.solver_iterations = 0;
        }
        try {
            result.best_solution_time = model_->get(GRB_DoubleAttr_Runtime);
        } catch (...) {
            result.best_solution_time = wall_clock_seconds;
        }
        
        // Add metadata
        populate_metadata(result, config);
        
        return result;
        
    } catch (GRBException& e) {
        std::cerr << "[" << formulation_name_ << "] Gurobi error: " << e.getMessage() << std::endl;
        
        ResultPayload error_result;
        error_result.optimization_status_code = OptimizationStatus::ERROR;
        error_result.has_solution = false;
        populate_metadata(error_result, config);
        return error_result;
        
    } catch (std::exception& e) {
        std::cerr << "[" << formulation_name_ << "] Error: " << e.what() << std::endl;
        
        ResultPayload error_result;
        error_result.optimization_status_code = OptimizationStatus::ERROR;
        error_result.has_solution = false;
        populate_metadata(error_result, config);
        return error_result;
    }
}

//=============================================================================
// DEFAULT IMPLEMENTATIONS OF LIFECYCLE HOOKS
//=============================================================================

void FormulationSolver::configure(const SolverConfig& config) {
    // Create Gurobi environment and model
    env_ = std::make_unique<GRBEnv>();
    model_ = std::make_unique<GRBModel>(*env_);

    // Apply standard configuration from config
    env_->set(GRB_IntParam_OutputFlag, config.verbose ? 1 : 0);
    env_->set(GRB_IntParam_Seed, config.seed);  // Set random seed for reproducibility
    model_->set(GRB_DoubleParam_TimeLimit, config.time_limit_seconds);
    model_->set(GRB_DoubleParam_MIPGap, config.mip_gap);
    model_->set(GRB_DoubleParam_Heuristics, config.heuristics_level);
    model_->set(GRB_IntParam_Threads, config.threads);

    // Conservative defaults for numerical stability
    model_->set(GRB_IntParam_NumericFocus, 2);
    model_->set(GRB_IntParam_MIPFocus, 1);
    
    // Subclasses can override to add formulation-specific tuning
}

//=============================================================================
// HELPER METHODS
//=============================================================================

bool FormulationSolver::is_graph_connected() const {
    if (instance_.num_nodes == 0) return true;
    
    // BFS from node 0
    std::vector<bool> visited(instance_.num_nodes, false);
    std::queue<int> q;
    q.push(0);
    visited[0] = true;
    int visited_count = 1;
    
    while (!q.empty()) {
        int u = q.front();
        q.pop();
        
        // Check all edges
        for (const Edge& edge : instance_.edges) {
            int neighbor = -1;
            if (edge.source == u && !visited[edge.destination]) {
                neighbor = edge.destination;
            } else if (edge.destination == u && !visited[edge.source]) {
                neighbor = edge.source;
            }
            
            if (neighbor != -1) {
                visited[neighbor] = true;
                visited_count++;
                q.push(neighbor);
            }
        }
    }
    
    return visited_count == instance_.num_nodes;
}

OptimizationStatus FormulationSolver::convert_gurobi_status(int grb_status) const {
    switch (grb_status) {
        case GRB_OPTIMAL:
            return OptimizationStatus::OPTIMAL;
        case GRB_INFEASIBLE:
            return OptimizationStatus::INFEASIBLE;
        case GRB_INF_OR_UNBD:
            return OptimizationStatus::INFEASIBLE;  // Treat as infeasible
        case GRB_UNBOUNDED:
            return OptimizationStatus::UNBOUNDED;
        case GRB_TIME_LIMIT:
            return OptimizationStatus::TIME_LIMIT;
        case GRB_NODE_LIMIT:
        case GRB_ITERATION_LIMIT:
        case GRB_SOLUTION_LIMIT:
            return OptimizationStatus::SUBOPTIMAL;  // Hit a limit, but may have solution
        case GRB_USER_OBJ_LIMIT:
            return OptimizationStatus::OPTIMAL;  // Reached objective limit (good enough)
        case GRB_INTERRUPTED:
            return OptimizationStatus::INTERRUPTED;
        default:
            return OptimizationStatus::SUBOPTIMAL;
    }
}

void FormulationSolver::populate_metadata(ResultPayload& result, const SolverConfig& config) const {
    // Solver identification
    result.solver_id = formulation_name_;
    result.solver_version = formulation_version_;
    result.formulation = formulation_name_;
    
    // Reproducibility
    result.reproducibility.git_commit = GIT_COMMIT;
    result.reproducibility.git_dirty = GIT_DIRTY;
    result.reproducibility.seed = config.seed;
    
    // Timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
    result.reproducibility.timestamp = ss.str();
    
    // Configuration digest (simplified)
    result.config_digest = formulation_name_ + "_v" + formulation_version_;
    
    // Store key configuration parameters
    result.config_params["time_limit"] = std::to_string(config.time_limit_seconds);
    result.config_params["mip_gap"] = std::to_string(config.mip_gap);
    result.config_params["threads"] = std::to_string(config.threads);
    result.config_params["heuristics"] = std::to_string(config.heuristics_level);
    result.config_params["warm_start_enabled"] = config.enable_warm_start ? "true" : "false";
    if (!config.warm_start_ideas.empty()) {
        std::string ideas_csv;
        for (size_t i = 0; i < config.warm_start_ideas.size(); ++i) {
            if (i > 0) ideas_csv += ",";
            ideas_csv += config.warm_start_ideas[i];
        }
        result.config_params["warm_start_ideas"] = ideas_csv;
    }
    
    // Store solver-specific metrics in metadata
    result.solver_metadata["lazy_constraints"] = std::to_string(lazy_constraints_added_);
    result.solver_metadata["cutting_planes"] = std::to_string(cutting_planes_added_);
    result.solver_metadata["callback_invocations"] = std::to_string(callback_invocations_);

    // Status description (human-readable)
    switch (result.optimization_status_code) {
        case OptimizationStatus::OPTIMAL:
            result.optimization_status_description = "Optimal solution found";
            break;
        case OptimizationStatus::TIME_LIMIT:
            result.optimization_status_description = "Time limit reached";
            break;
        case OptimizationStatus::INFEASIBLE:
            result.optimization_status_description = "Model infeasible";
            break;
        case OptimizationStatus::UNBOUNDED:
            result.optimization_status_description = "Model unbounded";
            break;
        case OptimizationStatus::SUBOPTIMAL:
            result.optimization_status_description = "Limit reached (suboptimal)";
            break;
        case OptimizationStatus::INTERRUPTED:
            result.optimization_status_description = "Interrupted";
            break;
        default:
            result.optimization_status_description = "Error";
            break;
    }
}

} // namespace common
} // namespace ocst
