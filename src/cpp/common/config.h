#ifndef OCST_COMMON_CONFIG_H
#define OCST_COMMON_CONFIG_H

#include <string>
#include <map>
#include <optional>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <iostream>
#include <fstream>

namespace ocst::common {

/**
 * @brief Default configuration values for OCST solvers
 */
struct ConfigDefaults {
    static constexpr double TIME_LIMIT = 3600.0;        // 1 hour
    static constexpr double TOLERANCE = 1e-6;           // Numerical tolerance
    static constexpr double MIP_GAP = 0.0;              // MIP gap (0 = optimal)
    static constexpr int THREADS = 1;                   // Number of threads
    static constexpr int CALLBACK_FREQUENCY = 1;        // Callback frequency
    static constexpr bool PRESOLVE = true;              // Enable presolve
    static constexpr bool OUTPUT_FLAG = false;          // Solver output
    static constexpr const char* BRANCHING_STRATEGY = "strong";        // Default branching
};


/**
 * @brief Configuration loader for OCST solvers
 *
 * Loads solver configuration from JSON files following config.schema.v1.json
 */
class Config {
public:
    // Constructor
    Config() = default;

    /**
     * @brief Load configuration from JSON file
     * @param filepath Path to JSON configuration file
     * @return true if loaded successfully
     * @throws std::runtime_error if file cannot be read or JSON is invalid
     */
    bool load_from_file(const std::string& filepath);

    /**
     * @brief Load configuration from JSON object
     * @param json_config JSON configuration object
     * @return true if loaded successfully
     */
    bool load_from_json(const nlohmann::json& json_config);

    // Getter methods with defaults
    double get_time_limit() const { return time_limit_.value_or(ConfigDefaults::TIME_LIMIT); }
    double get_tolerance() const { return tolerance_.value_or(ConfigDefaults::TOLERANCE); }
    double get_mip_gap() const { return mip_gap_.value_or(ConfigDefaults::MIP_GAP); }
    int get_threads() const { return threads_.value_or(ConfigDefaults::THREADS); }
    int get_callback_frequency() const { return callback_frequency_.value_or(ConfigDefaults::CALLBACK_FREQUENCY); }
    bool get_presolve() const { return presolve_.value_or(ConfigDefaults::PRESOLVE); }
    bool get_output_flag() const { return output_flag_.value_or(ConfigDefaults::OUTPUT_FLAG); }
    std::string get_branching_strategy() const { return branching_strategy_.value_or(ConfigDefaults::BRANCHING_STRATEGY); }

    /**
     * @brief Get all configuration parameters as a map
     * @return Map of parameter names to string values
     */
    std::map<std::string, std::string> get_all_params() const;

    /**
     * @brief Validate configuration against schema
     * @return true if valid
     */
    bool validate() const;

private:
    // Optional configuration parameters
    std::optional<double> time_limit_;
    std::optional<double> tolerance_;
    std::optional<double> mip_gap_;
    std::optional<int> threads_;
    std::optional<int> callback_frequency_;
    std::optional<bool> presolve_;
    std::optional<bool> output_flag_;
    std::optional<std::string> branching_strategy_;

    // Helper method to parse optional values
    template<typename T>
    void parse_optional(const nlohmann::json& json, const std::string& key, std::optional<T>& target);
};

// Template specializations for parsing
template<>
void Config::parse_optional<double>(const nlohmann::json& json, const std::string& key, std::optional<double>& target);

template<>
void Config::parse_optional<int>(const nlohmann::json& json, const std::string& key, std::optional<int>& target);

template<>
void Config::parse_optional<bool>(const nlohmann::json& json, const std::string& key, std::optional<bool>& target);

template<>
void Config::parse_optional<std::string>(const nlohmann::json& json, const std::string& key, std::optional<std::string>& target);

} // namespace ocst::common

#endif // OCST_COMMON_CONFIG_H
