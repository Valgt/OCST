# Makefile for Unit Tests
# Google Test framework

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g
INCLUDES = -I../../src/cpp/common \
           -I../../third_party \
           -I../../third_party/googletest/googletest/include \
           -I../../third_party/googletest/googlemock/include
LDFLAGS = -L../../third_party/googletest/build/lib -lgtest -lgtest_main -lpthread

# Directorios
TEST_DIR = .
BUILD_DIR = ../../build/tests
BIN_DIR = $(BUILD_DIR)/bin
OBJ_DIR = $(BUILD_DIR)/obj

# Test sources
TEST_SOURCES = $(wildcard *_test.cpp)
TEST_BINARIES = $(patsubst %.cpp,$(BIN_DIR)/%,$(TEST_SOURCES))

# Objetivos principales
all: setup gtest test_binaries
	@echo "✓ All unit tests compiled"

setup:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(OBJ_DIR)

# Build Google Test library (compile directly without CMake)
GTEST_DIR = ../../third_party/googletest/googletest
GTEST_SRC = $(GTEST_DIR)/src/gtest-all.cc
GTEST_MAIN_SRC = $(GTEST_DIR)/src/gtest_main.cc
GTEST_OBJ = $(OBJ_DIR)/gtest-all.o
GTEST_MAIN_OBJ = $(OBJ_DIR)/gtest_main.o

gtest: $(GTEST_OBJ) $(GTEST_MAIN_OBJ)
	@echo "✓ Google Test compiled"

$(GTEST_OBJ): $(GTEST_SRC) | $(OBJ_DIR)
	@echo "Compiling Google Test..."
	$(CXX) $(CXXFLAGS) -I$(GTEST_DIR) -I$(GTEST_DIR)/include -c $< -o $@ -pthread

$(GTEST_MAIN_OBJ): $(GTEST_MAIN_SRC) | $(OBJ_DIR)
	@echo "Compiling Google Test Main..."
	$(CXX) $(CXXFLAGS) -I$(GTEST_DIR) -I$(GTEST_DIR)/include -c $< -o $@ -pthread

# Compile test binaries
test_binaries: $(TEST_BINARIES)

# Common source files needed by tests
COMMON_SOURCES = ../../src/cpp/common/config.cpp ../../src/cpp/common/structured_logger.cpp

$(BIN_DIR)/%: %.cpp $(GTEST_OBJ) $(GTEST_MAIN_OBJ) $(COMMON_SOURCES) | $(BIN_DIR)
	@echo "Compiling $@..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(COMMON_SOURCES) $(GTEST_OBJ) $(GTEST_MAIN_OBJ) -o $@ -lpthread
	@echo "✓ $@ compiled"

# Ensure BIN_DIR exists
$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# Ensure OBJ_DIR exists
$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

# Run all tests
test: all
	@echo "========================================="
	@echo "Running Unit Tests"
	@echo "========================================="
	@for test in $(TEST_BINARIES); do \
		echo ""; \
		echo "Running $$test..."; \
		$$test || exit 1; \
	done
	@echo "========================================="
	@echo "✓ All tests passed"

# Run specific test
run_%: $(BIN_DIR)/%
	@echo "Running $<..."
	$<

# Clean
clean:
	@rm -rf $(BUILD_DIR)
	@echo "✓ Test build files cleaned"

clean_all: clean
	@echo "✓ All test files cleaned"

.PHONY: all setup gtest test_binaries test clean clean_all

