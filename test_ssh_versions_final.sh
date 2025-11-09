#!/bin/bash
# Test SSH server versions with real SSH client
# All versions use port 2222, so we test sequentially

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Config
PORT=2222
PASSWORD="password123"
USER="user"

# Versions to test (mentioned in README)
VERSIONS=(
    "v15-crypto:Recommended for embedded"
    "v16-crypto-standalone:100% standalone"
    "v17-from14:Almost independent"
    "v18-selfcontained:Self-contained"
    "v19-donna:Donna implementation"
    "v20-opt:Latest optimized"
    "v0-vanilla:Baseline"
    "v14-opt12:Smallest"
    "v12-static:Fully static"
)

# Stats
TOTAL=0
PASSED=0
FAILED=0
SKIPPED=0

echo "=============================================="
echo "   SSH Server Test Suite"
echo "   Port: $PORT | User: $USER | Pass: $PASSWORD"
echo "=============================================="
echo ""

# Test one version
test_version() {
    local ver_info="$1"
    local version=$(echo "$ver_info" | cut -d: -f1)
    local description=$(echo "$ver_info" | cut -d: -f2-)
    local binary="/home/user/nano_ssh_server/$version/nano_ssh_server"

    ((TOTAL++))

    printf "${BLUE}[%2d/%2d]${NC} %-25s " "$TOTAL" "${#VERSIONS[@]}" "$version"

    # Check binary exists
    if [ ! -f "$binary" ]; then
        echo -e "${YELLOW}SKIP${NC} (not built)"
        ((SKIPPED++))
        return
    fi

    # Kill any existing server
    pkill -9 nano_ssh_server 2>/dev/null || true
    sleep 0.5

    # Start server (don't redirect output - some servers need it)
    cd "/home/user/nano_ssh_server/$version"
    ./nano_ssh_server > /tmp/ssh_server_$version.log 2>&1 &
    local pid=$!

    # Wait for it to start (longer for RSA key gen)
    sleep 5

    # Test SSH connection (enable RSA for older versions)
    local result=$(sshpass -p "$PASSWORD" ssh \
        -o StrictHostKeyChecking=no \
        -o UserKnownHostsFile=/dev/null \
        -o PubkeyAcceptedKeyTypes=+ssh-rsa \
        -o HostKeyAlgorithms=+ssh-rsa \
        -o ConnectTimeout=3 \
        -p $PORT \
        "$USER@localhost" \
        "echo 'Hello World'" 2>&1 || true)

    # Kill server
    kill -9 $pid 2>/dev/null || true
    pkill -9 nano_ssh_server 2>/dev/null || true
    sleep 0.5

    # Check result
    if echo "$result" | grep -q "Hello World"; then
        echo -e "${GREEN}PASS${NC} ✅"
        ((PASSED++))
    else
        echo -e "${RED}FAIL${NC} ❌"
        ((FAILED++))
        if [ -n "$VERBOSE" ]; then
            echo "       $(echo "$result" | head -1)"
        fi
    fi

    cd /home/user/nano_ssh_server
}

# Run tests
for ver_info in "${VERSIONS[@]}"; do
    test_version "$ver_info"
done

# Summary
echo ""
echo "=============================================="
echo "                 RESULTS"
echo "=============================================="
echo ""
printf "Total:   %2d\n" "$TOTAL"
printf "${GREEN}Passed:  %2d${NC}\n" "$PASSED"
printf "${RED}Failed:  %2d${NC}\n" "$FAILED"
printf "${YELLOW}Skipped: %2d${NC}\n" "$SKIPPED"
echo ""

if [ $PASSED -gt 0 ]; then
    PERCENT=$((PASSED * 100 / (PASSED + FAILED)))
    if [ $FAILED -eq 0 ]; then
        echo -e "${GREEN}✅ All built versions pass!${NC}"
    else
        echo -e "${YELLOW}⚠️  $PASSED/$((PASSED+FAILED)) tests passed ($PERCENT%)${NC}"
    fi
fi

echo ""
echo "=============================================="

# Return success if at least some passed
[ $PASSED -gt 0 ]
