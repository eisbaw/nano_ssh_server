#!/bin/bash
set -e

echo "Starting SSH server..."
./nano_ssh_server 2>&1 &
SERVER_PID=$!
sleep 2

echo "Testing SSH connection..."
echo "=========================================="

timeout 10 ssh -vv \
    -o StrictHostKeyChecking=no \
    -o UserKnownHostsFile=/dev/null \
    -o HostKeyAlgorithms=+ssh-rsa \
    -o PubkeyAcceptedKeyTypes=+ssh-rsa \
    -o PreferredAuthentications=password \
    -o PubkeyAuthentication=no \
    -p 2222 root@127.0.0.1 \
    "echo 'SUCCESS: Command executed!'" 2>&1 | tee /tmp/ssh_test_output.log

echo ""
echo "=========================================="
echo "Checking for key indicators..."
grep -E "(bignum is negative|SSH2_MSG_KEX_ECDH_REPLY|Authenticated|SUCCESS:)" /tmp/ssh_test_output.log || echo "No matches found"

kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true
