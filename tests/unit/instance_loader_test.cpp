/**
 * @file instance_loader_test.cpp
 * @brief Unit tests for instance_loader.h (JSON parsing)
 * 
 * Tests cover:
 * - Valid JSON instance loading
 * - Schema validation (version check)
 * - Required field validation
 * - Node index validation (0 <= i < n)
 * - Non-negative cost/weight validation
 * - Error handling and descriptive messages
 * - O(n + m) complexity guarantee
 */

#include <gtest/gtest.h>
#include "instance_loader.h"
#include <fstream>
#include <filesystem>

using namespace ocst::path_based;
namespace fs = std::filesystem;

// ============================================================================
// Test Fixture with Temp Directory
// ============================================================================

class InstanceLoaderTest : public ::testing::Test {
protected:
    std::string test_dir;
    
    void SetUp() override {
        test_dir = "/tmp/ocst_test_instances_" + std::to_string(getpid());
        fs::create_directories(test_dir);
    }
    
    void TearDown() override {
        fs::remove_all(test_dir);
    }
    
    std::string write_json_file(const std::string& filename, const std::string& content) {
        std::string filepath = test_dir + "/" + filename;
        std::ofstream file(filepath);
        file << content;
        file.close();
        return filepath;
    }
};

// ============================================================================
// Valid Instance Tests
// ============================================================================

TEST_F(InstanceLoaderTest, LoadValidMinimalInstance) {
    std::string json = R"({
        "schema_version": "1.0",
        "name": "test_minimal",
        "tags": ["test"],
        "graph": {
            "nodes": 3,
            "edges": [
                {"source": 0, "destination": 1, "cost": 5.0},
                {"source": 1, "destination": 2, "cost": 7.0}
            ]
        },
        "requirements": [
            {"origin": 0, "destination": 2, "weight": 10.0}
        ]
    })";
    
    std::string filepath = write_json_file("minimal.json", json);
    OCSTInstance instance = load_instance(filepath);
    
    EXPECT_EQ(instance.num_nodes, 3);
    EXPECT_EQ(instance.num_edges, 2);
    EXPECT_EQ(instance.edges.size(), 2);
    EXPECT_EQ(instance.requirements.size(), 1);
    
    // Verify edges
    EXPECT_EQ(instance.edges[0].source, 0);
    EXPECT_EQ(instance.edges[0].destination, 1);
    EXPECT_DOUBLE_EQ(instance.edges[0].cost, 5.0);
    
    // Verify requirements
    EXPECT_EQ(instance.requirements[0].origin, 0);
    EXPECT_EQ(instance.requirements[0].destination, 2);
    EXPECT_DOUBLE_EQ(instance.requirements[0].weight, 10.0);
    
    // Verify adjacency structures
    EXPECT_TRUE(instance.has_edge(0, 1));
    EXPECT_TRUE(instance.has_edge(1, 2));
    EXPECT_FALSE(instance.has_edge(0, 2));
}

TEST_F(InstanceLoaderTest, LoadValidCompleteInstance) {
    std::string json = R"({
        "schema_version": "1.0",
        "name": "test_complete",
        "tags": ["test", "quick_check"],
        "provenance": {
            "source": "unit_test",
            "generator": "manual",
            "date": "2025-11-10"
        },
        "graph": {
            "nodes": 4,
            "edges": [
                {"source": 0, "destination": 1, "cost": 1.0},
                {"source": 1, "destination": 2, "cost": 2.0},
                {"source": 2, "destination": 3, "cost": 3.0},
                {"source": 0, "destination": 3, "cost": 4.0}
            ]
        },
        "requirements": [
            {"origin": 0, "destination": 3, "weight": 5.0},
            {"origin": 1, "destination": 2, "weight": 3.0}
        ],
        "metadata": {
            "description": "Test instance",
            "optimal_value": 15.0
        }
    })";
    
    std::string filepath = write_json_file("complete.json", json);
    OCSTInstance instance = load_instance(filepath);
    
    EXPECT_EQ(instance.num_nodes, 4);
    EXPECT_EQ(instance.num_edges, 4);
    EXPECT_EQ(instance.edges.size(), 4);
    EXPECT_EQ(instance.requirements.size(), 2);
    
    // Validate
    auto [valid, message] = instance.validate();
    EXPECT_TRUE(valid) << message;
}

TEST_F(InstanceLoaderTest, LoadEmptyRequirements) {
    std::string json = R"({
        "schema_version": "1.0",
        "name": "test_no_reqs",
        "tags": [],
        "graph": {
            "nodes": 3,
            "edges": [
                {"source": 0, "destination": 1, "cost": 5.0}
            ]
        },
        "requirements": []
    })";
    
    std::string filepath = write_json_file("no_reqs.json", json);
    OCSTInstance instance = load_instance(filepath);
    
    EXPECT_EQ(instance.num_nodes, 3);
    EXPECT_EQ(instance.num_edges, 1);
    EXPECT_TRUE(instance.requirements.empty());
}

// ============================================================================
// Schema Validation Tests
// ============================================================================

TEST_F(InstanceLoaderTest, MissingSchemaVersion) {
    std::string json = R"({
        "name": "test",
        "graph": {"nodes": 3, "edges": []},
        "requirements": []
    })";
    
    std::string filepath = write_json_file("no_schema.json", json);
    EXPECT_THROW({
        load_instance(filepath);
    }, std::runtime_error);
}

TEST_F(InstanceLoaderTest, WrongSchemaVersion) {
    std::string json = R"({
        "schema_version": "2.0",
        "name": "test",
        "graph": {"nodes": 3, "edges": []},
        "requirements": []
    })";
    
    std::string filepath = write_json_file("wrong_schema.json", json);
    
    try {
        load_instance(filepath);
        FAIL() << "Expected runtime_error for wrong schema version";
    } catch (const std::runtime_error& e) {
        std::string error_msg = e.what();
        EXPECT_NE(error_msg.find("Unsupported schema version"), std::string::npos);
        EXPECT_NE(error_msg.find("2.0"), std::string::npos);
    }
}

// ============================================================================
// Required Field Validation Tests
// ============================================================================

TEST_F(InstanceLoaderTest, MissingGraphField) {
    std::string json = R"({
        "schema_version": "1.0",
        "name": "test",
        "requirements": []
    })";
    
    std::string filepath = write_json_file("no_graph.json", json);
    
    try {
        load_instance(filepath);
        FAIL() << "Expected runtime_error for missing graph";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("graph"), std::string::npos);
    }
}

TEST_F(InstanceLoaderTest, MissingRequirementsField) {
    std::string json = R"({
        "schema_version": "1.0",
        "name": "test",
        "graph": {"nodes": 3, "edges": []}
    })";
    
    std::string filepath = write_json_file("no_requirements.json", json);
    
    try {
        load_instance(filepath);
        FAIL() << "Expected runtime_error for missing requirements";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("requirements"), std::string::npos);
    }
}

TEST_F(InstanceLoaderTest, MissingNodesField) {
    std::string json = R"({
        "schema_version": "1.0",
        "name": "test",
        "graph": {"edges": []},
        "requirements": []
    })";
    
    std::string filepath = write_json_file("no_nodes.json", json);
    
    try {
        load_instance(filepath);
        FAIL() << "Expected runtime_error for missing nodes";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("nodes"), std::string::npos);
    }
}

TEST_F(InstanceLoaderTest, MissingEdgesField) {
    std::string json = R"({
        "schema_version": "1.0",
        "name": "test",
        "graph": {"nodes": 3},
        "requirements": []
    })";
    
    std::string filepath = write_json_file("no_edges.json", json);
    
    try {
        load_instance(filepath);
        FAIL() << "Expected runtime_error for missing edges";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("edges"), std::string::npos);
    }
}

// ============================================================================
// Node Index Validation Tests
// ============================================================================

TEST_F(InstanceLoaderTest, EdgeWithNegativeSource) {
    std::string json = R"({
        "schema_version": "1.0",
        "name": "test",
        "graph": {
            "nodes": 3,
            "edges": [{"source": -1, "destination": 1, "cost": 5.0}]
        },
        "requirements": []
    })";
    
    std::string filepath = write_json_file("negative_source.json", json);
    
    try {
        load_instance(filepath);
        FAIL() << "Expected runtime_error for negative source";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("out of range"), std::string::npos);
    }
}

TEST_F(InstanceLoaderTest, EdgeWithSourceOutOfBounds) {
    std::string json = R"({
        "schema_version": "1.0",
        "name": "test",
        "graph": {
            "nodes": 3,
            "edges": [{"source": 3, "destination": 1, "cost": 5.0}]
        },
        "requirements": []
    })";
    
    std::string filepath = write_json_file("source_oob.json", json);
    
    try {
        load_instance(filepath);
        FAIL() << "Expected runtime_error for source out of bounds";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("out of range"), std::string::npos);
    }
}

TEST_F(InstanceLoaderTest, RequirementWithInvalidNode) {
    std::string json = R"({
        "schema_version": "1.0",
        "name": "test",
        "graph": {
            "nodes": 3,
            "edges": []
        },
        "requirements": [{"origin": 0, "destination": 5, "weight": 10.0}]
    })";
    
    std::string filepath = write_json_file("req_invalid_node.json", json);
    
    try {
        load_instance(filepath);
        FAIL() << "Expected runtime_error for requirement node out of bounds";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("out of range"), std::string::npos);
    }
}

// ============================================================================
// Cost/Weight Validation Tests
// ============================================================================

TEST_F(InstanceLoaderTest, EdgeWithNegativeCost) {
    std::string json = R"({
        "schema_version": "1.0",
        "name": "test",
        "graph": {
            "nodes": 3,
            "edges": [{"source": 0, "destination": 1, "cost": -5.0}]
        },
        "requirements": []
    })";
    
    std::string filepath = write_json_file("negative_cost.json", json);
    
    try {
        load_instance(filepath);
        FAIL() << "Expected runtime_error for negative cost";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("negative cost"), std::string::npos);
    }
}

TEST_F(InstanceLoaderTest, RequirementWithNegativeWeight) {
    std::string json = R"({
        "schema_version": "1.0",
        "name": "test",
        "graph": {
            "nodes": 3,
            "edges": []
        },
        "requirements": [{"origin": 0, "destination": 2, "weight": -10.0}]
    })";
    
    std::string filepath = write_json_file("negative_weight.json", json);
    
    try {
        load_instance(filepath);
        FAIL() << "Expected runtime_error for negative weight";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("negative weight"), std::string::npos);
    }
}

// ============================================================================
// File Error Tests
// ============================================================================

TEST_F(InstanceLoaderTest, NonexistentFile) {
    EXPECT_THROW({
        load_instance("/nonexistent/path/file.json");
    }, std::runtime_error);
}

TEST_F(InstanceLoaderTest, InvalidJSON) {
    std::string invalid_json = "{ this is not valid JSON }";
    std::string filepath = write_json_file("invalid.json", invalid_json);
    
    try {
        load_instance(filepath);
        FAIL() << "Expected runtime_error for invalid JSON";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("Invalid JSON"), std::string::npos);
    }
}

// ============================================================================
// Performance / Complexity Tests
// ============================================================================

TEST_F(InstanceLoaderTest, LargeInstanceComplexity) {
    // Generate a moderately large instance to verify O(n + m) behavior
    // 100 nodes, 99 edges (path graph), 50 requirements
    
    std::ostringstream json;
    json << R"({
        "schema_version": "1.0",
        "name": "large_test",
        "tags": ["performance"],
        "graph": {
            "nodes": 100,
            "edges": [)";
    
    // Add 99 edges forming a simple path: 0-1-2-...-99
    for (int i = 0; i < 99; ++i) {
        if (i > 0) json << ",";
        json << "{\"source\": " << i 
             << ", \"destination\": " << (i + 1) 
             << ", \"cost\": " << (i + 1.0) << "}";
    }
    
    json << R"(]
        },
        "requirements": [)";
    
    // Add 50 requirements
    for (int i = 0; i < 50; ++i) {
        if (i > 0) json << ",";
        json << "{\"origin\": " << (i * 2 % 100)
             << ", \"destination\": " << ((i * 2 + 1) % 100)
             << ", \"weight\": " << (i + 1.0) << "}";
    }
    
    json << "]}";
    
    std::string filepath = write_json_file("large.json", json.str());
    
    auto start = std::chrono::high_resolution_clock::now();
    OCSTInstance instance = load_instance(filepath);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_EQ(instance.num_nodes, 100);
    EXPECT_EQ(instance.num_edges, 99);
    EXPECT_EQ(instance.requirements.size(), 50);
    
    // Loading should be fast (< 100ms for this size)
    EXPECT_LT(duration.count(), 100) << "Loading took " << duration.count() << "ms";
}

// Google Test's main() is provided by gtest_main.o

