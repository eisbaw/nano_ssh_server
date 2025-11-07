#!/bin/bash
# Test all versions for "Hello World" SSH output

set -e

echo "========================================"
echo "Testing All Versions - Hello World SSH"
echo "========================================"
echo ""

# Kill any existing servers
killall -9 nano_ssh_server 2>/dev/null || true
sleep 1

RESULTS_FILE="/tmp/test_results.txt"
> "$RESULTS_FILE"

# Find all built versions
VERSIONS=$(find . -maxdepth 2 -name "nano_ssh_server" -type f -executable | grep -E "v[0-9]" | sed 's|./||' | sed 's|/nano_ssh_server||' | sort)

if [ -z "$VERSIONS" ]; then
    echo "No built versions found!"
    exit 1
fi

echo "Found versions to test:"
echo "$VERSIONS" | sed 's/^/  - /'
echo ""

# Test each version
for VERSION in $VERSIONS; do
    echo "----------------------------------------"
    echo "Testing: $VERSION"
    echo "----------------------------------------"

    # Kill any existing server
    killall -9 nano_ssh_server 2>/dev/null || true
    sleep 1

    # Start server
    cd "$VERSION"
    ./nano_ssh_server > /tmp/test_${VERSION}.log 2>&1 &
    SERVER_PID=$!
    cd ..

    # Wait for server to be ready
    sleep 2

    # Check if server is running
    if ! ps -p $SERVER_PID > /dev/null 2>&1; then
        echo "✗ FAIL: Server failed to start"
        echo "$VERSION: FAIL - Server crashed" >> "$RESULTS_FILE"
        continue
    fi

    # Verify port is listening
    if ! lsof -i :2222 2>/dev/null | grep -q LISTEN; then
        echo "✗ FAIL: Server not listening on port 2222"
        echo "$VERSION: FAIL - Not listening" >> "$RESULTS_FILE"
        kill $SERVER_PID 2>/dev/null || true
        continue
    fi

    echo "  ✓ Server started and listening"

    # Test SSH connection
    OUTPUT=$(timeout 10 sshpass -p password123 ssh \
        -o StrictHostKeyChecking=no \
        -o UserKnownHostsFile=/dev/null \
        -o HostKeyAlgorithms=+ssh-rsa \
        -o PubkeyAcceptedAlgorithms=+ssh-rsa \
        -o LogLevel=ERROR \
        -p 2222 user@localhost 2>&1 || true)

    # Kill server
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true

    # Check result
    if echo "$OUTPUT" | grep -q "Hello World"; then
        echo "  ✅ SUCCESS: Received 'Hello World'"
        echo "  Output: $OUTPUT"
        echo "$VERSION: SUCCESS - $OUTPUT" >> "$RESULTS_FILE"
    elif [ -z "$OUTPUT" ]; then
        echo "  ⚠️  WARNING: No output (connection may have failed)"
        echo "$VERSION: FAIL - No output" >> "$RESULTS_FILE"
    else
        echo "  ⚠️  WARNING: Unexpected output"
        echo "  Output: $OUTPUT"
        echo "$VERSION: PARTIAL - $OUTPUT" >> "$RESULTS_FILE"
    fi

    echo ""
done

# Summary
echo "========================================"
echo "SUMMARY"
echo "========================================"
cat "$RESULTS_FILE"
echo ""

# Count results
SUCCESS=$(grep -c "SUCCESS" "$RESULTS_FILE" || echo "0")
TOTAL=$(wc -l < "$RESULTS_FILE")

echo "Results: $SUCCESS/$TOTAL versions successful"
echo ""

if [ "$SUCCESS" -gt 0 ]; then
    echo "✅ At least one version works!"
    exit 0
else
    echo "❌ No versions produced 'Hello World' output"
    exit 1
fi
