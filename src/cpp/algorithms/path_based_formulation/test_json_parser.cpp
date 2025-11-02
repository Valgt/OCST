#include <iostream>
#include <string>
#include <exception>

// Include the instance loader
#include "include/instance_loader.h"

/**
 * Test program to validate JSON instance parsing
 * 
 * Usage:
 *   test_json_parser <instance_file.json>
 * 
 * This program:
 * 1. Loads a JSON instance using the unified loader
 * 2. Validates the instance structure
 * 3. Prints summary statistics
 * 4. Can be used for debugging before integration into algorithm.cpp
 */

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <instance_file.json>" << std::endl;
        std::cerr << "Example: " << argv[0] << " ../../../../data/input/ocstpin0.json" << std::endl;
        return 1;
    }
    
    std::string filename = argv[1];
    
    try {
        std::cout << "Testing JSON instance parser..." << std::endl;
        std::cout << "Loading instance: " << filename << std::endl;
        
        // Load instance (JSON format only)
        ocst::path_based::OCSTInstance instance = ocst::path_based::load_instance(filename);
        
        // Print instance statistics
        std::cout << "\n=== Instance Statistics ===" << std::endl;
        std::cout << "Nodes: " << instance.num_nodes << std::endl;
        std::cout << "Edges: " << instance.num_edges << std::endl;
        std::cout << "Requirements: " << instance.requirements.size() << std::endl;
        std::cout << "Probability: " << instance.probability << std::endl;
        
        // Validate instance
        if (instance.validate()) {
            std::cout << "\n✓ Instance validation: PASSED" << std::endl;
        } else {
            std::cout << "\n✗ Instance validation: FAILED" << std::endl;
            return 1;
        }
        
        // Print sample edges (first 5)
        std::cout << "\n=== Sample Edges (first 5) ===" << std::endl;
        int edge_count = 0;
        for (const auto& edge : instance.edges) {
            if (edge_count++ >= 5) break;
            std::cout << "  Edge " << (edge_count - 1) << ": (" << edge.source 
                      << ", " << edge.destination << ") cost=" << edge.cost << std::endl;
        }
        
        // Print sample requirements (first 5)
        std::cout << "\n=== Sample Requirements (first 5) ===" << std::endl;
        int req_count = 0;
        for (const auto& req : instance.requirements) {
            if (req_count++ >= 5) break;
            std::cout << "  Req " << (req_count - 1) << ": (" << req.origin 
                      << ", " << req.destination << ") weight=" << req.weight << std::endl;
        }
        
        std::cout << "\n✓ JSON parsing successful!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Error: " << e.what() << std::endl;
        return 1;
    }
}

