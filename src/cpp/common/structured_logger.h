#ifndef OCST_COMMON_STRUCTURED_LOGGER_H
#define OCST_COMMON_STRUCTURED_LOGGER_H

#include <string>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <chrono>
#include <mutex>

namespace ocst::common {

/**
 * @brief Event types for structured logging
 */
enum class LogEventType {
    BRANCHING_DECISION,
    LAZY_CUT_ADDED,
    CALLBACK_STATS,
    SOLVER_STATUS,
    ITERATION_COMPLETE
};

/**
 * @brief Branching decision event data
 */
struct BranchingEvent {
    int node_id;
    int variable_index;
    double variable_value;
    std::string branch_direction; // "up" or "down"
    double bound_value;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Lazy cut event data
 */
struct LazyCutEvent {
    int cut_id;
    int violated_constraints;
    double violation_amount;
    std::string cut_type;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Callback statistics event data
 */
struct CallbackStats {
    int current_node;
    double current_objective;
    double best_bound;
    int cuts_added;
    double gap_percentage;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Warm start lifecycle event data
 */
struct WarmStartEvent {
    std::string idea;
    std::string status;  // start | success | failure
    double duration_ms = 0.0;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Structured logger for OCST solvers
 *
 * Emits JSON Lines format for downstream analysis and visualization.
 * Supports automatic rotation after 128MB and optional gzip compression.
 */
class StructuredLogger {
public:
    /**
     * @brief Constructor
     * @param base_filename Base filename for log files (will append .jsonl)
     * @param max_file_size_mb Maximum file size before rotation (default: 128MB)
     */
    StructuredLogger(const std::string& base_filename, size_t max_file_size_mb = 128);

    /**
     * @brief Destructor - ensures proper file closure
     */
    ~StructuredLogger();

    // Delete copy/move operations
    StructuredLogger(const StructuredLogger&) = delete;
    StructuredLogger& operator=(const StructuredLogger&) = delete;
    StructuredLogger(StructuredLogger&&) = delete;
    StructuredLogger& operator=(StructuredLogger&&) = delete;

    /**
     * @brief Log a branching decision event
     */
    void log_branching_event(const BranchingEvent& event);

    /**
     * @brief Log a lazy cut addition event
     */
    void log_lazy_cut(const LazyCutEvent& event);

    /**
     * @brief Log callback statistics
     */
    void log_callback_stats(const CallbackStats& event);

    /**
     * @brief Log warm start lifecycle events
     */
    void log_warm_start_event(const WarmStartEvent& event);

    /**
     * @brief Check if file needs rotation and rotate if necessary
     */
    void rotate_if_needed();

    /**
     * @brief Force file rotation
     */
    void rotate_file();

    /**
     * @brief Close current log file and prepare for gzip compression
     * @param compress Whether to gzip the closed file
     */
    void finalize(bool compress = false);

    /**
     * @brief Get current file size in bytes
     */
    size_t get_current_file_size() const;

    /**
     * @brief Get number of log files created (including current)
     */
    size_t get_file_count() const { return file_count_; }

private:
    /**
     * @brief Open a new log file
     * @param file_index Index for the new file
     */
    void open_new_file(size_t file_index);

    /**
     * @brief Write a JSON object as a line to the current file
     */
    void write_json_line(const nlohmann::json& json_obj);

    /**
     * @brief Compress a file using gzip
     */
    void gzip_file(const std::string& filepath);

    /**
     * @brief Convert timestamp to ISO 8601 string
     */
    std::string timestamp_to_string(std::chrono::system_clock::time_point tp) const;

    // Configuration
    std::string base_filename_;
    size_t max_file_size_bytes_;

    // File management
    std::unique_ptr<std::ofstream> current_file_;
    std::string current_filepath_;
    size_t file_count_;
    size_t current_file_size_;

    // Thread safety
    mutable std::mutex mutex_;

    // Constants
    static constexpr size_t BUFFER_SIZE = 8192;
};

} // namespace ocst::common

#endif // OCST_COMMON_STRUCTURED_LOGGER_H
