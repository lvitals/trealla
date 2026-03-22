#!/bin/bash

# Trealla-Lua Integration Test Runner
# This script compiles Trealla with Lua support and runs all functional/performance tests.

# Exit on any error
set -e

# Configuration
LUA_VER="5.4"
TPL_BIN="./tpl"

echo "===================================================="
echo "   Trealla-Lua Integration: Automated Test Suite"
echo "===================================================="

# 1. Compilation
echo "[1/4] Compiling Trealla with Lua $LUA_VER support..."
make LUA_VERSION=$LUA_VER -j$(nproc)
echo "Compilation successful."
echo

# 2. Performance Benchmark (Fibonacci)
echo "[2/4] Running Performance Benchmark (Fibonacci 40)..."
$TPL_BIN -l ../benchmark.pl -g "run_lua_prolog(40), halt."
echo "Benchmark completed."
echo

# 3. Complex Data Structures Test
echo "[3/4] Running Complex Data Structures Test..."
$TPL_BIN -l tests/complex_test.pl -g "run_tests, halt."
echo "Functional tests completed."
echo

# 4. Advanced API Features (Backing Store & Backtracking)
echo "[4/4] Running Advanced API Tests (Store & Yield)..."
$TPL_BIN -l tests/advanced_lua_test.pl -g "run_advanced_tests, halt."
echo "Advanced tests completed."
echo

echo "===================================================="
echo "   All Trealla-Lua tests passed successfully!"
echo "===================================================="
