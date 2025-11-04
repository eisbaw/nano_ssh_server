#!/bin/bash
# Test: SSH authentication
# Verifies that correct password works and wrong password fails

set -e

VERSION=${1:-v0-vanilla}
PORT=2222
TIMEOUT=10

echo "========================================"
echo "Test: SSH Authentication"
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

# Test 1: Wrong password should fail
echo ""
echo "Test 1: Wrong password (should fail)..."
cd $VERSION
./nano_ssh_server > test_auth_wrong.log 2>&1 &
SERVER_PID=$!
cd ..
sleep 2

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "ERROR: Server failed to start"
    cat $VERSION/test_auth_wrong.log
    exit 1
fi

# Try to connect with wrong password (should fail)
timeout $TIMEOUT sshpass -p wrongpassword ssh \
    -o StrictHostKeyChecking=no \
    -o UserKnownHostsFile=/dev/null \
    -o LogLevel=ERROR \
    -o NumberOfPasswordPrompts=1 \
    -o ConnectTimeout=5 \
    -p $PORT user@localhost 2>&1 && RESULT="success" || RESULT="failed"

kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

if [ "$RESULT" = "failed" ]; then
    echo "✓ PASS: Wrong password correctly rejected"
else
    echo "✗ FAIL: Wrong password was accepted"
    exit 1
fi

sleep 1

# Test 2: Correct password should succeed
echo ""
echo "Test 2: Correct password (should succeed)..."
cd $VERSION
./nano_ssh_server > test_auth_correct.log 2>&1 &
SERVER_PID=$!
cd ..
sleep 2

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "ERROR: Server failed to start"
    cat $VERSION/test_auth_correct.log
    exit 1
fi

# Try to connect with correct password (should succeed)
OUTPUT=$(timeout $TIMEOUT sshpass -p password123 ssh \
    -o StrictHostKeyChecking=no \
    -o UserKnownHostsFile=/dev/null \
    -o LogLevel=ERROR \
    -o NumberOfPasswordPrompts=1 \
    -o ConnectTimeout=5 \
    -p $PORT user@localhost 2>&1 || true)

kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

if echo "$OUTPUT" | grep -q "Hello World"; then
    echo "✓ PASS: Correct password accepted and authenticated"
    echo ""
    echo "========================================"
    echo "All authentication tests PASSED"
    echo "========================================"
    exit 0
else
    echo "✗ FAIL: Correct password did not work"
    echo "  Output: $OUTPUT"
    cat $VERSION/test_auth_correct.log
    exit 1
fi
