#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "config.h"

namespace ocst::common {

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary test directory
        test_dir_ = std::filesystem::temp_directory_path() / "ocst_config_test";
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        // Clean up test directory
        std::filesystem::remove_all(test_dir_);
    }

    std::filesystem::path test_dir_;
};

TEST_F(ConfigTest, DefaultValues) {
    Config config;

    EXPECT_DOUBLE_EQ(config.get_time_limit(), ConfigDefaults::TIME_LIMIT);
    EXPECT_DOUBLE_EQ(config.get_tolerance(), ConfigDefaults::TOLERANCE);
    EXPECT_DOUBLE_EQ(config.get_mip_gap(), ConfigDefaults::MIP_GAP);
    EXPECT_EQ(config.get_threads(), ConfigDefaults::THREADS);
    EXPECT_EQ(config.get_callback_frequency(), ConfigDefaults::CALLBACK_FREQUENCY);
    EXPECT_EQ(config.get_presolve(), ConfigDefaults::PRESOLVE);
    EXPECT_EQ(config.get_output_flag(), ConfigDefaults::OUTPUT_FLAG);
    EXPECT_STREQ(config.get_branching_strategy().c_str(), ConfigDefaults::BRANCHING_STRATEGY);
}

TEST_F(ConfigTest, LoadFromJsonObject) {
    nlohmann::json json_config = {
        {"time_limit", 1200.0},
        {"tolerance", 1e-8},
        {"threads", 4},
        {"presolve", false},
        {"branching_strategy", "pseudocost"}
    };

    Config config;
    bool success = config.load_from_json(json_config);

    EXPECT_TRUE(success);
    EXPECT_DOUBLE_EQ(config.get_time_limit(), 1200.0);
    EXPECT_DOUBLE_EQ(config.get_tolerance(), 1e-8);
    EXPECT_EQ(config.get_threads(), 4);
    EXPECT_FALSE(config.get_presolve());
    EXPECT_EQ(config.get_branching_strategy(), "pseudocost");

    // Check defaults for unspecified values
    EXPECT_DOUBLE_EQ(config.get_mip_gap(), ConfigDefaults::MIP_GAP);
    EXPECT_EQ(config.get_callback_frequency(), ConfigDefaults::CALLBACK_FREQUENCY);
}

TEST_F(ConfigTest, LoadFromFile) {
    // Create test config file
    std::filesystem::path config_file = test_dir_ / "test_config.json";
    nlohmann::json json_config = {
        {"time_limit", 600.0},
        {"mip_gap", 0.01},
        {"output_flag", true}
    };

    std::ofstream file(config_file);
    file << json_config.dump(2);
    file.close();

    Config config;
    bool success = config.load_from_file(config_file.string());

    EXPECT_TRUE(success);
    EXPECT_DOUBLE_EQ(config.get_time_limit(), 600.0);
    EXPECT_DOUBLE_EQ(config.get_mip_gap(), 0.01);
    EXPECT_TRUE(config.get_output_flag());
}

TEST_F(ConfigTest, InvalidJsonFile) {
    std::filesystem::path invalid_file = test_dir_ / "invalid.json";
    {
        std::ofstream file(invalid_file);
        file << "invalid json content {";
        file.close();
    }

    Config config;
    EXPECT_THROW(config.load_from_file(invalid_file.string()), std::runtime_error);
}

TEST_F(ConfigTest, NonexistentFile) {
    Config config;
    EXPECT_THROW(config.load_from_file("/nonexistent/file.json"), std::runtime_error);
}

TEST_F(ConfigTest, Validation) {
    // Test invalid time_limit
    nlohmann::json invalid_config = {{"time_limit", -100.0}};
    Config config;
    config.load_from_json(invalid_config);
    EXPECT_FALSE(config.validate());

    // Test invalid tolerance
    invalid_config = {{"tolerance", 2.0}};  // > 1.0
    config.load_from_json(invalid_config);
    EXPECT_FALSE(config.validate());

    // Test invalid threads
    invalid_config = {{"threads", 0}};
    config.load_from_json(invalid_config);
    EXPECT_FALSE(config.validate());

    // Test invalid branching strategy
    invalid_config = {{"branching_strategy", "invalid_strategy"}};
    config.load_from_json(invalid_config);
    EXPECT_FALSE(config.validate());
}

TEST_F(ConfigTest, GetAllParams) {
    nlohmann::json json_config = {
        {"time_limit", 900.0},
        {"threads", 2},
        {"presolve", false}
    };

    Config config;
    config.load_from_json(json_config);

    auto params = config.get_all_params();

    EXPECT_EQ(params.size(), 3);
    EXPECT_EQ(params["time_limit"], "900.000000");
    EXPECT_EQ(params["threads"], "2");
    EXPECT_EQ(params["presolve"], "false");
}

} // namespace ocst::common
