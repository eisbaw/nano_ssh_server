#!/bin/bash
# Test all SSH server versions

set -e

RESULTS_FILE="test_results.txt"
echo "=====================================" > "$RESULTS_FILE"
echo "SSH Server Version Test Results" >> "$RESULTS_FILE"
echo "Date: $(date)" >> "$RESULTS_FILE"
echo "=====================================" >> "$RESULTS_FILE"
echo "" >> "$RESULTS_FILE"

# Test function
test_version() {
    local version_name="$1"
    local binary_path="$2"
    local port=2222

    echo "Testing: $version_name"
    echo "Binary: $binary_path"

    if [ ! -f "$binary_path" ]; then
        echo "  [SKIP] Binary not found"
        echo "$version_name: SKIP - Binary not found" >> "$RESULTS_FILE"
        return
    fi

    # Kill any existing server
    pkill -f "nano_ssh_server" || true
    sleep 1

    # Start server in background
    cd "$(dirname "$binary_path")"
    "$binary_path" > server_test.log 2>&1 &
    SERVER_PID=$!
    sleep 2

    # Check if server is running
    if ! ps -p $SERVER_PID > /dev/null 2>&1; then
        echo "  [FAIL] Server failed to start"
        echo "$version_name: FAIL - Server failed to start" >> "$RESULTS_FILE"
        cat server_test.log
        return
    fi

    # Test SSH connection
    echo "  Connecting with SSH client..."
    OUTPUT=$(sshpass -p password123 ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=10 -p $port user@localhost "echo 'Hello World'" 2>&1 || true)

    # Kill server
    kill $SERVER_PID 2>/dev/null || true
    sleep 1

    # Check result
    if echo "$OUTPUT" | grep -q "Hello World"; then
        echo "  [PASS] ✓ Hello World received!"
        echo "$version_name: PASS ✓" >> "$RESULTS_FILE"
    else
        echo "  [FAIL] Did not receive Hello World"
        echo "  Output: $OUTPUT"
        echo "$version_name: FAIL - $OUTPUT" >> "$RESULTS_FILE"
        echo "  Server log:"
        cat server_test.log | head -50
    fi

    echo ""
    cd /home/user/nano_ssh_server
}

cd /home/user/nano_ssh_server

# Test v15-opt13
test_version "v15-opt13 (debug)" "/home/user/nano_ssh_server/v15-opt13/nano_ssh_server_debug"
test_version "v15-opt13 (no_linker)" "/home/user/nano_ssh_server/v15-opt13/nano_ssh_server_no_linker"

# Test v17-from14
test_version "v17-from14 (production)" "/home/user/nano_ssh_server/v17-from14/nano_ssh_server_production"

# Test v19-donna
test_version "v19-donna (production)" "/home/user/nano_ssh_server/v19-donna/nano_ssh_server_production"

echo ""
echo "======================================"
echo "Test Summary"
echo "======================================"
cat "$RESULTS_FILE"
echo ""
