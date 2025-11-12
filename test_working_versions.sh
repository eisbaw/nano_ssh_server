#!/usr/bin/env bash
# Test the working versions to see if they actually function

echo "=== Testing Working SSH Server Versions ==="
echo ""

test_version() {
    version=$1
    binary="/home/user/nano_ssh_server/$version/nano_ssh_server"

    if [ ! -f "$binary" ]; then
        echo "❌ $version: Binary not found at $binary"
        return 1
    fi

    echo "Testing $version..."

    # Start the server in background
    cd "/home/user/nano_ssh_server/$version"
    timeout 5 ./nano_ssh_server &
    server_pid=$!

    # Give it a moment to start
    sleep 1

    # Try to connect with SSH
    timeout 3 ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
        -o ConnectTimeout=2 -p 2222 user@localhost "echo test" 2>&1 | head -5

    result=$?

    # Kill the server
    kill $server_pid 2>/dev/null
    wait $server_pid 2>/dev/null

    if [ $result -eq 0 ]; then
        echo "✅ $version: Working!"
        return 0
    elif [ $result -eq 124 ]; then
        echo "⚠️  $version: Timeout (server may not be responding)"
        return 1
    else
        echo "⚠️  $version: Connection failed (exit code: $result)"
        return 1
    fi
}

# Test each working version
for version in v15-crypto v16-crypto-standalone; do
    test_version "$version"
    echo ""
    cd /home/user/nano_ssh_server
done

echo "=== Test Complete ==="
