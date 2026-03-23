#!/bin/bash

# Trealla-Lua Integration Test Runner
# This script compiles Trealla with Lua support and runs all functional/performance tests.

# Exit on any error
set -e

# Get the directory of this script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
ROOT_DIR="$( cd "$SCRIPT_DIR/../../" &> /dev/null && pwd )"

# Configuration
LUA_VER="5.4"
TPL_BIN="$ROOT_DIR/tpl"

echo "===================================================="
echo "   Trealla-Lua Integration: Automated Test Suite"
echo "===================================================="

# 1. Compilation
echo "[1/4] Compiling Trealla with Lua support..."
(cd "$ROOT_DIR" && make LUA=1 -j$(nproc))
echo "Compilation successful."
echo

# Move to script directory to ensure relative paths for tests work
cd "$SCRIPT_DIR"

# 2. Performance Benchmark (Fibonacci)
echo "[2/4] Running Performance Benchmark (Fibonacci 40)..."
$TPL_BIN -l lua_benchmark.pl -g "run_lua_prolog(40), halt."
echo "Benchmark completed."
echo

# 3. Complex Data Structures Test
echo "[3/4] Running Complex Data Structures Test..."
$TPL_BIN -l complex_test.pl -g "run_tests, halt."
echo "Functional tests completed."
echo

# 4. Advanced API Features (Backing Store & Backtracking)
echo "[4/5] Running Advanced API Tests (Store & Yield)..."
$TPL_BIN -l advanced_lua_test.pl -g "run_advanced_tests, halt."
echo "Advanced tests completed."
echo

# 5. Cycle Detection Test (Hybrid Control)
echo "[5/6] Running Cycle Detection Test (Lua-based state)..."
$TPL_BIN -l cycle_detection.pl -g "run_cycle_test, halt."
echo "Cycle detection test completed."
echo

# 6. Idiomatic Library Test
echo "[6/7] Running Idiomatic Library Test..."
$TPL_BIN -l idiomatic_lua_test.pl -g "run_idiomatic_test, halt."
echo "Idiomatic library test completed."
echo

# 7. Lua Sets & Math Test
echo "[7/10] Running Lua Sets & Math Test..."
$TPL_BIN -l lua_sets_test.pl -g "run_set_tests, halt."
echo "Lua Sets & Math test completed."
echo

# 8. Pure Cycle Detection Test
echo "[8/10] Running Pure Cycle Detection Test (Baseline)..."
$TPL_BIN -l cycle_detection_pure.pl -g "run_cycle_test, halt."
echo "Pure cycle detection test completed."
echo

# 9. Performance Hybrid Benchmark
echo "[9/10] Running Comprehensive Hybrid Benchmark (Trealla + Lua)..."
$TPL_BIN -l performance_hybrid.pl -g "run_hybrid_benchmark, halt."
echo "Hybrid benchmark completed."
echo

# 10. Performance Pure Benchmark
echo "[10/10] Running Comprehensive Pure Benchmark (Prolog Standard)..."
$TPL_BIN -l performance_pure.pl -g "run_pure_benchmark, halt."
echo "Pure benchmark completed."
echo

echo "===================================================="
echo "   All Trealla-Lua tests passed successfully!"
echo "===================================================="
