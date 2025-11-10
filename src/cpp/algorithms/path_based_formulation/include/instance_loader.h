#ifndef OCST_PATH_BASED_INSTANCE_LOADER_H
#define OCST_PATH_BASED_INSTANCE_LOADER_H

#include "common_types.h"
#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>

// Include nlohmann::json for JSON parsing
// Note: Expects -Ithird_party compiler flag
#include <nlohmann/json.hpp>

namespace ocst {
namespace path_based {

/**
 * @file instance_loader.h
 * @brief JSON instance loader for path_based_formulation
 * 
 * This loader parses JSON instances following instance.schema.v1.json.
 * Maintains O(n + m) time complexity.
 * 
 * Note: Legacy .ocstpin format is supported only in path_based_formulation_original.
 */

/**
 * @brief Parse JSON instance format
 * 
 * Parses JSON maintaining O(n + m) complexity by streaming over arrays.
 * Follows instance.schema.v1.json specification.
 * 
 * @param filename Path to JSON instance file
 * @return Parsed OCSTInstance
 * @throws std::runtime_error on parse errors
 * 
 * Time complexity: O(n + m)
 */
OCSTInstance load_instance(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    
    nlohmann::json json_data;
    try {
        file >> json_data;
        file.close();
    } catch (const nlohmann::json::exception& e) {
        file.close();
        throw std::runtime_error("Invalid JSON format in " + filename + ": " + std::string(e.what()));
    } catch (const std::exception& e) {
        file.close();
        throw std::runtime_error("Error reading JSON file " + filename + ": " + std::string(e.what()));
    }
    
    // Validate schema_version (must be "1.0")
    if (!json_data.contains("schema_version")) {
        throw std::runtime_error("Missing 'schema_version' field");
    }
    std::string schema_version = json_data["schema_version"].get<std::string>();
    if (schema_version != "1.0") {
        throw std::runtime_error("Unsupported schema version: " + schema_version + " (expected 1.0)");
    }
    
    // Validate required fields
    if (!json_data.contains("graph")) {
        throw std::runtime_error("Missing required field: 'graph'");
    }
    if (!json_data.contains("requirements")) {
        throw std::runtime_error("Missing required field: 'requirements'");
    }
    
    const auto& graph = json_data["graph"];
    if (!graph.contains("nodes")) {
        throw std::runtime_error("Missing required graph field: 'nodes'");
    }
    if (!graph.contains("edges")) {
        throw std::runtime_error("Missing required graph field: 'edges'");
    }
    
    // Extract graph data
    int n = graph["nodes"].get<int>();
    if (n <= 0) {
        throw std::runtime_error("Invalid number of nodes: " + std::to_string(n) + " (must be > 0)");
    }
    
    // Extract probability from metadata (default 0.0 if not present)
    double probability = 0.0;
    if (json_data.contains("metadata") && json_data["metadata"].is_object()) {
        const auto& metadata = json_data["metadata"];
        if (metadata.contains("probability") && metadata["probability"].is_number()) {
            probability = metadata["probability"].get<double>();
        }
    }
    
    OCSTInstance instance(n, probability);
    
    // Parse edges: O(m)
    const auto& edges_array = graph["edges"];
    if (!edges_array.is_array()) {
        throw std::runtime_error("'edges' must be an array");
    }
    
    for (size_t i = 0; i < edges_array.size(); ++i) {
        const auto& edge_obj = edges_array[i];
        if (!edge_obj.is_object()) {
            throw std::runtime_error("Edge at index " + std::to_string(i) + " must be an object");
        }
        if (!edge_obj.contains("source") || !edge_obj.contains("destination") || !edge_obj.contains("cost")) {
            throw std::runtime_error("Edge at index " + std::to_string(i) + " missing required fields");
        }
        
        int source = edge_obj["source"].get<int>();
        int destination = edge_obj["destination"].get<int>();
        double cost = edge_obj["cost"].get<double>();
        
        if (source < 0 || source >= n || destination < 0 || destination >= n) {
            throw std::runtime_error("Edge at index " + std::to_string(i) + " has node indices out of range");
        }
        if (cost < 0.0) {
            throw std::runtime_error("Edge at index " + std::to_string(i) + " has negative cost");
        }
        
        instance.add_edge(source, destination, cost);
    }
    
    // Parse requirements: O(|requirements|)
    const auto& requirements_array = json_data["requirements"];
    if (!requirements_array.is_array()) {
        throw std::runtime_error("'requirements' must be an array");
    }
    
    for (size_t i = 0; i < requirements_array.size(); ++i) {
        const auto& req_obj = requirements_array[i];
        if (!req_obj.is_object()) {
            throw std::runtime_error("Requirement at index " + std::to_string(i) + " must be an object");
        }
        if (!req_obj.contains("origin") || !req_obj.contains("destination") || !req_obj.contains("weight")) {
            throw std::runtime_error("Requirement at index " + std::to_string(i) + " missing required fields");
        }
        
        int origin = req_obj["origin"].get<int>();
        int destination = req_obj["destination"].get<int>();
        double weight = req_obj["weight"].get<double>();
        
        if (origin < 0 || origin >= n || destination < 0 || destination >= n) {
            throw std::runtime_error("Requirement at index " + std::to_string(i) + " has node indices out of range");
        }
        if (weight < 0.0) {
            throw std::runtime_error("Requirement at index " + std::to_string(i) + " has negative weight");
        }
        
        instance.add_requirement(origin, destination, weight);
    }
    
    // Validate instance consistency
    auto [is_valid, error_message] = instance.validate();
    if (!is_valid) {
        throw std::runtime_error("Invalid instance structure after parsing JSON: " + error_message);
    }
    
    return instance;
}

} // namespace path_based
} // namespace ocst

#endif // OCST_PATH_BASED_INSTANCE_LOADER_H

