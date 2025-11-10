/**
 * @file formulation_solver.h
 * @brief Common solver interface for OCST formulations (Workstream 2)
 * 
 * Provides a unified interface for all OCST formulations with standardized
 * lifecycle hooks:
 *   1. configure()         - Setup Gurobi environment and parameters
 *   2. build_variables()   - Create decision variables
 *   3. build_constraints() - Add problem constraints
 *   4. build_objective()   - Define objective function
 *   5. warm_start()        - Set initial solution (optional)
 *   6. solve_model()       - Invoke optimizer
 *   7. collect_results()   - Extract and serialize solution
 * 
 * Design Philosophy:
 * - Template Method Pattern: Base class defines solve() skeleton, 
 *   subclasses implement formulation-specific steps
 * - Each hook is virtual and can be overridden
 * - Default implementations provided where applicable
 * - Metrics collection standardized across all formulations
 * 
 * Namespace: ocst::path_based (will become ocst::common after promotion)
 * 
 * @author Phase 1.5 Standardization Team
 * @date 2025-11-10
 */

#ifndef OCST_PATH_BASED_FORMULATION_SOLVER_H
#define OCST_PATH_BASED_FORMULATION_SOLVER_H

#include <string>
#include <memory>
#include <chrono>
#include <iostream>
#include <gurobi_c++.h>

#include "common_types.h"
#include "result_serializer.h"

namespace ocst {
namespace path_based {

/**
 * @brief Abstract base class for OCST formulations
 * 
 * Defines a unified solver interface with lifecycle hooks that all
 * formulations must implement. Uses Template Method pattern to ensure
 * consistent workflow while allowing formulation-specific implementations.
 * 
 * Usage Example:
 * 
 *   class MyFormulationSolver : public FormulationSolver {
 *   public:
 *       explicit MyFormulationSolver(const OCSTInstance& instance)
 *           : FormulationSolver(instance, "my_formulation") {}
 *   
 *   protected:
 *       void build_variables() override { ... }
 *       void build_constraints() override { ... }
 *       void build_objective() override { ... }
 *       ResultPayload collect_results() override { ... }
 *   };
 * 
 *   MyFormulationSolver solver(instance);
 *   ResultPayload result = solver.solve(config);
 */
class FormulationSolver {
public:
    /**
     * @brief Configuration for solver execution
     * 
     * Consolidates all solver parameters in a single struct for easy
     * passing and future extensibility. Will be loaded from JSON config
     * files in Workstream 4.
     */
    struct SolverConfig {
        double time_limit_seconds = 3600.0;   ///< Maximum wall-clock time
        double mip_gap = 1e-6;                ///< Relative optimality gap
        double mip_gap_abs = 1e-6;            ///< Absolute optimality gap
        double heuristics_level = 0.5;        ///< Gurobi heuristics (0.0-1.0)
        int threads = 1;                      ///< Number of threads (1 = reproducible)
        int numeric_focus = 2;                ///< Numerical stability (0-3)
        int mip_focus = 1;                    ///< Focus on feasibility vs optimality
        bool enable_presolve = false;         ///< Presolve can interfere with callbacks
        bool enable_default_cuts = false;     ///< Disable to prioritize custom cuts
        bool verbose = true;                  ///< Console output
        bool enable_warm_start = false;       ///< Use MST-based initial solution
        
        // Formulation-specific extensions stored as key-value pairs
        std::unordered_map<std::string, double> extensions;
    };

protected:
    // Instance and environment
    const OCSTInstance& instance_;           ///< Problem instance (immutable reference)
    std::unique_ptr<GRBEnv> env_;            ///< Gurobi environment
    std::unique_ptr<GRBModel> model_;        ///< Gurobi model
    
    // Solver metadata
    std::string formulation_name_;           ///< Identifier (e.g., "path_based")
    std::string formulation_version_;        ///< Version string (e.g., "1.0.0")
    
    // Configuration
    SolverConfig config_;                    ///< Solver parameters
    
    // Timing
    std::chrono::high_resolution_clock::time_point solve_start_time_;
    std::chrono::high_resolution_clock::time_point solve_end_time_;
    
    // Metrics (to be collected during solve)
    int lazy_constraints_added_ = 0;         ///< Lazy cuts (e.g., SEC)
    int cutting_planes_added_ = 0;           ///< User cuts (e.g., fractional SEC)
    int callback_invocations_ = 0;           ///< Total callback calls
    
public:
    /**
     * @brief Constructor
     * @param instance Problem instance (must outlive solver)
     * @param formulation_name Identifier for this formulation
     * @param formulation_version Version string (default "1.0.0")
     */
    explicit FormulationSolver(const OCSTInstance& instance,
                               const std::string& formulation_name,
                               const std::string& formulation_version = "1.0.0")
        : instance_(instance),
          env_(nullptr),
          model_(nullptr),
          formulation_name_(formulation_name),
          formulation_version_(formulation_version)
    {
        // Environment and model will be created in configure()
    }
    
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~FormulationSolver() = default;
    
    /**
     * @brief Main solve method (Template Method pattern)
     * 
     * Executes the full solver pipeline with standardized lifecycle hooks.
     * This method should NOT be overridden; override the individual hooks instead.
     * 
     * @param config Solver configuration
     * @return ResultPayload with solution and metrics
     * 
     * @throws GRBException if Gurobi encounters an error
     * @throws std::runtime_error if lifecycle hook fails
     */
    ResultPayload solve(const SolverConfig& config) {
        config_ = config;
        
        try {
            solve_start_time_ = std::chrono::high_resolution_clock::now();
            
            // Lifecycle Hook 1: Configure environment
            if (config_.verbose) {
                std::cout << "[" << formulation_name_ << "] Configuring Gurobi environment..." << std::endl;
            }
            configure();
            
            // Lifecycle Hook 2: Create decision variables
            if (config_.verbose) {
                std::cout << "[" << formulation_name_ << "] Building variables..." << std::endl;
            }
            build_variables();
            
            // Lifecycle Hook 3: Add constraints
            if (config_.verbose) {
                std::cout << "[" << formulation_name_ << "] Building constraints..." << std::endl;
            }
            build_constraints();
            
            // Lifecycle Hook 4: Define objective
            if (config_.verbose) {
                std::cout << "[" << formulation_name_ << "] Building objective..." << std::endl;
            }
            build_objective();
            
            // Lifecycle Hook 5: Warm-start (optional)
            if (config_.enable_warm_start) {
                if (config_.verbose) {
                    std::cout << "[" << formulation_name_ << "] Setting warm-start solution..." << std::endl;
                }
                warm_start();
            }
            
            // Lifecycle Hook 6: Optimize
            if (config_.verbose) {
                std::cout << "[" << formulation_name_ << "] Solving model (time limit: " 
                          << config_.time_limit_seconds << "s)..." << std::endl;
            }
            solve_model();
            
            solve_end_time_ = std::chrono::high_resolution_clock::now();
            
            // Lifecycle Hook 7: Collect results
            if (config_.verbose) {
                std::cout << "[" << formulation_name_ << "] Collecting results..." << std::endl;
            }
            ResultPayload result = collect_results();
            
            // Populate reproducibility metadata
            populate_metadata(result);
            
            return result;
            
        } catch (GRBException& e) {
            std::cerr << "[" << formulation_name_ << "] Gurobi error: " << e.getMessage() << std::endl;
            
            // Return error payload
            ResultPayload error_result;
            error_result.optimization_status_code = OptimizationStatus::ERROR;
            error_result.has_solution = false;
            populate_metadata(error_result);
            
            return error_result;
            
        } catch (std::exception& e) {
            std::cerr << "[" << formulation_name_ << "] Runtime error: " << e.what() << std::endl;
            
            ResultPayload error_result;
            error_result.optimization_status_code = OptimizationStatus::ERROR;
            error_result.has_solution = false;
            populate_metadata(error_result);
            
            return error_result;
        }
    }
    
    /**
     * @brief Get formulation identifier
     */
    const std::string& formulation_name() const { return formulation_name_; }
    
    /**
     * @brief Get formulation version
     */
    const std::string& formulation_version() const { return formulation_version_; }

protected:
    //=========================================================================
    // LIFECYCLE HOOKS (to be implemented by subclasses)
    //=========================================================================
    
    /**
     * @brief Configure Gurobi environment and model parameters
     * 
     * Called first in solve() pipeline. Subclasses should:
     * - Create GRBEnv and GRBModel
     * - Set common Gurobi parameters from config_
     * - Add formulation-specific parameter tuning
     * 
     * Default implementation provides standard configuration.
     */
    virtual void configure() {
        // Create Gurobi environment
        env_ = std::make_unique<GRBEnv>();
        model_ = std::make_unique<GRBModel>(*env_);
        
        // Apply configuration
        env_->set(GRB_IntParam_OutputFlag, config_.verbose ? 1 : 0);
        model_->set(GRB_DoubleParam_TimeLimit, config_.time_limit_seconds);
        model_->set(GRB_DoubleParam_MIPGap, config_.mip_gap);
        model_->set(GRB_DoubleParam_MIPGapAbs, config_.mip_gap_abs);
        model_->set(GRB_DoubleParam_Heuristics, config_.heuristics_level);
        model_->set(GRB_IntParam_Threads, config_.threads);
        model_->set(GRB_IntParam_NumericFocus, config_.numeric_focus);
        model_->set(GRB_IntParam_MIPFocus, config_.mip_focus);
        model_->set(GRB_IntParam_Presolve, config_.enable_presolve ? -1 : 0);
        model_->set(GRB_IntParam_Cuts, config_.enable_default_cuts ? -1 : 0);
    }
    
    /**
     * @brief Create decision variables
     * 
     * Subclasses must implement this to define their specific variables.
     * No default implementation (pure virtual).
     */
    virtual void build_variables() = 0;
    
    /**
     * @brief Add all problem constraints
     * 
     * Subclasses must implement this to add their formulation-specific
     * constraints. No default implementation (pure virtual).
     */
    virtual void build_constraints() = 0;
    
    /**
     * @brief Define objective function
     * 
     * Subclasses must implement this to set the objective.
     * No default implementation (pure virtual).
     */
    virtual void build_objective() = 0;
    
    /**
     * @brief Set initial solution (warm-start)
     * 
     * Optional hook for providing an initial feasible solution to speed up
     * convergence. Default implementation does nothing.
     * 
     * Common approach: MST-based heuristic
     */
    virtual void warm_start() {
        // Default: no-op
        // Subclasses can override to provide MST-based or other heuristics
    }
    
    /**
     * @brief Invoke Gurobi optimizer
     * 
     * Performs the actual optimization. Default implementation calls
     * model_->optimize(), but subclasses can override for custom behavior
     * (e.g., multi-phase optimization).
     */
    virtual void solve_model() {
        model_->optimize();
    }
    
    /**
     * @brief Extract solution and populate ResultPayload
     * 
     * Subclasses must implement this to:
     * - Check optimization status
     * - Extract variable values
     * - Build tree structure
     * - Populate all ResultPayload fields
     * 
     * No default implementation (pure virtual).
     * 
     * @return ResultPayload with complete solution data
     */
    virtual ResultPayload collect_results() = 0;
    
    //=========================================================================
    // HELPER METHODS (available to subclasses)
    //=========================================================================
    
    /**
     * @brief Populate reproducibility metadata in result
     * 
     * Fills in solver_id, runtime_stats, and reproducibility fields.
     * Called automatically by solve().
     */
    void populate_metadata(ResultPayload& result) {
        // Solver identification
        result.solver_id = formulation_name_;
        result.solver_version = formulation_version_;
        
        // Runtime statistics
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            solve_end_time_ - solve_start_time_
        );
        result.runtime_stats.wall_clock_seconds = duration.count() / 1000.0;
        
        if (model_) {
            try {
                result.runtime_stats.solver_nodes = static_cast<int>(model_->get(GRB_DoubleAttr_NodeCount));
                result.runtime_stats.solver_iterations = static_cast<int>(model_->get(GRB_DoubleAttr_IterCount));
            } catch (...) {
                // Attributes may not be available if solve failed
            }
        }
        
        // Reproducibility (will be populated by build system)
        #ifdef GIT_COMMIT
        result.reproducibility.git_commit = GIT_COMMIT;
        #else
        result.reproducibility.git_commit = "unknown";
        #endif
        
        #ifdef GIT_DIRTY
        result.reproducibility.git_dirty = GIT_DIRTY;
        #else
        result.reproducibility.git_dirty = false;
        #endif
        
        #ifdef BUILD_TIMESTAMP
        result.reproducibility.timestamp = BUILD_TIMESTAMP;
        #else
        result.reproducibility.timestamp = "unknown";
        #endif
        
        // Formulation-specific metadata
        result.solver_metadata["lazy_constraints"] = std::to_string(lazy_constraints_added_);
        result.solver_metadata["cutting_planes"] = std::to_string(cutting_planes_added_);
        result.solver_metadata["callback_invocations"] = std::to_string(callback_invocations_);
        result.solver_metadata["time_limit"] = std::to_string(config_.time_limit_seconds);
        result.solver_metadata["mip_gap"] = std::to_string(config_.mip_gap);
        result.solver_metadata["threads"] = std::to_string(config_.threads);
    }
    
    /**
     * @brief Check if graph is connected (basic DFS)
     * 
     * Utility method for validating instance before solving.
     * 
     * @return true if graph is connected, false otherwise
     */
    bool is_graph_connected() const {
        if (instance_.num_nodes <= 1) return true;
        
        std::vector<bool> visited(instance_.num_nodes, false);
        std::vector<int> stack;
        stack.push_back(0);
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
     * @brief Convert Gurobi status to OptimizationStatus enum
     * 
     * Standardizes status codes across all formulations.
     * 
     * @param gurobi_status GRB_OPTIMAL, GRB_TIME_LIMIT, etc.
     * @return Corresponding OptimizationStatus
     */
    OptimizationStatus convert_gurobi_status(int gurobi_status) const {
        switch (gurobi_status) {
            case GRB_OPTIMAL:
                return OptimizationStatus::OPTIMAL;
            case GRB_TIME_LIMIT:
                return OptimizationStatus::TIME_LIMIT;
            case GRB_INFEASIBLE:
                return OptimizationStatus::INFEASIBLE;
            case GRB_UNBOUNDED:
                return OptimizationStatus::UNBOUNDED;
            case GRB_SUBOPTIMAL:
                return OptimizationStatus::SUBOPTIMAL;
            case GRB_INTERRUPTED:
                return OptimizationStatus::INTERRUPTED;
            default:
                return OptimizationStatus::ERROR;
        }
    }
};

} // namespace path_based
} // namespace ocst

#endif // OCST_PATH_BASED_FORMULATION_SOLVER_H

