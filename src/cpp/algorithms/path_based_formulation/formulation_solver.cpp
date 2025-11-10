/**
 * @file formulation_solver.cpp
 * @brief Implementation of simplified FormulationSolver base class
 * 
 * Moves implementations from header to source file (proper C++ practice).
 * 
 * @author Phase 1.5 Standardization Team
 * @date 2025-11-10
 */

#include "include/formulation_solver.h"
#include "build_info.h"
#include <queue>
#include <iostream>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace ocst {
namespace path_based {

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
    config_ = config;
    solve_start_time_ = std::chrono::high_resolution_clock::now();
    
    try {
        // LIFECYCLE HOOK 1: Configure
        if (config_.verbose) {
            std::cout << "[" << formulation_name_ << "] Configuring solver..." << std::endl;
        }
        configure();
        
        // LIFECYCLE HOOK 2: Build model (variables + constraints + objective)
        if (config_.verbose) {
            std::cout << "[" << formulation_name_ << "] Building model..." << std::endl;
        }
        build_model();
        
        // OPTIMIZE (not a hook - always the same)
        if (config_.verbose) {
            std::cout << "[" << formulation_name_ << "] Optimizing (time limit: " 
                      << config_.time_limit_seconds << "s)..." << std::endl;
        }
        model_->optimize();
        
        solve_end_time_ = std::chrono::high_resolution_clock::now();
        
        // LIFECYCLE HOOK 3: Collect results
        if (config_.verbose) {
            std::cout << "[" << formulation_name_ << "] Collecting results..." << std::endl;
        }
        ResultPayload result = collect_results();
        
        // Add metadata
        populate_metadata(result);
        
        return result;
        
    } catch (GRBException& e) {
        std::cerr << "[" << formulation_name_ << "] Gurobi error: " << e.getMessage() << std::endl;
        
        ResultPayload error_result;
        error_result.optimization_status_code = OptimizationStatus::ERROR;
        error_result.has_solution = false;
        populate_metadata(error_result);
        return error_result;
        
    } catch (std::exception& e) {
        std::cerr << "[" << formulation_name_ << "] Error: " << e.what() << std::endl;
        
        ResultPayload error_result;
        error_result.optimization_status_code = OptimizationStatus::ERROR;
        error_result.has_solution = false;
        populate_metadata(error_result);
        return error_result;
    }
}

//=============================================================================
// DEFAULT IMPLEMENTATIONS OF LIFECYCLE HOOKS
//=============================================================================

void FormulationSolver::configure() {
    // Create Gurobi environment and model
    env_ = std::make_unique<GRBEnv>();
    model_ = std::make_unique<GRBModel>(*env_);
    
    // Apply standard configuration from config_
    env_->set(GRB_IntParam_OutputFlag, config_.verbose ? 1 : 0);
    model_->set(GRB_DoubleParam_TimeLimit, config_.time_limit_seconds);
    model_->set(GRB_DoubleParam_MIPGap, config_.mip_gap);
    model_->set(GRB_DoubleParam_Heuristics, config_.heuristics_level);
    model_->set(GRB_IntParam_Threads, config_.threads);
    
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

void FormulationSolver::populate_metadata(ResultPayload& result) const {
    // Solver identification
    result.solver_id = formulation_name_;
    result.solver_version = formulation_version_;
    result.formulation = formulation_name_;
    
    // Reproducibility
    result.reproducibility.git_commit = GIT_COMMIT;
    result.reproducibility.git_dirty = GIT_DIRTY;
    result.reproducibility.seed = 0;  // TODO: Add seed support
    
    // Timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
    result.reproducibility.timestamp = ss.str();
    
    // Configuration digest (simplified)
    result.config_digest = formulation_name_ + "_v" + formulation_version_;
    
    // Store key configuration parameters
    result.config_params["time_limit"] = std::to_string(config_.time_limit_seconds);
    result.config_params["mip_gap"] = std::to_string(config_.mip_gap);
    result.config_params["threads"] = std::to_string(config_.threads);
    result.config_params["heuristics"] = std::to_string(config_.heuristics_level);
    
    // Store solver-specific metrics in metadata
    result.solver_metadata["lazy_constraints"] = std::to_string(lazy_constraints_added_);
    result.solver_metadata["cutting_planes"] = std::to_string(cutting_planes_added_);
    result.solver_metadata["callback_invocations"] = std::to_string(callback_invocations_);
}

} // namespace path_based
} // namespace ocst

