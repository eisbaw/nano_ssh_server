#!/bin/bash
cd /home/user/nano_ssh_server/v15-crypto
./nano_ssh_server &
SERVER_PID=$!
sleep 2

echo "Testing v15-crypto..."
timeout 10 ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o HostKeyAlgorithms=+ssh-rsa -o PreferredAuthentications=password -o PubkeyAuthentication=no -p 2222 root@127.0.0.1 "echo 'V15 WORKS!'" 2>&1 | grep -E "(WORKS|Authenticated|negative|Connection closed)"

kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null
