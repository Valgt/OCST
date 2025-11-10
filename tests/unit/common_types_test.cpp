/**
 * @file common_types_test.cpp
 * @brief Unit tests for common_types.h (Edge, Requirement, OCSTInstance)
 * 
 * Tests cover:
 * - Edge and Requirement equality
 * - OCSTInstance construction
 * - add_edge() with adjacency matrix updates
 * - add_requirement() with bounds checking
 * - has_edge() and get_edge_index() queries
 * - validate() consistency checks
 */

#include <gtest/gtest.h>
#include "common_types.h"
#include <stdexcept>

using namespace ocst::path_based;

// ============================================================================
// Edge Tests
// ============================================================================

TEST(EdgeTest, Construction) {
    Edge e(0, 1, 5.0);
    EXPECT_EQ(e.source, 0);
    EXPECT_EQ(e.destination, 1);
    EXPECT_DOUBLE_EQ(e.cost, 5.0);
}

TEST(EdgeTest, Equality) {
    Edge e1(0, 1, 5.0);
    Edge e2(0, 1, 5.0);
    Edge e3(0, 1, 5.0 + 1e-10);  // Within tolerance
    Edge e4(1, 0, 5.0);          // Different endpoints
    Edge e5(0, 1, 6.0);          // Different cost
    
    EXPECT_TRUE(e1 == e2);
    EXPECT_TRUE(e1 == e3);  // Tolerance check
    EXPECT_FALSE(e1 == e4);
    EXPECT_FALSE(e1 == e5);
}

// ============================================================================
// Requirement Tests
// ============================================================================

TEST(RequirementTest, Construction) {
    Requirement r(0, 5, 10.0);
    EXPECT_EQ(r.origin, 0);
    EXPECT_EQ(r.destination, 5);
    EXPECT_DOUBLE_EQ(r.weight, 10.0);
}

TEST(RequirementTest, Equality) {
    Requirement r1(0, 5, 10.0);
    Requirement r2(0, 5, 10.0);
    Requirement r3(0, 5, 10.0 + 1e-10);  // Within tolerance
    Requirement r4(5, 0, 10.0);          // Different endpoints
    Requirement r5(0, 5, 12.0);          // Different weight
    
    EXPECT_TRUE(r1 == r2);
    EXPECT_TRUE(r1 == r3);
    EXPECT_FALSE(r1 == r4);
    EXPECT_FALSE(r1 == r5);
}

// ============================================================================
// OCSTInstance Tests - Construction
// ============================================================================

TEST(OCSTInstanceTest, ConstructorDefault) {
    OCSTInstance instance(5);
    EXPECT_EQ(instance.num_nodes, 5);
    EXPECT_EQ(instance.num_edges, 0);
    EXPECT_DOUBLE_EQ(instance.probability, 0.0);
    EXPECT_TRUE(instance.edges.empty());
    EXPECT_TRUE(instance.requirements.empty());
    EXPECT_EQ(instance.adjacency_matrix.size(), 5);
    EXPECT_EQ(instance.adjacency_matrix[0].size(), 5);
}

TEST(OCSTInstanceTest, ConstructorWithProbability) {
    OCSTInstance instance(10, 0.75);
    EXPECT_EQ(instance.num_nodes, 10);
    EXPECT_DOUBLE_EQ(instance.probability, 0.75);
}

// ============================================================================
// OCSTInstance Tests - add_edge
// ============================================================================

TEST(OCSTInstanceTest, AddEdgeValid) {
    OCSTInstance instance(5);
    
    instance.add_edge(0, 1, 3.0);
    EXPECT_EQ(instance.num_edges, 1);
    EXPECT_EQ(instance.edges.size(), 1);
    EXPECT_EQ(instance.edges[0].source, 0);
    EXPECT_EQ(instance.edges[0].destination, 1);
    EXPECT_DOUBLE_EQ(instance.edges[0].cost, 3.0);
    
    // Check adjacency matrix updated
    EXPECT_EQ(instance.adjacency_matrix[0][1], 0);  // Edge index 0
    EXPECT_EQ(instance.adjacency_matrix[1][0], 0);  // Bidirectional
}

TEST(OCSTInstanceTest, AddEdgeMultiple) {
    OCSTInstance instance(4);
    
    instance.add_edge(0, 1, 5.0);
    instance.add_edge(1, 2, 7.0);
    instance.add_edge(2, 3, 3.0);
    
    EXPECT_EQ(instance.num_edges, 3);
    EXPECT_EQ(instance.edges.size(), 3);
    
    // Verify adjacency matrix
    EXPECT_EQ(instance.adjacency_matrix[0][1], 0);
    EXPECT_EQ(instance.adjacency_matrix[1][2], 1);
    EXPECT_EQ(instance.adjacency_matrix[2][3], 2);
    EXPECT_EQ(instance.adjacency_matrix[0][2], -1);  // No edge
}

TEST(OCSTInstanceTest, AddEdgeOutOfBounds) {
    OCSTInstance instance(5);
    
    // Test negative indices
    EXPECT_THROW(instance.add_edge(-1, 2, 5.0), std::out_of_range);
    EXPECT_THROW(instance.add_edge(2, -1, 5.0), std::out_of_range);
    
    // Test indices >= num_nodes
    EXPECT_THROW(instance.add_edge(5, 2, 5.0), std::out_of_range);
    EXPECT_THROW(instance.add_edge(2, 10, 5.0), std::out_of_range);
}

TEST(OCSTInstanceTest, AddEdgeIndexMap) {
    OCSTInstance instance(3);
    
    instance.add_edge(0, 1, 5.0);
    instance.add_edge(1, 2, 7.0);
    
    // Check edge_index_map
    EXPECT_EQ(instance.edge_index_map["0,1"], 0);
    EXPECT_EQ(instance.edge_index_map["1,0"], 0);
    EXPECT_EQ(instance.edge_index_map["1,2"], 1);
    EXPECT_EQ(instance.edge_index_map["2,1"], 1);
    EXPECT_EQ(instance.edge_index_map.count("0,2"), 0);  // No such edge
}

// ============================================================================
// OCSTInstance Tests - add_requirement
// ============================================================================

TEST(OCSTInstanceTest, AddRequirementValid) {
    OCSTInstance instance(5);
    
    instance.add_requirement(0, 4, 10.0);
    EXPECT_EQ(instance.requirements.size(), 1);
    EXPECT_EQ(instance.requirements[0].origin, 0);
    EXPECT_EQ(instance.requirements[0].destination, 4);
    EXPECT_DOUBLE_EQ(instance.requirements[0].weight, 10.0);
}

TEST(OCSTInstanceTest, AddRequirementMultiple) {
    OCSTInstance instance(6);
    
    instance.add_requirement(0, 3, 5.0);
    instance.add_requirement(1, 4, 8.0);
    instance.add_requirement(2, 5, 12.0);
    
    EXPECT_EQ(instance.requirements.size(), 3);
    EXPECT_DOUBLE_EQ(instance.requirements[1].weight, 8.0);
}

TEST(OCSTInstanceTest, AddRequirementOutOfBounds) {
    OCSTInstance instance(5);
    
    // Test negative indices
    EXPECT_THROW(instance.add_requirement(-1, 2, 5.0), std::out_of_range);
    EXPECT_THROW(instance.add_requirement(2, -1, 5.0), std::out_of_range);
    
    // Test indices >= num_nodes
    EXPECT_THROW(instance.add_requirement(5, 2, 5.0), std::out_of_range);
    EXPECT_THROW(instance.add_requirement(2, 10, 5.0), std::out_of_range);
}

// ============================================================================
// OCSTInstance Tests - has_edge and get_edge_index
// ============================================================================

TEST(OCSTInstanceTest, HasEdge) {
    OCSTInstance instance(4);
    
    instance.add_edge(0, 1, 5.0);
    instance.add_edge(1, 2, 7.0);
    
    EXPECT_TRUE(instance.has_edge(0, 1));
    EXPECT_TRUE(instance.has_edge(1, 0));  // Bidirectional
    EXPECT_TRUE(instance.has_edge(1, 2));
    EXPECT_FALSE(instance.has_edge(0, 2));
    EXPECT_FALSE(instance.has_edge(0, 3));
}

TEST(OCSTInstanceTest, GetEdgeIndex) {
    OCSTInstance instance(4);
    
    instance.add_edge(0, 1, 5.0);
    instance.add_edge(1, 2, 7.0);
    instance.add_edge(2, 3, 3.0);
    
    EXPECT_EQ(instance.get_edge_index(0, 1), 0);
    EXPECT_EQ(instance.get_edge_index(1, 0), 0);  // Bidirectional
    EXPECT_EQ(instance.get_edge_index(1, 2), 1);
    EXPECT_EQ(instance.get_edge_index(2, 3), 2);
    EXPECT_EQ(instance.get_edge_index(0, 3), -1);  // No edge
}

// ============================================================================
// OCSTInstance Tests - validate
// ============================================================================

TEST(OCSTInstanceTest, ValidateEmptyInstance) {
    OCSTInstance instance(5);
    
    auto [valid, message] = instance.validate();
    EXPECT_TRUE(valid);
    EXPECT_EQ(message, "");
}

TEST(OCSTInstanceTest, ValidateValidInstance) {
    OCSTInstance instance(4);
    
    instance.add_edge(0, 1, 5.0);
    instance.add_edge(1, 2, 7.0);
    instance.add_edge(2, 3, 3.0);
    instance.add_requirement(0, 3, 10.0);
    
    auto [valid, message] = instance.validate();
    EXPECT_TRUE(valid);
    EXPECT_EQ(message, "");
}

TEST(OCSTInstanceTest, ValidateNumEdgesMismatch) {
    OCSTInstance instance(3);
    
    instance.add_edge(0, 1, 5.0);
    instance.num_edges = 999;  // Manually corrupt
    
    auto [valid, message] = instance.validate();
    EXPECT_FALSE(valid);
    EXPECT_NE(message.find("num_edges"), std::string::npos);
}

TEST(OCSTInstanceTest, ValidateAdjacencyMatrixInconsistency) {
    OCSTInstance instance(3);
    
    instance.add_edge(0, 1, 5.0);
    instance.adjacency_matrix[0][1] = -1;  // Manually corrupt
    
    auto [valid, message] = instance.validate();
    EXPECT_FALSE(valid);
    EXPECT_NE(message.find("Adjacency matrix"), std::string::npos);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(OCSTInstanceTest, CompleteSmallInstance) {
    // Build a small complete instance: K4 (complete graph on 4 nodes)
    OCSTInstance instance(4);
    
    // Add all edges
    for (int i = 0; i < 4; ++i) {
        for (int j = i + 1; j < 4; ++j) {
            instance.add_edge(i, j, static_cast<double>(i + j));
        }
    }
    
    EXPECT_EQ(instance.num_edges, 6);  // C(4,2) = 6
    
    // Add requirements
    instance.add_requirement(0, 3, 10.0);
    instance.add_requirement(1, 2, 5.0);
    
    EXPECT_EQ(instance.requirements.size(), 2);
    
    // Validate
    auto [valid, message] = instance.validate();
    EXPECT_TRUE(valid);
    
    // Test queries
    EXPECT_TRUE(instance.has_edge(0, 2));
    EXPECT_TRUE(instance.has_edge(1, 3));
    EXPECT_EQ(instance.get_edge_index(0, 1), 0);
}

// Google Test's main() is provided by gtest_main.o

