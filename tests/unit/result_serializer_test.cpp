/**
 * @file result_serializer_test.cpp
 * @brief Unit tests for result_serializer.h (JSON serialization)
 * 
 * Tests cover:
 * - ResultPayload validation logic
 * - JSON serialization correctness
 * - File writing with directory creation
 * - Schema compliance with result.schema.v1.json
 * - Edge cases (empty collections, optional fields)
 * - Error handling
 */

#include <gtest/gtest.h>
#include "result_serializer.h"
#include <filesystem>
#include <fstream>

using namespace ocst::path_based;
namespace fs = std::filesystem;

// ============================================================================
// Test Fixture
// ============================================================================

class ResultSerializerTest : public ::testing::Test {
protected:
    std::string test_dir;
    
    void SetUp() override {
        test_dir = "/tmp/ocst_test_results_" + std::to_string(getpid());
        fs::create_directories(test_dir);
    }
    
    void TearDown() override {
        fs::remove_all(test_dir);
    }
    
    ResultPayload create_minimal_payload() {
        ResultPayload payload;
        payload.run_uuid = "test-uuid-1234";
        payload.instance_name = "test_instance";
        payload.solver_id = "path_based_formulation";
        return payload;
    }
    
    ResultPayload create_complete_payload() {
        ResultPayload payload = create_minimal_payload();
        
        payload.schema_version = "1.0";
        payload.instance_tags = {"test", "quick_check"};
        payload.solver_version = "1.0.0";
        payload.formulation = "path_based";
        payload.config_digest = "abc123";
        payload.config_params = {{"time_limit", "3600"}, {"mip_gap", "1e-6"}};
        
        payload.optimization_status_code = OptimizationStatus::OPTIMAL;
        payload.optimization_status_description = "Optimal solution found";
        payload.has_solution = true;
        payload.has_bound = true;
        
        payload.objective = 42.5;
        payload.primal_bound = 42.5;
        payload.dual_bound = 42.5;
        payload.gap = 0.0;
        payload.gap_percent = 0.0;
        payload.best_solution_time = 1.234;
        
        payload.runtime_stats.wall_clock_seconds = 5.67;
        payload.runtime_stats.cpu_seconds = 5.50;
        payload.runtime_stats.solver_nodes = 1234;
        payload.runtime_stats.solver_iterations = 5678;
        payload.runtime_stats.termination_reason = "Optimal";
        
        payload.solution.tree_edges = {
            {0, 1}, {1, 2}, {2, 3}
        };
        payload.solution.tree_cost = 42.5;
        payload.solution.is_spanning_tree = true;
        payload.solution.is_connected = true;
        
        payload.reproducibility.git_commit = "abc1234";
        payload.reproducibility.git_dirty = false;
        payload.reproducibility.seed = 42;
        payload.reproducibility.timestamp = "2025-11-10T05:00:00Z";
        
        payload.log_file_path = "/path/to/log.txt";
        payload.result_file_path = "/path/to/result.json";
        payload.warm_start_path = "/path/to/warmstart.mst";
        
        payload.solver_metadata = {{"cuts_added", "42"}, {"lazy_constraints", "10"}};
        
        return payload;
    }
};

// ============================================================================
// OptimizationStatus Tests
// ============================================================================

TEST(OptimizationStatusTest, ToStringConversion) {
    EXPECT_EQ(optimization_status_to_string(OptimizationStatus::OPTIMAL), "OPTIMAL");
    EXPECT_EQ(optimization_status_to_string(OptimizationStatus::TIME_LIMIT), "TIME_LIMIT");
    EXPECT_EQ(optimization_status_to_string(OptimizationStatus::INFEASIBLE), "INFEASIBLE");
    EXPECT_EQ(optimization_status_to_string(OptimizationStatus::UNBOUNDED), "UNBOUNDED");
    EXPECT_EQ(optimization_status_to_string(OptimizationStatus::SUBOPTIMAL), "SUBOPTIMAL");
    EXPECT_EQ(optimization_status_to_string(OptimizationStatus::INTERRUPTED), "INTERRUPTED");
    EXPECT_EQ(optimization_status_to_string(OptimizationStatus::ERROR), "ERROR");
}

// ============================================================================
// ResultPayload Validation Tests
// ============================================================================

TEST_F(ResultSerializerTest, ValidateMinimalValid) {
    ResultPayload payload = create_minimal_payload();
    auto [valid, message] = payload.validate();
    EXPECT_TRUE(valid) << "Message: " << message;
}

TEST_F(ResultSerializerTest, ValidateCompleteValid) {
    ResultPayload payload = create_complete_payload();
    auto [valid, message] = payload.validate();
    EXPECT_TRUE(valid) << "Message: " << message;
}

TEST_F(ResultSerializerTest, ValidateMissingUUID) {
    ResultPayload payload = create_minimal_payload();
    payload.run_uuid = "";
    
    auto [valid, message] = payload.validate();
    EXPECT_FALSE(valid);
    EXPECT_NE(message.find("run_uuid"), std::string::npos);
}

TEST_F(ResultSerializerTest, ValidateMissingInstanceName) {
    ResultPayload payload = create_minimal_payload();
    payload.instance_name = "";
    
    auto [valid, message] = payload.validate();
    EXPECT_FALSE(valid);
    EXPECT_NE(message.find("instance_name"), std::string::npos);
}

TEST_F(ResultSerializerTest, ValidateMissingSolverId) {
    ResultPayload payload = create_minimal_payload();
    payload.solver_id = "";
    
    auto [valid, message] = payload.validate();
    EXPECT_FALSE(valid);
    EXPECT_NE(message.find("solver_id"), std::string::npos);
}

TEST_F(ResultSerializerTest, ValidateHasSolutionButEmptyEdges) {
    ResultPayload payload = create_minimal_payload();
    payload.has_solution = true;
    payload.solution.tree_edges.clear();
    
    auto [valid, message] = payload.validate();
    EXPECT_FALSE(valid);
    EXPECT_NE(message.find("tree_edges"), std::string::npos);
}

TEST_F(ResultSerializerTest, ValidateNoSolutionButHasEdges) {
    ResultPayload payload = create_minimal_payload();
    payload.has_solution = false;
    payload.solution.tree_edges = {{0, 1}};
    
    auto [valid, message] = payload.validate();
    EXPECT_FALSE(valid);
    EXPECT_NE(message.find("has_solution"), std::string::npos);
}

TEST_F(ResultSerializerTest, ValidateCostObjectiveMismatch) {
    ResultPayload payload = create_minimal_payload();
    payload.has_solution = true;
    payload.objective = 42.0;
    payload.solution.tree_edges = {{0, 1}};
    payload.solution.tree_cost = 50.0;  // Mismatch
    
    auto [valid, message] = payload.validate();
    EXPECT_FALSE(valid);
    EXPECT_NE(message.find("tree_cost"), std::string::npos);
}

TEST_F(ResultSerializerTest, ValidateInfeasibleWithSolution) {
    ResultPayload payload = create_minimal_payload();
    payload.optimization_status_code = OptimizationStatus::INFEASIBLE;
    payload.has_solution = true;
    payload.solution.tree_edges = {{0, 1}};  // Add edges since has_solution is true
    payload.solution.tree_cost = 0.0;
    payload.objective = 0.0;
    
    auto [valid, message] = payload.validate();
    EXPECT_FALSE(valid);
    EXPECT_NE(message.find("INFEASIBLE"), std::string::npos);
}

// ============================================================================
// Serialization Tests - Minimal Payload
// ============================================================================

TEST_F(ResultSerializerTest, SerializeMinimalPayload) {
    ResultPayload payload = create_minimal_payload();
    
    nlohmann::json json = ResultSerializer::to_json(payload);
    
    EXPECT_EQ(json["schema_version"], "1.0");
    EXPECT_EQ(json["run_uuid"], "test-uuid-1234");
    EXPECT_EQ(json["instance"]["name"], "test_instance");
    EXPECT_EQ(json["solver"]["id"], "path_based_formulation");
    EXPECT_TRUE(json.contains("config"));
    EXPECT_TRUE(json.contains("optimization_status"));
    EXPECT_TRUE(json.contains("results"));
    EXPECT_TRUE(json.contains("runtime"));
    EXPECT_TRUE(json.contains("reproducibility"));
}

// ============================================================================
// Serialization Tests - Complete Payload
// ============================================================================

TEST_F(ResultSerializerTest, SerializeCompletePayload) {
    ResultPayload payload = create_complete_payload();
    
    nlohmann::json json = ResultSerializer::to_json(payload);
    
    // Instance
    EXPECT_EQ(json["instance"]["name"], "test_instance");
    EXPECT_EQ(json["instance"]["tags"].size(), 2);
    EXPECT_EQ(json["instance"]["tags"][0], "test");
    
    // Solver
    EXPECT_EQ(json["solver"]["id"], "path_based_formulation");
    EXPECT_EQ(json["solver"]["version"], "1.0.0");
    EXPECT_EQ(json["solver"]["formulation"], "path_based");
    
    // Config
    EXPECT_EQ(json["config"]["digest"], "abc123");
    EXPECT_EQ(json["config"]["time_limit"], "3600");
    EXPECT_EQ(json["config"]["mip_gap"], "1e-6");
    
    // Optimization status
    EXPECT_EQ(json["optimization_status"]["code"], "OPTIMAL");
    EXPECT_EQ(json["optimization_status"]["description"], "Optimal solution found");
    EXPECT_TRUE(json["optimization_status"]["has_solution"]);
    EXPECT_TRUE(json["optimization_status"]["has_bound"]);
    
    // Results
    EXPECT_DOUBLE_EQ(json["results"]["objective"], 42.5);
    EXPECT_DOUBLE_EQ(json["results"]["primal_bound"], 42.5);
    EXPECT_DOUBLE_EQ(json["results"]["dual_bound"], 42.5);
    EXPECT_DOUBLE_EQ(json["results"]["gap"], 0.0);
    EXPECT_DOUBLE_EQ(json["results"]["gap_percent"], 0.0);
    EXPECT_DOUBLE_EQ(json["results"]["best_solution_time"], 1.234);
    
    // Runtime
    EXPECT_DOUBLE_EQ(json["runtime"]["wall_clock_seconds"], 5.67);
    EXPECT_DOUBLE_EQ(json["runtime"]["cpu_seconds"], 5.50);
    EXPECT_EQ(json["runtime"]["solver_nodes"], 1234);
    EXPECT_EQ(json["runtime"]["solver_iterations"], 5678);
    EXPECT_EQ(json["runtime"]["termination_reason"], "Optimal");
    
    // Solution
    EXPECT_TRUE(json.contains("solution"));
    EXPECT_EQ(json["solution"]["tree_edges"].size(), 3);
    EXPECT_EQ(json["solution"]["tree_edges"][0]["source"], 0);
    EXPECT_EQ(json["solution"]["tree_edges"][0]["destination"], 1);
    EXPECT_DOUBLE_EQ(json["solution"]["tree_cost"], 42.5);
    EXPECT_TRUE(json["solution"]["is_spanning_tree"]);
    EXPECT_TRUE(json["solution"]["is_connected"]);
    
    // Reproducibility
    EXPECT_EQ(json["reproducibility"]["git_commit"], "abc1234");
    EXPECT_FALSE(json["reproducibility"]["git_dirty"]);
    EXPECT_EQ(json["reproducibility"]["seed"], 42);
    EXPECT_EQ(json["reproducibility"]["timestamp"], "2025-11-10T05:00:00Z");
    
    // Artifacts
    EXPECT_TRUE(json.contains("artifacts"));
    EXPECT_EQ(json["artifacts"]["log_file"], "/path/to/log.txt");
    EXPECT_EQ(json["artifacts"]["result_file"], "/path/to/result.json");
    EXPECT_EQ(json["artifacts"]["warm_start"], "/path/to/warmstart.mst");
    
    // Solver metadata
    EXPECT_TRUE(json.contains("solver_metadata"));
    EXPECT_EQ(json["solver_metadata"]["cuts_added"], "42");
    EXPECT_EQ(json["solver_metadata"]["lazy_constraints"], "10");
}

// ============================================================================
// Serialization Tests - No Solution
// ============================================================================

TEST_F(ResultSerializerTest, SerializeWithoutSolution) {
    ResultPayload payload = create_minimal_payload();
    payload.optimization_status_code = OptimizationStatus::INFEASIBLE;
    payload.has_solution = false;
    
    nlohmann::json json = ResultSerializer::to_json(payload);
    
    EXPECT_EQ(json["optimization_status"]["code"], "INFEASIBLE");
    EXPECT_FALSE(json["optimization_status"]["has_solution"]);
    EXPECT_FALSE(json.contains("solution"));
}

// ============================================================================
// Serialization Tests - Optional Fields
// ============================================================================

TEST_F(ResultSerializerTest, SerializeWithoutArtifacts) {
    ResultPayload payload = create_minimal_payload();
    payload.log_file_path = "";
    payload.result_file_path = "";
    payload.warm_start_path = "";
    
    nlohmann::json json = ResultSerializer::to_json(payload);
    
    EXPECT_FALSE(json.contains("artifacts"));
}

TEST_F(ResultSerializerTest, SerializeWithoutSolverMetadata) {
    ResultPayload payload = create_minimal_payload();
    payload.solver_metadata.clear();
    
    nlohmann::json json = ResultSerializer::to_json(payload);
    
    EXPECT_FALSE(json.contains("solver_metadata"));
}

TEST_F(ResultSerializerTest, SerializeWithPartialArtifacts) {
    ResultPayload payload = create_minimal_payload();
    payload.log_file_path = "/path/to/log.txt";
    // result_file_path and warm_start_path remain empty
    
    nlohmann::json json = ResultSerializer::to_json(payload);
    
    EXPECT_TRUE(json.contains("artifacts"));
    EXPECT_TRUE(json["artifacts"].contains("log_file"));
    EXPECT_FALSE(json["artifacts"].contains("result_file"));
    EXPECT_FALSE(json["artifacts"].contains("warm_start"));
}

// ============================================================================
// File Writing Tests
// ============================================================================

TEST_F(ResultSerializerTest, WriteToFilePretty) {
    ResultPayload payload = create_complete_payload();
    std::string filepath = test_dir + "/result_pretty.json";
    
    EXPECT_NO_THROW(ResultSerializer::write_to_file(payload, filepath, true));
    EXPECT_TRUE(fs::exists(filepath));
    
    // Read back and verify
    std::ifstream file(filepath);
    nlohmann::json json_read;
    file >> json_read;
    file.close();
    
    EXPECT_EQ(json_read["run_uuid"], "test-uuid-1234");
    EXPECT_EQ(json_read["instance"]["name"], "test_instance");
    
    // Check pretty printing (file size should be larger)
    auto file_size = fs::file_size(filepath);
    EXPECT_GT(file_size, 500);  // Pretty JSON should be > 500 bytes for complete payload
}

TEST_F(ResultSerializerTest, WriteToFileCompact) {
    ResultPayload payload = create_complete_payload();
    std::string filepath = test_dir + "/result_compact.json";
    
    ResultSerializer::write_to_file(payload, filepath, false);
    EXPECT_TRUE(fs::exists(filepath));
    
    // Compact should be smaller than pretty
    std::string filepath_pretty = test_dir + "/result_pretty.json";
    ResultSerializer::write_to_file(payload, filepath_pretty, true);
    
    auto size_compact = fs::file_size(filepath);
    auto size_pretty = fs::file_size(filepath_pretty);
    EXPECT_LT(size_compact, size_pretty);
}

TEST_F(ResultSerializerTest, WriteToFileWithNestedDirectory) {
    ResultPayload payload = create_minimal_payload();
    std::string filepath = test_dir + "/nested/deep/path/result.json";
    
    EXPECT_NO_THROW(ResultSerializer::write_to_file(payload, filepath));
    EXPECT_TRUE(fs::exists(filepath));
    EXPECT_TRUE(fs::is_directory(test_dir + "/nested/deep/path"));
}

TEST_F(ResultSerializerTest, WriteInvalidPayloadThrows) {
    ResultPayload payload = create_minimal_payload();
    payload.run_uuid = "";  // Make invalid
    
    std::string filepath = test_dir + "/invalid.json";
    
    EXPECT_THROW({
        ResultSerializer::write_to_file(payload, filepath);
    }, std::invalid_argument);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(ResultSerializerTest, SerializeInvalidPayloadThrows) {
    ResultPayload payload = create_minimal_payload();
    payload.instance_name = "";  // Make invalid
    
    EXPECT_THROW({
        ResultSerializer::to_json(payload);
    }, std::invalid_argument);
}

TEST_F(ResultSerializerTest, WriteToInvalidPathThrows) {
    ResultPayload payload = create_minimal_payload();
    
    // Try to write to a path that cannot be created (e.g., root on most systems)
    EXPECT_THROW({
        ResultSerializer::write_to_file(payload, "/invalid/root/path/result.json");
    }, std::runtime_error);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(ResultSerializerTest, EmptyTags) {
    ResultPayload payload = create_minimal_payload();
    payload.instance_tags.clear();
    
    nlohmann::json json = ResultSerializer::to_json(payload);
    
    EXPECT_TRUE(json["instance"]["tags"].is_array());
    EXPECT_EQ(json["instance"]["tags"].size(), 0);
}

TEST_F(ResultSerializerTest, EmptyConfigParams) {
    ResultPayload payload = create_minimal_payload();
    payload.config_params.clear();
    
    nlohmann::json json = ResultSerializer::to_json(payload);
    
    EXPECT_TRUE(json["config"].is_object());
    EXPECT_TRUE(json["config"].contains("digest"));
}

TEST_F(ResultSerializerTest, ZeroRuntimeStats) {
    ResultPayload payload = create_minimal_payload();
    // All runtime stats default to 0
    
    nlohmann::json json = ResultSerializer::to_json(payload);
    
    EXPECT_DOUBLE_EQ(json["runtime"]["wall_clock_seconds"], 0.0);
    EXPECT_DOUBLE_EQ(json["runtime"]["cpu_seconds"], 0.0);
    EXPECT_EQ(json["runtime"]["solver_nodes"], 0);
    EXPECT_EQ(json["runtime"]["solver_iterations"], 0);
}

// Google Test's main() is provided by gtest_main.o

