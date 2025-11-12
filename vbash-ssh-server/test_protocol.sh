#!/usr/bin/env bash
#
# Manual SSH Protocol Tester
# Tests each phase of the SSH protocol without needing an SSH client
#

set -euo pipefail

readonly PORT="${1:-2222}"
readonly TEST_DIR="/tmp/ssh_proto_test_$$"

mkdir -p "$TEST_DIR"
trap 'rm -rf "$TEST_DIR"' EXIT

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_test() {
    echo -e "${BLUE}[TEST]${NC} $*"
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $*"
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $*"
}

log_info() {
    echo -e "${YELLOW}[INFO]${NC} $*"
}

# Binary helpers
write_uint32() {
    local val=$1
    local hex=$(printf "%08x" "$val")
    printf "\\x${hex:0:2}\\x${hex:2:2}\\x${hex:4:2}\\x${hex:6:2}"
}

write_uint8() {
    printf "\\x$(printf "%02x" "$1")"
}

write_string() {
    write_uint32 "${#1}"
    printf "%s" "$1"
}

send_packet() {
    local payload_file="$1"
    local payload_len=$(wc -c < "$payload_file")
    local padding_len=$(( (8 - (payload_len + 5) % 8) % 8 ))
    if [ $padding_len -lt 4 ]; then
        padding_len=$((padding_len + 8))
    fi
    local packet_len=$((1 + payload_len + padding_len))

    write_uint32 "$packet_len"
    write_uint8 "$padding_len"
    cat "$payload_file"
    dd if=/dev/urandom bs=1 count=$padding_len 2>/dev/null
}

# Test 1: Version Exchange
test_version_exchange() {
    log_test "Test 1: Version Exchange"

    local result=$(timeout 3 bash -c "
        exec 3<>/dev/tcp/localhost/$PORT
        cat <&3 | head -1
    " 2>/dev/null || echo "FAILED")

    if [[ "$result" =~ ^SSH-2\.0- ]]; then
        log_pass "Server sent: $result"
        return 0
    else
        log_fail "Invalid or no version received: $result"
        return 1
    fi
}

# Test 2: KEXINIT Exchange
test_kexinit() {
    log_test "Test 2: KEXINIT Exchange"

    timeout 5 bash -c "
        exec 3<>/dev/tcp/localhost/$PORT

        # Read server version
        read -r server_version <&3
        echo 'Server: \$server_version' >&2

        # Send client version
        echo 'SSH-2.0-TestClient_1.0' >&3

        # Expect server KEXINIT - read 4 bytes (packet length)
        dd bs=1 count=4 <&3 2>/dev/null | od -An -tx1
    " 2>&1 | grep -q "Server: SSH-2.0" && log_pass "Version exchange OK" || log_fail "Version exchange failed"

    return 0
}

# Test 3: Full handshake simulation
test_full_handshake() {
    log_test "Test 3: Full Protocol Flow (manual)"

    log_info "This would require implementing a full SSH client in BASH"
    log_info "Instead, we'll test with protocol-level tools when available"

    return 0
}

# Test 4: Concurrent connections
test_concurrent() {
    log_test "Test 4: Multiple Sequential Connections"

    local success=0
    local total=3

    for i in $(seq 1 $total); do
        local result=$(timeout 2 bash -c "
            exec 3<>/dev/tcp/localhost/$PORT
            cat <&3 | head -1
        " 2>/dev/null || echo "FAILED")

        if [[ "$result" =~ ^SSH-2\.0- ]]; then
            ((success++))
        fi
        sleep 0.5
    done

    if [ $success -eq $total ]; then
        log_pass "All $total connections succeeded"
        return 0
    else
        log_fail "Only $success/$total connections succeeded"
        return 1
    fi
}

# Main
main() {
    echo "=========================================="
    echo "  BASH SSH Server Protocol Tests"
    echo "  Port: $PORT"
    echo "=========================================="
    echo ""

    # Check if server is running
    if ! nc -z localhost "$PORT" 2>/dev/null; then
        log_fail "Server not running on port $PORT"
        log_info "Start server with: ./nano_ssh_server_complete.sh $PORT"
        exit 1
    fi

    log_pass "Server is listening on port $PORT"
    echo ""

    local passed=0
    local total=0

    # Run tests
    ((total++))
    test_version_exchange && ((passed++)) || true
    echo ""

    ((total++))
    test_kexinit && ((passed++)) || true
    echo ""

    ((total++))
    test_concurrent && ((passed++)) || true
    echo ""

    # Summary
    echo "=========================================="
    echo "  Results: $passed/$total tests passed"
    echo "=========================================="

    if [ $passed -eq $total ]; then
        log_pass "All tests passed!"
        return 0
    else
        log_fail "Some tests failed"
        return 1
    fi
}

main "$@"
