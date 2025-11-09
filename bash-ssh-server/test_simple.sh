#!/bin/bash
# Simple test to debug version exchange

set -x

echo "Testing basic version exchange..."

# Start a simple TCP server that just does version exchange
(
    echo "SSH-2.0-BashSSH_0.1"
    read -r client_version
    echo "Got from client: $client_version" >&2
) | nc -l -p 3333 &

sleep 2

# Try to connect
echo "Connecting with netcat..."
echo "SSH-2.0-TestClient" | nc localhost 3333

sleep 1
pkill -f "nc -l -p 3333"
