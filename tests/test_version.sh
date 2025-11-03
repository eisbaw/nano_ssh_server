#!/bin/bash
# Test: SSH version exchange
# Verifies that the server sends the SSH-2.0 version string

set -e

VERSION=${1:-v0-vanilla}
PORT=2222
TIMEOUT=5

echo "========================================"
echo "Test: SSH Version Exchange"
echo "Version: $VERSION"
echo "========================================"

# Check if binary exists
if [ ! -f "$VERSION/nano_ssh_server" ]; then
    echo "ERROR: $VERSION/nano_ssh_server not found"
    echo "Run 'just build $VERSION' first"
    exit 1
fi

# Kill any existing server on the port
pkill -f nano_ssh_server || true
sleep 1

# Start the server in background
echo "Starting server..."
cd $VERSION
./nano_ssh_server > test_version.log 2>&1 &
SERVER_PID=$!
cd ..

# Wait for server to be ready
sleep 2

# Check if server is still running
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "ERROR: Server failed to start"
    cat $VERSION/test_version.log
    exit 1
fi

# Test version exchange with netcat
echo "Testing version exchange..."
RESPONSE=$(echo "" | timeout $TIMEOUT nc localhost $PORT | head -n 1)

# Kill the server
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

# Verify response
if echo "$RESPONSE" | grep -q "SSH-2.0"; then
    echo "✓ PASS: Received SSH-2.0 version string"
    echo "  Response: $RESPONSE"
    exit 0
else
    echo "✗ FAIL: Did not receive SSH-2.0 version string"
    echo "  Response: $RESPONSE"
    cat $VERSION/test_version.log
    exit 1
fi
