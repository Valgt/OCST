#include "config.h"
#include <filesystem>

namespace ocst::common {

bool Config::load_from_file(const std::string& filepath) {
    if (!std::filesystem::exists(filepath)) {
        throw std::runtime_error("Configuration file does not exist: " + filepath);
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open configuration file: " + filepath);
    }

    nlohmann::json json_config;
    try {
        file >> json_config;
    } catch (const std::exception& e) {
        throw std::runtime_error("Invalid JSON in configuration file: " + std::string(e.what()));
    }

    return load_from_json(json_config);
}

bool Config::load_from_json(const nlohmann::json& json_config) {
    try {
        // Parse optional parameters
        parse_optional(json_config, "time_limit", time_limit_);
        parse_optional(json_config, "tolerance", tolerance_);
        parse_optional(json_config, "mip_gap", mip_gap_);
        parse_optional(json_config, "threads", threads_);
        parse_optional(json_config, "callback_frequency", callback_frequency_);
        parse_optional(json_config, "presolve", presolve_);
        parse_optional(json_config, "output_flag", output_flag_);
        parse_optional(json_config, "branching_strategy", branching_strategy_);

        return validate();
    } catch (const std::exception& e) {
        std::cerr << "Error parsing configuration: " << e.what() << std::endl;
        return false;
    }
}

std::map<std::string, std::string> Config::get_all_params() const {
    std::map<std::string, std::string> params;

    if (time_limit_) params["time_limit"] = std::to_string(*time_limit_);
    if (tolerance_) params["tolerance"] = std::to_string(*tolerance_);
    if (mip_gap_) params["mip_gap"] = std::to_string(*mip_gap_);
    if (threads_) params["threads"] = std::to_string(*threads_);
    if (callback_frequency_) params["callback_frequency"] = std::to_string(*callback_frequency_);
    if (presolve_) params["presolve"] = *presolve_ ? "true" : "false";
    if (output_flag_) params["output_flag"] = *output_flag_ ? "true" : "false";
    if (branching_strategy_) params["branching_strategy"] = *branching_strategy_;

    return params;
}

bool Config::validate() const {
    // Basic validation - check ranges
    if (time_limit_ && *time_limit_ <= 0) {
        std::cerr << "Error: time_limit must be positive" << std::endl;
        return false;
    }

    if (tolerance_ && (*tolerance_ < 0 || *tolerance_ > 1)) {
        std::cerr << "Error: tolerance must be between 0 and 1" << std::endl;
        return false;
    }

    if (mip_gap_ && (*mip_gap_ < 0 || *mip_gap_ > 1)) {
        std::cerr << "Error: mip_gap must be between 0 and 1" << std::endl;
        return false;
    }

    if (threads_ && *threads_ <= 0) {
        std::cerr << "Error: threads must be positive" << std::endl;
        return false;
    }

    if (callback_frequency_ && *callback_frequency_ <= 0) {
        std::cerr << "Error: callback_frequency must be positive" << std::endl;
        return false;
    }

    // Validate branching strategy
    if (branching_strategy_) {
        const std::vector<std::string> valid_strategies = {"strong", "pseudocost", "most_fractional"};
        if (std::find(valid_strategies.begin(), valid_strategies.end(), *branching_strategy_) == valid_strategies.end()) {
            std::cerr << "Error: invalid branching_strategy. Must be one of: strong, pseudocost, most_fractional" << std::endl;
            return false;
        }
    }

    return true;
}

// Template specializations
template<>
void Config::parse_optional<double>(const nlohmann::json& json, const std::string& key, std::optional<double>& target) {
    if (json.contains(key) && !json[key].is_null()) {
        target = json[key].get<double>();
    }
}

template<>
void Config::parse_optional<int>(const nlohmann::json& json, const std::string& key, std::optional<int>& target) {
    if (json.contains(key) && !json[key].is_null()) {
        target = json[key].get<int>();
    }
}

template<>
void Config::parse_optional<bool>(const nlohmann::json& json, const std::string& key, std::optional<bool>& target) {
    if (json.contains(key) && !json[key].is_null()) {
        target = json[key].get<bool>();
    }
}

template<>
void Config::parse_optional<std::string>(const nlohmann::json& json, const std::string& key, std::optional<std::string>& target) {
    if (json.contains(key) && !json[key].is_null()) {
        target = json[key].get<std::string>();
    }
}

} // namespace ocst::common
