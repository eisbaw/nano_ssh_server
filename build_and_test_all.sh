#!/bin/bash
# Build and test all versions systematically

set -e

echo "========================================"
echo "Building All Versions"
echo "========================================"
echo ""

# Kill any existing servers
killall -9 nano_ssh_server 2>/dev/null || true
sleep 1

RESULTS_FILE="/tmp/all_versions_results.txt"
> "$RESULTS_FILE"

# Get all version directories
VERSIONS=$(ls -d v*/ 2>/dev/null | sed 's|/||' | sort -V)

# Build each version
for VERSION in $VERSIONS; do
    echo "----------------------------------------"
    echo "Version: $VERSION"
    echo "----------------------------------------"

    # Check if Makefile exists
    if [ ! -f "$VERSION/Makefile" ]; then
        echo "  ⊘ No Makefile - skipping"
        echo "$VERSION: SKIP - No Makefile" >> "$RESULTS_FILE"
        continue
    fi

    # Check if already built
    if [ -f "$VERSION/nano_ssh_server" ]; then
        echo "  ✓ Already built"
    else
        echo "  Building..."
        cd "$VERSION"
        if make clean > /dev/null 2>&1 && make > /tmp/build_${VERSION}.log 2>&1; then
            echo "  ✓ Build successful"
            cd ..
        else
            echo "  ✗ Build failed"
            echo "$VERSION: FAIL - Build error" >> "$RESULTS_FILE"
            cd ..
            continue
        fi
    fi

    # Test the version
    echo "  Testing SSH connection..."

    # Start server
    cd "$VERSION"
    ./nano_ssh_server > /tmp/test_${VERSION}.log 2>&1 &
    SERVER_PID=$!
    cd ..

    # Wait for server
    sleep 2

    # Check if server is running
    if ! ps -p $SERVER_PID > /dev/null 2>&1; then
        echo "  ✗ Server crashed on startup"
        echo "$VERSION: FAIL - Server crashed" >> "$RESULTS_FILE"
        continue
    fi

    # Check if listening
    if ! lsof -i :2222 2>/dev/null | grep -q LISTEN; then
        echo "  ✗ Server not listening"
        echo "$VERSION: FAIL - Not listening" >> "$RESULTS_FILE"
        kill $SERVER_PID 2>/dev/null || true
        continue
    fi

    # Try SSH connection
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
        echo "  ✅ SUCCESS - Received 'Hello World'"
        SIZE=$(stat -c%s "$VERSION/nano_ssh_server" 2>/dev/null || echo "?")
        echo "$VERSION: SUCCESS - $SIZE bytes - $OUTPUT" >> "$RESULTS_FILE"
    elif [ -z "$OUTPUT" ]; then
        echo "  ⚠️  FAIL - No output"
        SIZE=$(stat -c%s "$VERSION/nano_ssh_server" 2>/dev/null || echo "?")
        echo "$VERSION: FAIL - $SIZE bytes - No output" >> "$RESULTS_FILE"
    else
        echo "  ⚠️  PARTIAL - Got: $OUTPUT"
        SIZE=$(stat -c%s "$VERSION/nano_ssh_server" 2>/dev/null || echo "?")
        echo "$VERSION: PARTIAL - $SIZE bytes - $OUTPUT" >> "$RESULTS_FILE"
    fi

    echo ""
done

# Summary
echo "========================================"
echo "SUMMARY - ALL VERSIONS"
echo "========================================"
echo ""

echo "Working versions (Hello World):"
grep "SUCCESS" "$RESULTS_FILE" | while read line; do
    version=$(echo "$line" | cut -d: -f1)
    size=$(echo "$line" | grep -o '[0-9]\+ bytes' | head -1)
    echo "  ✅ $version - $size"
done

echo ""
echo "Failed versions:"
grep -E "FAIL|PARTIAL|SKIP" "$RESULTS_FILE" | while read line; do
    version=$(echo "$line" | cut -d: -f1)
    reason=$(echo "$line" | cut -d: -f2 | cut -d- -f1 | xargs)
    echo "  ❌ $version - $reason"
done

echo ""
SUCCESS_COUNT=$(grep -c "SUCCESS" "$RESULTS_FILE" || echo "0")
TOTAL_COUNT=$(wc -l < "$RESULTS_FILE")
echo "Results: $SUCCESS_COUNT working / $TOTAL_COUNT total"
echo ""

# Detailed results
echo "========================================"
echo "DETAILED RESULTS"
echo "========================================"
cat "$RESULTS_FILE"
echo ""

if [ "$SUCCESS_COUNT" -gt 0 ]; then
    exit 0
else
    exit 1
fi
