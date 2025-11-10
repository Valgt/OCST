#include "structured_logger.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iostream>

namespace ocst::common {

StructuredLogger::StructuredLogger(const std::string& base_filename, size_t max_file_size_mb)
    : base_filename_(base_filename)
    , max_file_size_bytes_(max_file_size_mb * 1024 * 1024)
    , file_count_(0)
    , current_file_size_(0) {
    // Create logs directory if it doesn't exist
    std::filesystem::create_directories(std::filesystem::path(base_filename_).parent_path());

    // Open first log file
    open_new_file(0);
}

StructuredLogger::~StructuredLogger() {
    finalize(true); // Compress by default on destruction
}

void StructuredLogger::log_branching_event(const BranchingEvent& event) {
    nlohmann::json json_event = {
        {"timestamp", timestamp_to_string(event.timestamp)},
        {"event_type", "branching_decision"},
        {"node_id", event.node_id},
        {"variable_index", event.variable_index},
        {"variable_value", event.variable_value},
        {"branch_direction", event.branch_direction},
        {"bound_value", event.bound_value}
    };

    write_json_line(json_event);
}

void StructuredLogger::log_lazy_cut(const LazyCutEvent& event) {
    nlohmann::json json_event = {
        {"timestamp", timestamp_to_string(event.timestamp)},
        {"event_type", "lazy_cut_added"},
        {"cut_id", event.cut_id},
        {"violated_constraints", event.violated_constraints},
        {"violation_amount", event.violation_amount},
        {"cut_type", event.cut_type}
    };

    write_json_line(json_event);
}

void StructuredLogger::log_callback_stats(const CallbackStats& event) {
    nlohmann::json json_event = {
        {"timestamp", timestamp_to_string(event.timestamp)},
        {"event_type", "callback_stats"},
        {"current_node", event.current_node},
        {"current_objective", event.current_objective},
        {"best_bound", event.best_bound},
        {"cuts_added", event.cuts_added},
        {"gap_percentage", event.gap_percentage}
    };

    write_json_line(json_event);
}

void StructuredLogger::rotate_if_needed() {
    if (current_file_size_ >= max_file_size_bytes_) {
        rotate_file();
    }
}

void StructuredLogger::rotate_file() {
    if (current_file_ && current_file_->is_open()) {
        current_file_->close();
    }

    open_new_file(file_count_);
}

void StructuredLogger::finalize(bool compress) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (current_file_ && current_file_->is_open()) {
        current_file_->close();
        current_file_.reset();
    }

    if (compress && file_count_ > 0) {
        // Compress all log files
        for (size_t i = 0; i < file_count_; ++i) {
            std::string filepath = base_filename_ + "." + std::to_string(i) + ".jsonl";
            if (std::filesystem::exists(filepath)) {
                gzip_file(filepath);
            }
        }
    }
}

size_t StructuredLogger::get_current_file_size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_file_size_;
}

void StructuredLogger::open_new_file(size_t file_index) {
    std::lock_guard<std::mutex> lock(mutex_);

    current_filepath_ = base_filename_ + "." + std::to_string(file_index) + ".jsonl";
    current_file_ = std::make_unique<std::ofstream>(current_filepath_, std::ios::out | std::ios::trunc);

    if (!current_file_->is_open()) {
        throw std::runtime_error("Cannot open log file: " + current_filepath_);
    }

    current_file_size_ = 0;
    file_count_ = std::max(file_count_, file_index + 1);
}

void StructuredLogger::write_json_line(const nlohmann::json& json_obj) {
    std::lock_guard<std::mutex> lock(mutex_);

    rotate_if_needed();

    if (!current_file_ || !current_file_->is_open()) {
        return; // Silently fail if no file is open
    }

    std::string json_line = json_obj.dump() + "\n";
    current_file_->write(json_line.c_str(), json_line.size());
    current_file_->flush();

    current_file_size_ += json_line.size();
}

void StructuredLogger::gzip_file(const std::string& filepath) {
    // Note: This is a placeholder for gzip compression
    // In a full implementation, you would use zlib or similar
    // For now, just rename the file to indicate it's ready for compression
    try {
        std::string gz_filepath = filepath + ".gz";
        std::filesystem::rename(filepath, gz_filepath);
        std::cout << "File ready for compression: " << gz_filepath << std::endl;
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Warning: Could not prepare file for compression: " << e.what() << std::endl;
    }
}

std::string StructuredLogger::timestamp_to_string(std::chrono::system_clock::time_point tp) const {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(
        tp.time_since_epoch() % std::chrono::seconds(1)).count();

    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S")
       << "." << std::setfill('0') << std::setw(6) << microseconds << "Z";
    return ss.str();
}

} // namespace ocst::common
