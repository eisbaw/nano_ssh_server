#!/bin/bash
# Run all tests for a specific version
# Usage: ./run_all.sh [version]

VERSION=${1:-v0-vanilla}

echo "========================================"
echo "Running All Tests for $VERSION"
echo "========================================"
echo ""

# Track results
PASSED=0
FAILED=0
TOTAL=0

# Function to run a test
run_test() {
    local test_script=$1
    local test_name=$2

    TOTAL=$((TOTAL + 1))

    echo ""
    if bash "$test_script" "$VERSION"; then
        PASSED=$((PASSED + 1))
        echo "✓ $test_name: PASSED"
    else
        FAILED=$((FAILED + 1))
        echo "✗ $test_name: FAILED"
    fi

    # Clean up any lingering processes
    pkill -f nano_ssh_server 2>/dev/null || true
    sleep 1
}

# Run all tests
cd "$(dirname "$0")/.."

run_test "tests/test_version.sh" "Version Exchange"
run_test "tests/test_connection.sh" "Full SSH Connection"
run_test "tests/test_auth.sh" "Authentication"

# Print summary
echo ""
echo "========================================"
echo "Test Summary for $VERSION"
echo "========================================"
echo "Total tests: $TOTAL"
echo "Passed:      $PASSED"
echo "Failed:      $FAILED"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "✓ ALL TESTS PASSED"
    exit 0
else
    echo "✗ SOME TESTS FAILED"
    exit 1
fi
