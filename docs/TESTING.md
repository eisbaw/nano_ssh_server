# Testing Guide for Nano SSH Server

## Overview

Testing is CRITICAL. Do not proceed to optimization until ALL tests pass consistently.

## Test Environment Setup

### Using Nix Shell

```bash
nix-shell
# This provides:
# - openssh client
# - gcc, make, just
# - valgrind
# - tcpdump (optional, for packet inspection)
```

## Testing Phases

### Phase 1: TCP Connection

**Objective:** Verify server accepts connections

```bash
# Terminal 1: Start server
./v0-vanilla/nano_ssh_server

# Terminal 2: Test with netcat
nc localhost 2222

# Expected: Connection accepted
# Server should be listening and accepting
```

### Phase 2: Version Exchange

**Objective:** Verify SSH version string exchange

```bash
# Terminal 1: Start server
./v0-vanilla/nano_ssh_server

# Terminal 2: Test with netcat
nc localhost 2222

# Expected output (immediately):
SSH-2.0-NanoSSH_0.1

# Type this and press Enter:
SSH-2.0-TestClient

# Expected: Server accepts it
# (May disconnect after, that's okay at this stage)
```

### Phase 3: SSH Client Handshake

**Objective:** Verify KEXINIT exchange

```bash
# Start server
./v0-vanilla/nano_ssh_server

# Connect with verbose SSH client
ssh -vvv -p 2222 localhost

# Look for in output:
# - "debug1: SSH2_MSG_KEXINIT sent"
# - "debug1: SSH2_MSG_KEXINIT received"
# - Algorithm negotiation messages

# May fail after this, that's expected until key exchange is implemented
```

### Phase 4: Key Exchange Complete

**Objective:** Verify Diffie-Hellman key exchange

```bash
ssh -vvv -p 2222 localhost

# Look for:
# - "debug1: expecting SSH2_MSG_KEX_ECDH_REPLY"
# - "debug1: Server host key: ..."
# - "debug1: SSH2_MSG_NEWKEYS sent"
# - "debug1: expecting SSH2_MSG_NEWKEYS"
# - "debug1: SSH2_MSG_NEWKEYS received"

# Should see "encryption enabled" or similar
```

### Phase 5: Authentication

**Objective:** Verify password authentication

```bash
ssh -p 2222 user@localhost

# Should prompt for password
# Enter: password123 (or your configured password)

# Expected: "Authenticated to localhost"
```

### Phase 6: Full Connection

**Objective:** Receive "Hello World" message

```bash
ssh -p 2222 user@localhost

# Expected output:
# Hello World
# (then disconnect)
```

## Automated Test Scripts

### tests/test_version.sh

```bash
#!/bin/bash
# Test version exchange

# Start server in background
./v0-vanilla/nano_ssh_server &
SERVER_PID=$!
sleep 1

# Test version exchange
echo "SSH-2.0-TestClient" | nc localhost 2222 > /tmp/test_output.txt

# Check for server version
if grep -q "SSH-2.0-NanoSSH" /tmp/test_output.txt; then
    echo "PASS: Version exchange"
    EXIT=0
else
    echo "FAIL: Version exchange"
    EXIT=1
fi

# Cleanup
kill $SERVER_PID 2>/dev/null
rm /tmp/test_output.txt
exit $EXIT
```

### tests/test_connection.sh

```bash
#!/bin/bash
# Test full SSH connection

# Start server in background
./v0-vanilla/nano_ssh_server &
SERVER_PID=$!
sleep 1

# Set test password
export SSHPASS="password123"

# Test connection (requires sshpass for automation)
sshpass -e ssh -o StrictHostKeyChecking=no \
               -o UserKnownHostsFile=/dev/null \
               -p 2222 user@localhost > /tmp/test_output.txt 2>&1

# Check for "Hello World"
if grep -q "Hello World" /tmp/test_output.txt; then
    echo "PASS: Full connection"
    EXIT=0
else
    echo "FAIL: Full connection"
    cat /tmp/test_output.txt
    EXIT=1
fi

# Cleanup
kill $SERVER_PID 2>/dev/null
rm /tmp/test_output.txt
exit $EXIT
```

### tests/test_auth.sh

```bash
#!/bin/bash
# Test authentication failure and success

./v0-vanilla/nano_ssh_server &
SERVER_PID=$!
sleep 1

# Test wrong password
export SSHPASS="wrongpassword"
if sshpass -e ssh -o StrictHostKeyChecking=no \
                   -o UserKnownHostsFile=/dev/null \
                   -p 2222 user@localhost 2>&1 | grep -q "Permission denied"; then
    echo "PASS: Authentication rejection"
else
    echo "FAIL: Should reject wrong password"
    kill $SERVER_PID 2>/dev/null
    exit 1
fi

# Test correct password
export SSHPASS="password123"
if sshpass -e ssh -o StrictHostKeyChecking=no \
                   -o UserKnownHostsFile=/dev/null \
                   -p 2222 user@localhost 2>&1 | grep -q "Hello World"; then
    echo "PASS: Authentication success"
    EXIT=0
else
    echo "FAIL: Authentication success"
    EXIT=1
fi

kill $SERVER_PID 2>/dev/null
exit $EXIT
```

### tests/run_all.sh

```bash
#!/bin/bash
# Run all tests

TESTS=(
    "tests/test_version.sh"
    "tests/test_connection.sh"
    "tests/test_auth.sh"
)

PASSED=0
FAILED=0

for test in "${TESTS[@]}"; do
    echo "Running $test..."
    if bash "$test"; then
        ((PASSED++))
    else
        ((FAILED++))
    fi
    echo
    sleep 1  # Give server time to clean up
done

echo "Results: $PASSED passed, $FAILED failed"
exit $FAILED
```

## Manual Testing Procedures

### Test 1: Basic Connection

```bash
# Terminal 1
just run v0-vanilla

# Terminal 2
just connect

# Expected: Prompt for password, then "Hello World"
```

### Test 2: Multiple Connections

```bash
# Terminal 1
just run v0-vanilla

# Terminal 2
just connect

# Terminal 3 (while Terminal 2 still connected)
just connect

# Expected: Second connection handled (if server supports it)
# or rejected gracefully
```

### Test 3: Disconnect Handling

```bash
# Terminal 1
just run v0-vanilla

# Terminal 2
ssh -p 2222 user@localhost
# Press Ctrl+C immediately after connecting

# Terminal 1
# Expected: Server doesn't crash, handles disconnect gracefully
```

### Test 4: Invalid Data

```bash
# Terminal 1
just run v0-vanilla

# Terminal 2
echo "GARBAGE DATA" | nc localhost 2222

# Terminal 1
# Expected: Server rejects gracefully, doesn't crash
```

### Test 5: Memory Leaks

```bash
valgrind --leak-check=full --show-leak-kinds=all ./v0-vanilla/nano_ssh_server

# In another terminal, connect and disconnect
ssh -p 2222 user@localhost

# Stop server (Ctrl+C)

# Expected: No memory leaks reported
```

## Debugging with Verbose SSH Client

### Level 1 Verbosity
```bash
ssh -v -p 2222 user@localhost
# Shows basic connection info
```

### Level 2 Verbosity
```bash
ssh -vv -p 2222 user@localhost
# Shows protocol details
```

### Level 3 Verbosity
```bash
ssh -vvv -p 2222 user@localhost
# Shows everything, including key exchange details
```

## Common Issues and Debugging

### Issue: "Connection closed by remote host"

**After version exchange:**
- Check KEXINIT message format
- Verify algorithm lists are valid
- Check packet framing (length, padding)

**After KEXINIT:**
- Check key exchange implementation
- Verify exchange hash calculation
- Check signature format

**After NEWKEYS:**
- Check encryption is enabled correctly
- Verify key derivation
- Check sequence numbers

### Issue: "No matching key exchange method found"

**Problem:** Client doesn't support our algorithms

**Solution:**
```bash
# Force client to use our algorithms
ssh -o KexAlgorithms=curve25519-sha256 \
    -o HostKeyAlgorithms=ssh-ed25519 \
    -o Ciphers=chacha20-poly1305@openssh.com \
    -p 2222 user@localhost
```

### Issue: "Corrupted MAC on input"

**Problem:** Encryption/MAC implementation issue

**Debug steps:**
1. Verify key derivation
2. Check sequence numbers (must increment)
3. Verify MAC calculation includes sequence number
4. Check packet format (length prefix, padding)

### Issue: "Permission denied"

**After password entered:**
- Check password comparison logic
- Verify string parsing (length prefix)
- Check authentication message format

## Packet Capture for Debugging

### Capture SSH traffic

```bash
# Terminal 1
sudo tcpdump -i lo -w ssh_capture.pcap port 2222

# Terminal 2
just run v0-vanilla

# Terminal 3
ssh -p 2222 user@localhost

# Stop tcpdump (Ctrl+C in Terminal 1)

# Analyze with Wireshark
wireshark ssh_capture.pcap
```

**Note:** Wireshark can decode SSH protocol, showing message types and fields.

## Comparing Versions

### Binary Size Comparison

```bash
just size-report

# Expected output:
# v0-vanilla:  156KB
# v1-portable: 168KB
# v2-opt1:      98KB
# v3-opt2:      72KB
# ...
```

### Functional Testing All Versions

```bash
just test-all

# Runs test suite against each version
# Ensures optimizations don't break functionality
```

## Stress Testing (Optional)

### Multiple Rapid Connections

```bash
# Server
just run v0-vanilla

# Client
for i in {1..100}; do
    ssh -p 2222 user@localhost echo "Test $i"
    sleep 0.1
done

# Expected: All connections succeed
```

### Long-Running Test

```bash
# Server
just run v0-vanilla

# Client (leave running)
while true; do
    ssh -p 2222 user@localhost
    sleep 5
done

# Monitor server for memory leaks, crashes
```

## Performance Testing

### Connection Time

```bash
time ssh -p 2222 user@localhost

# Measure: real time taken
# Goal: < 1 second for connection + auth + data
```

### Memory Usage

```bash
# Start server
./v0-vanilla/nano_ssh_server &
PID=$!

# Monitor memory
watch -n 1 "ps -o pid,vsz,rss,cmd -p $PID"

# Connect from another terminal
ssh -p 2222 user@localhost

# Watch memory during connection
```

## Test Checklist (Per Version)

- [ ] Server starts without errors
- [ ] Accepts TCP connections
- [ ] Sends version string
- [ ] Receives and validates client version
- [ ] Sends KEXINIT
- [ ] Receives and parses client KEXINIT
- [ ] Completes key exchange
- [ ] Sends/receives NEWKEYS
- [ ] Enables encryption correctly
- [ ] Accepts SERVICE_REQUEST
- [ ] Receives authentication request
- [ ] Validates password
- [ ] Accepts channel open
- [ ] Handles channel requests
- [ ] Sends data to client
- [ ] Closes channel cleanly
- [ ] No memory leaks (valgrind)
- [ ] No crashes on disconnect
- [ ] Handles invalid input gracefully

## Success Criteria

A version is considered **WORKING** when:

1. Standard `ssh` client connects successfully
2. Password authentication works
3. "Hello World" message is received
4. Connection closes cleanly
5. No memory leaks
6. No crashes during normal operation
7. Passes all automated tests

Only after meeting these criteria should you proceed to optimization.

## Regression Testing

When creating new optimized versions:

```bash
# Ensure new version passes same tests as previous
just test v3-opt2
just test v0-vanilla

# Compare results - should be identical
```

## Documentation of Test Results

Keep a test log:

```
Version: v0-vanilla
Date: 2024-01-15
Binary Size: 156KB
Test Results:
  - Version exchange: PASS
  - Key exchange: PASS
  - Authentication: PASS
  - Full connection: PASS
  - Memory leaks: NONE
  - Crash on disconnect: NONE

Version: v2-opt1
Date: 2024-01-16
Binary Size: 98KB
Test Results:
  - All tests: PASS
  - 37% size reduction from v0
  - Performance: Similar to v0
```

This helps track progress and ensure optimizations don't introduce bugs.
