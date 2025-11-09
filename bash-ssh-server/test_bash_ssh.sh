#!/bin/bash
#
# Test script for BASH SSH server
#

set -euo pipefail

PORT=2222
SERVER_SCRIPT="./nano_ssh.sh"
PASSWORD="password123"

log() {
    echo "[TEST] $*"
}

error() {
    echo "[ERROR] $*" >&2
    exit 1
}

# Check dependencies
check_dependencies() {
    log "Checking dependencies..."

    local missing=()

    command -v bash &>/dev/null || missing+=("bash")
    command -v xxd &>/dev/null || missing+=("xxd")
    command -v openssl &>/dev/null || missing+=("openssl")
    command -v bc &>/dev/null || missing+=("bc")
    command -v ssh &>/dev/null || missing+=("ssh")
    command -v sshpass &>/dev/null || missing+=("sshpass")

    if command -v socat &>/dev/null; then
        log "Found socat ✓"
    elif command -v nc &>/dev/null; then
        log "Found nc (netcat) ✓"
    else
        missing+=("socat or nc")
    fi

    if [ ${#missing[@]} -gt 0 ]; then
        error "Missing dependencies: ${missing[*]}"
    fi

    log "All dependencies found ✓"
}

# Start the server
start_server() {
    log "Starting BASH SSH server on port $PORT..."

    if [ ! -x "$SERVER_SCRIPT" ]; then
        chmod +x "$SERVER_SCRIPT"
    fi

    # Start server in background
    "$SERVER_SCRIPT" $PORT > /tmp/bash_ssh_server.log 2>&1 &
    SERVER_PID=$!

    log "Server started with PID: $SERVER_PID"
    log "Waiting for server to be ready..."
    sleep 2

    # Check if server is running
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        error "Server failed to start. Check /tmp/bash_ssh_server.log"
    fi

    log "Server is ready ✓"
}

# Stop the server
stop_server() {
    log "Stopping server..."

    if [ -n "${SERVER_PID:-}" ]; then
        kill $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
    fi

    # Kill any remaining processes
    pkill -f "nano_ssh.sh" || true

    log "Server stopped ✓"
}

# Test SSH connection
test_connection() {
    log "Testing SSH connection..."

    # Wait for port to be available
    local retries=10
    while [ $retries -gt 0 ]; do
        if nc -z localhost $PORT 2>/dev/null; then
            break
        fi
        log "Waiting for port $PORT... ($retries retries left)"
        sleep 1
        retries=$((retries - 1))
    done

    if [ $retries -eq 0 ]; then
        error "Server port $PORT not available"
    fi

    log "Port $PORT is open ✓"

    # Test with sshpass
    log "Connecting with SSH client..."

    local output
    if output=$(echo "$PASSWORD" | sshpass ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p $PORT user@localhost 2>&1); then
        log "SSH connection successful ✓"

        # Check if we received "Hello World"
        if echo "$output" | grep -q "Hello World"; then
            log "Received 'Hello World' ✓"
            log "TEST PASSED! 🎉"
            return 0
        else
            log "WARNING: Connection succeeded but didn't receive 'Hello World'"
            log "Output: $output"
            return 1
        fi
    else
        error "SSH connection failed!"
        log "Output: $output"
        return 1
    fi
}

# Main test sequence
main() {
    log "=== BASH SSH Server Test ==="

    # Ensure cleanup on exit
    trap stop_server EXIT

    check_dependencies
    start_server
    test_connection

    log "=== TEST COMPLETE ==="
}

main "$@"
