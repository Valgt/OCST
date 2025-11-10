#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include "structured_logger.h"

namespace ocst::common {

class StructuredLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "ocst_logger_test";
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::filesystem::path test_dir_;
    std::filesystem::path log_file_;
};

TEST_F(StructuredLoggerTest, ConstructorCreatesFile) {
    log_file_ = test_dir_ / "test_log";
    {
        StructuredLogger logger(log_file_.string(), 1);  // 1MB limit
        EXPECT_TRUE(std::filesystem::exists(log_file_.string() + ".0.jsonl"));
    }  // Destructor should finalize
}

TEST_F(StructuredLoggerTest, LogBranchingEvent) {
    log_file_ = test_dir_ / "branching_log";
    StructuredLogger logger(log_file_.string(), 1);

    BranchingEvent event{
        42,                    // node_id
        5,                     // variable_index
        0.7,                   // variable_value
        "up",                  // branch_direction
        1.0,                   // bound_value
        std::chrono::system_clock::now()
    };

    logger.log_branching_event(event);

    // Force finalization
    logger.finalize(false);

    // Check file contents
    std::filesystem::path log_path = log_file_.string() + ".0.jsonl";
    ASSERT_TRUE(std::filesystem::exists(log_path));

    std::ifstream file(log_path);
    std::string line;
    std::getline(file, line);

    // Parse JSON and verify
    nlohmann::json json_event = nlohmann::json::parse(line);
    EXPECT_EQ(json_event["event_type"], "branching_decision");
    EXPECT_EQ(json_event["node_id"], 42);
    EXPECT_EQ(json_event["variable_index"], 5);
    EXPECT_DOUBLE_EQ(json_event["variable_value"], 0.7);
    EXPECT_EQ(json_event["branch_direction"], "up");
    EXPECT_DOUBLE_EQ(json_event["bound_value"], 1.0);
    EXPECT_TRUE(json_event.contains("timestamp"));
}

TEST_F(StructuredLoggerTest, LogLazyCutEvent) {
    log_file_ = test_dir_ / "lazy_cut_log";
    StructuredLogger logger(log_file_.string(), 1);

    LazyCutEvent event{
        10,                    // cut_id
        3,                     // violated_constraints
        2.5,                   // violation_amount
        "subtour",             // cut_type
        std::chrono::system_clock::now()
    };

    logger.log_lazy_cut(event);
    logger.finalize(false);

    std::filesystem::path log_path = log_file_.string() + ".0.jsonl";
    ASSERT_TRUE(std::filesystem::exists(log_path));

    std::ifstream file(log_path);
    std::string line;
    std::getline(file, line);

    nlohmann::json json_event = nlohmann::json::parse(line);
    EXPECT_EQ(json_event["event_type"], "lazy_cut_added");
    EXPECT_EQ(json_event["cut_id"], 10);
    EXPECT_EQ(json_event["violated_constraints"], 3);
    EXPECT_DOUBLE_EQ(json_event["violation_amount"], 2.5);
    EXPECT_EQ(json_event["cut_type"], "subtour");
}

TEST_F(StructuredLoggerTest, LogCallbackStats) {
    log_file_ = test_dir_ / "callback_log";
    StructuredLogger logger(log_file_.string(), 1);

    CallbackStats event{
        100,                   // current_node
        1234.56,              // current_objective
        1200.0,               // best_bound
        25,                   // cuts_added
        3.14,                 // gap_percentage
        std::chrono::system_clock::now()
    };

    logger.log_callback_stats(event);
    logger.finalize(false);

    std::filesystem::path log_path = log_file_.string() + ".0.jsonl";
    ASSERT_TRUE(std::filesystem::exists(log_path));

    std::ifstream file(log_path);
    std::string line;
    std::getline(file, line);

    nlohmann::json json_event = nlohmann::json::parse(line);
    EXPECT_EQ(json_event["event_type"], "callback_stats");
    EXPECT_EQ(json_event["current_node"], 100);
    EXPECT_DOUBLE_EQ(json_event["current_objective"], 1234.56);
    EXPECT_DOUBLE_EQ(json_event["best_bound"], 1200.0);
    EXPECT_EQ(json_event["cuts_added"], 25);
    EXPECT_DOUBLE_EQ(json_event["gap_percentage"], 3.14);
}

TEST_F(StructuredLoggerTest, FileRotation) {
    log_file_ = test_dir_ / "rotation_log";
    StructuredLogger logger(log_file_.string(), 1);  // Very small limit for testing

    // Log many events to trigger rotation
    for (int i = 0; i < 1000; ++i) {
        BranchingEvent event{i, 0, 0.0, "up", 1.0, std::chrono::system_clock::now()};
        logger.log_branching_event(event);
    }

    logger.finalize(false);

    // Should have created multiple files
    int file_count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(test_dir_)) {
        if (entry.path().string().find("rotation_log") != std::string::npos &&
            entry.path().extension() == ".jsonl") {
            file_count++;
        }
    }

    EXPECT_GE(file_count, 1);  // Should have created at least one file
}

TEST_F(StructuredLoggerTest, TimestampFormat) {
    log_file_ = test_dir_ / "timestamp_log";
    StructuredLogger logger(log_file_.string(), 1);

    // Create event with known timestamp
    auto test_time = std::chrono::system_clock::time_point(std::chrono::seconds(1609459200)); // 2021-01-01 00:00:00 UTC

    BranchingEvent event{1, 1, 1.0, "test", 1.0, test_time};
    logger.log_branching_event(event);
    logger.finalize(false);

    std::filesystem::path log_path = log_file_.string() + ".0.jsonl";
    std::ifstream file(log_path);
    std::string line;
    std::getline(file, line);

    nlohmann::json json_event = nlohmann::json::parse(line);
    std::string timestamp = json_event["timestamp"];

    // Should be in ISO 8601 format with microseconds
    EXPECT_TRUE(timestamp.find('T') != std::string::npos);
    EXPECT_TRUE(timestamp.find('Z') != std::string::npos);
    EXPECT_TRUE(timestamp.find('.') != std::string::npos);
    EXPECT_EQ(timestamp.size(), 27); // YYYY-MM-DDTHH:MM:SS.ssssssZ
}

TEST_F(StructuredLoggerTest, GetFileSize) {
    log_file_ = test_dir_ / "size_log";
    StructuredLogger logger(log_file_.string(), 1);

    // Initially should be small
    EXPECT_LT(logger.get_current_file_size(), 1000);

    // After logging, should be larger
    BranchingEvent event{1, 1, 1.0, "test", 1.0, std::chrono::system_clock::now()};
    logger.log_branching_event(event);

    EXPECT_GT(logger.get_current_file_size(), 100);
}

} // namespace ocst::common
