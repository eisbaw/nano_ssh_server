#!/bin/bash
# Comprehensive test of all SSH server versions

set -e

RESULTS_FILE="comprehensive_test_results.md"
echo "# SSH Server Version Test Results" > "$RESULTS_FILE"
echo "" >> "$RESULTS_FILE"
echo "**Date:** $(date)" >> "$RESULTS_FILE"
echo "" >> "$RESULTS_FILE"
echo "## Test Summary" >> "$RESULTS_FILE"
echo "" >> "$RESULTS_FILE"
echo "| Version | Status | Notes |" >> "$RESULTS_FILE"
echo "|---------|--------|-------|" >> "$RESULTS_FILE"

test_binary() {
    local version_name="$1"
    local binary_path="$2"
    local port=2222

    echo "Testing: $version_name"

    if [ ! -f "$binary_path" ]; then
        echo "  [SKIP] Binary not found at $binary_path"
        echo "| $version_name | SKIP | Binary not found |" >> "$RESULTS_FILE"
        return
    fi

    # Kill any existing server
    pkill -f "nano_ssh_server" 2>/dev/null || true
    sleep 1

    # Start server
    cd "$(dirname "$binary_path")"
    timeout 30 "$binary_path" > server_test.log 2>&1 &
    SERVER_PID=$!
    sleep 3

    # Check if server is running
    if ! ps -p $SERVER_PID > /dev/null 2>&1; then
        echo "  [FAIL] Server failed to start"
        echo "| $version_name | FAIL | Server failed to start |" >> "$RESULTS_FILE"
        return
    fi

    # Test SSH connection
    OUTPUT=$(timeout 15 sshpass -p password123 ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=10 -p $port user@localhost "echo 'Test Successful'" 2>&1 || true)

    # Kill server
    kill $SERVER_PID 2>/dev/null || true
    pkill -f "nano_ssh_server" 2>/dev/null || true
    sleep 1

    # Check result
    if echo "$OUTPUT" | grep -q "Hello World\|Test Successful"; then
        echo "  [PASS] ✓"
        echo "| $version_name | ✅ PASS | Hello World received |" >> "$RESULTS_FILE"
    else
        echo "  [FAIL] Connection failed or no output"
        ERROR=$(echo "$OUTPUT" | grep -i "error\|refused\|timeout" | head -1)
        echo "| $version_name | ❌ FAIL | $ERROR |" >> "$RESULTS_FILE"
    fi

    cd /home/user/nano_ssh_server
}

# Test all prebuilt binaries
echo "=== Testing prebuilt binaries ==="
test_binary "v14-static" "/home/user/nano_ssh_server/v14-static/nano_ssh_server"
test_binary "v15-opt13 (debug)" "/home/user/nano_ssh_server/v15-opt13/nano_ssh_server_debug"
test_binary "v15-opt13 (no_linker)" "/home/user/nano_ssh_server/v15-opt13/nano_ssh_server_no_linker"
test_binary "v17-from14 (production)" "/home/user/nano_ssh_server/v17-from14/nano_ssh_server_production"
test_binary "v19-donna (production)" "/home/user/nano_ssh_server/v19-donna/nano_ssh_server_production"

echo ""
echo "=== Test Complete ==="
echo ""
cat "$RESULTS_FILE"
