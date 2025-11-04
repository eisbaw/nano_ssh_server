#!/bin/bash
# Test: Full SSH connection
# Verifies that an SSH client can connect and receive "Hello World"

set -e

VERSION=${1:-v0-vanilla}
PORT=2222
TIMEOUT=10

echo "========================================"
echo "Test: Full SSH Connection"
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
./nano_ssh_server > test_connection.log 2>&1 &
SERVER_PID=$!
cd ..

# Wait for server to be ready
sleep 2

# Check if server is still running
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "ERROR: Server failed to start"
    cat $VERSION/test_connection.log
    exit 1
fi

# Test SSH connection with sshpass
echo "Testing SSH connection (user: user, password: password123)..."

# Capture output from SSH session
OUTPUT=$(timeout $TIMEOUT sshpass -p password123 ssh \
    -o StrictHostKeyChecking=no \
    -o UserKnownHostsFile=/dev/null \
    -o LogLevel=ERROR \
    -p $PORT user@localhost 2>&1 || true)

# Kill the server
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

# Verify response
if echo "$OUTPUT" | grep -q "Hello World"; then
    echo "✓ PASS: SSH connection successful and received 'Hello World'"
    echo "  Output: $OUTPUT"
    exit 0
else
    echo "✗ FAIL: Did not receive 'Hello World'"
    echo "  Output: $OUTPUT"
    echo ""
    echo "Server log:"
    cat $VERSION/test_connection.log
    exit 1
fi
