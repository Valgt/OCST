/**
 * @file formulation_solver_v2.h
 * @brief Simplified solver interface for OCST formulations (Phase 1.5 - Simplified)
 * 
 * DESIGN PHILOSOPHY:
 * - KISS (Keep It Simple, Stupid): Only 3 lifecycle hooks
 * - Minimal abstraction: Only what's reused across 3+ formulations
 * - Headers for declarations, .cpp for implementations
 * - Self-documenting code > excessive comments
 * 
 * LIFECYCLE (Template Method Pattern - Simplified):
 *   1. configure()    - Setup Gurobi environment and parameters
 *   2. build_model()  - Create variables, constraints, and objective
 *   3. collect_results() - Extract solution and metrics
 * 
 * Note: solve_model() is NOT virtual (always model_->optimize())
 * 
 * @author Phase 1.5 Standardization Team
 * @date 2025-11-10
 * @version 2.0 (Simplified)
 */

#ifndef OCST_COMMON_FORMULATION_SOLVER_H
#define OCST_COMMON_FORMULATION_SOLVER_H

#include <string>
#include <memory>
#include <chrono>
#include <gurobi_c++.h>

#include "common_types.h"
#include "result_serializer.h"
#include "config.h"
#include "structured_logger.h"

namespace ocst {
namespace common {

/**
 * @brief Simplified solver configuration
 *
 * Now integrates with the common Config system.
 * Only includes parameters that are actually used across formulations.
 * Formulation-specific tuning can be done in configure() override.
 */
struct SolverConfig {
    // Core parameters (used by all formulations)
    double time_limit_seconds = 3600.0;
    double mip_gap = 1e-6;
    int threads = 1;
    bool verbose = true;

    // Optional parameters
    double heuristics_level = 0.5;  // Gurobi heuristics (0.0-1.0)
    bool enable_warm_start = false;

    // Integration with common config
    Config common_config;

    // Logger for structured logging
    std::unique_ptr<StructuredLogger> logger;
};

/**
 * @brief Base class for OCST formulation solvers (Simplified)
 * 
 * Provides common infrastructure for all formulations with minimal hooks.
 * 
 * USAGE:
 *   class MyFormulation : public FormulationSolver {
 *   public:
 *       MyFormulation(const OCSTInstance& inst) 
 *           : FormulationSolver(inst, "my_formulation") {}
 *   
 *   protected:
 *       void configure() override { ... }        // Setup Gurobi
 *       void build_model() override { ... }      // Variables + Constraints + Objective
 *       ResultPayload collect_results() override { ... }  // Extract solution
 *   };
 */
class FormulationSolver {
protected:
    // Core components
    const OCSTInstance& instance_;
    std::unique_ptr<GRBEnv> env_;
    std::unique_ptr<GRBModel> model_;
    
    // Metadata
    std::string formulation_name_;
    std::string formulation_version_;
    
    // Timing
    std::chrono::high_resolution_clock::time_point solve_start_time_;
    std::chrono::high_resolution_clock::time_point solve_end_time_;
    
    // Metrics (updated by subclass, read by collect_results)
    int lazy_constraints_added_ = 0;
    int cutting_planes_added_ = 0;
    int callback_invocations_ = 0;
    
public:
    /**
     * @brief Constructor
     */
    FormulationSolver(const OCSTInstance& instance, 
                      const std::string& name,
                      const std::string& version = "1.0");
    
    /**
     * @brief Destructor
     */
    virtual ~FormulationSolver() = default;
    
    /**
     * @brief Main solve method (Template Method pattern)
     * 
     * Executes: configure() → build_model() → optimize() → collect_results()
     * 
     * DO NOT override this method. Override the lifecycle hooks instead.
     */
    ResultPayload solve(const SolverConfig& config);
    
    // Accessors
    const std::string& formulation_name() const { return formulation_name_; }
    const std::string& formulation_version() const { return formulation_version_; }

protected:
    //=========================================================================
    // LIFECYCLE HOOKS (implement in subclass)
    //=========================================================================
    
    /**
     * @brief Configure Gurobi environment
     *
     * Create GRBEnv and GRBModel, set parameters.
     * Default implementation provides standard configuration.
     * Override to add formulation-specific tuning.
     */
    virtual void configure(const SolverConfig& config);
    
    /**
     * @brief Build complete optimization model
     *
     * Create variables, add constraints, set objective.
     * This is the main work - pure virtual (must implement).
     */
    virtual void build_model(const SolverConfig& config) = 0;
    
    /**
     * @brief Extract solution and metrics
     * 
     * Query Gurobi model for solution, populate ResultPayload.
     * Pure virtual (must implement).
     */
    virtual ResultPayload collect_results() = 0;
    
    //=========================================================================
    // HELPER METHODS (available to subclasses)
    //=========================================================================
    
    /**
     * @brief Check graph connectivity using BFS
     */
    bool is_graph_connected() const;
    
    /**
     * @brief Convert Gurobi status code to OptimizationStatus enum
     */
    OptimizationStatus convert_gurobi_status(int grb_status) const;
    
    /**
     * @brief Populate reproducibility and solver metadata
     */
    void populate_metadata(ResultPayload& result, const SolverConfig& config) const;
};

} // namespace common
} // namespace ocst

#endif // OCST_COMMON_FORMULATION_SOLVER_H

