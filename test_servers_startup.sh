#!/bin/bash
# Test if servers can start and listen on port 2222

echo "=== Testing SSH Server Startup ==="
echo ""

test_startup() {
    version=$1
    binary="/home/user/nano_ssh_server/$version/nano_ssh_server"

    if [ ! -f "$binary" ]; then
        echo "❌ $version: Binary not found"
        return 1
    fi

    echo "Testing $version..."

    # Start the server in background
    cd "/home/user/nano_ssh_server/$version"
    timeout 3 ./nano_ssh_server &
    server_pid=$!

    # Give it a moment to start
    sleep 0.5

    # Check if it's listening on port 2222
    if netstat -tln 2>/dev/null | grep -q ":2222 " || ss -tln 2>/dev/null | grep -q ":2222 "; then
        echo "✅ $version: Server started and listening on port 2222"
        status="Working"
    else
        echo "⚠️  $version: Server started but not listening on port 2222"
        status="Not Listening"
    fi

    # Check if process is still running
    if ps -p $server_pid > /dev/null 2>&1; then
        echo "   Process is running (PID: $server_pid)"
    else
        echo "   Process exited early"
        status="Crashed"
    fi

    # Kill the server
    kill $server_pid 2>/dev/null
    wait $server_pid 2>/dev/null

    cd /home/user/nano_ssh_server
    echo "   Status: $status"
    echo ""
}

# Test each working version
for version in v15-crypto v16-crypto-standalone; do
    test_startup "$version"
done

echo "=== Test Complete ==="
