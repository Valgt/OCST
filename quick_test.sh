#!/bin/bash
# Simple compilation script
echo "Compiling path-based formulation..."
g++ -std=c++17 -Wall -Wextra -O2 -I$GUROBI_HOME/include -L$GUROBI_HOME/lib src/cpp/algorithms/path_based_formulation/algorithm.cpp -o build/executables/path_based_formulation -lgurobi_c++ -lgurobi120

if [ $? -eq 0 ]; then
    echo "✓ Compilation successful"
    echo "Testing with ocstpin1..."
    echo "Expected optimal: 4135577"
    echo "========================"
    ./build/executables/path_based_formulation data/input/test_instances/ocstpin1 | head -50
else
    echo "❌ Compilation failed"
fi
