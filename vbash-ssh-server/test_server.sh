#!/bin/bash
#
# Test script for BASH SSH Server
#

set -euo pipefail

readonly PORT=2222
readonly USER="user"
readonly PASSWORD="password123"
readonly TIMEOUT=10

# Colors for output
readonly RED='\033[0;31m'
readonly GREEN='\033[0;32m'
readonly YELLOW='\033[1;33m'
readonly NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $*"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

# Check if server is running
check_server() {
    log_info "Checking if server is running on port $PORT..."

    if nc -z localhost "$PORT" 2>/dev/null; then
        log_info "✓ Server is listening on port $PORT"
        return 0
    else
        log_error "✗ Server is not running on port $PORT"
        return 1
    fi
}

# Test 1: Version exchange
test_version_exchange() {
    log_info "Test 1: SSH version exchange"

    local version
    version=$(timeout 2 bash -c "exec 3<>/dev/tcp/localhost/$PORT; cat <&3 | head -1" 2>/dev/null || echo "")

    if [[ "$version" =~ ^SSH-2\.0- ]]; then
        log_info "✓ Received SSH version: $version"
        return 0
    else
        log_error "✗ Invalid or no version received"
        return 1
    fi
}

# Test 2: Connection with SSH client (manual)
test_ssh_client_manual() {
    log_info "Test 2: SSH client connection (manual)"
    log_warn "Please run in another terminal:"
    log_warn "  ssh -p $PORT $USER@localhost"
    log_warn "  Password: $PASSWORD"
    log_info "Press Enter when ready to continue..."
    read -r
}

# Test 3: Connection with sshpass (automated)
test_ssh_client_auto() {
    log_info "Test 3: SSH client connection (automated with sshpass)"

    if ! command -v sshpass >/dev/null 2>&1; then
        log_warn "✗ sshpass not installed, skipping automated test"
        log_info "Install with: sudo apt-get install sshpass"
        return 0
    fi

    log_info "Connecting with sshpass..."

    # Try to connect and execute a command
    local output
    output=$(timeout $TIMEOUT sshpass -p "$PASSWORD" ssh \
        -o StrictHostKeyChecking=no \
        -o UserKnownHostsFile=/dev/null \
        -o LogLevel=ERROR \
        -p "$PORT" "$USER@localhost" "echo 'Connected successfully'" 2>&1 || echo "")

    if [[ "$output" =~ "Connected successfully" ]] || [[ "$output" =~ "Hello World" ]]; then
        log_info "✓ SSH connection successful"
        log_info "Output: $output"
        return 0
    else
        log_warn "✗ SSH connection failed or unexpected output"
        log_info "Output: $output"
        return 1
    fi
}

# Test 4: Stress test (multiple connections)
test_multiple_connections() {
    log_info "Test 4: Multiple connections"

    local success=0
    local total=5

    for i in $(seq 1 $total); do
        log_info "Connection $i/$total..."

        if timeout 2 bash -c "exec 3<>/dev/tcp/localhost/$PORT; cat <&3 | head -1" 2>/dev/null | grep -q "SSH-2.0"; then
            ((success++))
        fi

        sleep 1
    done

    log_info "✓ Successful connections: $success/$total"

    if [ $success -eq $total ]; then
        return 0
    else
        return 1
    fi
}

# Main test runner
main() {
    log_info "================================================"
    log_info "BASH SSH Server Test Suite"
    log_info "================================================"
    echo ""

    # Check dependencies
    log_info "Checking dependencies..."
    for cmd in nc timeout bash; do
        if ! command -v $cmd >/dev/null 2>&1; then
            log_error "Required command not found: $cmd"
            exit 1
        fi
    done
    log_info "✓ All required dependencies found"
    echo ""

    # Check if server is running
    if ! check_server; then
        log_error "Please start the server first:"
        log_error "  ./nano_ssh_server.sh"
        exit 1
    fi
    echo ""

    # Run tests
    local passed=0
    local total=0

    # Test 1: Version exchange
    ((total++))
    if test_version_exchange; then
        ((passed++))
    fi
    echo ""

    # Test 2: Multiple connections
    ((total++))
    if test_multiple_connections; then
        ((passed++))
    fi
    echo ""

    # Test 3: SSH client (automated)
    ((total++))
    if test_ssh_client_auto; then
        ((passed++))
    fi
    echo ""

    # Summary
    log_info "================================================"
    log_info "Test Results: $passed/$total passed"
    log_info "================================================"

    if [ $passed -eq $total ]; then
        log_info "✓ All tests passed!"
        exit 0
    else
        log_warn "✗ Some tests failed"
        exit 1
    fi
}

# Run tests
main "$@"
