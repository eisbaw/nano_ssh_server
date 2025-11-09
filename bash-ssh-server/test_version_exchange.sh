#!/bin/bash
# Test SSH version exchange only

SERVER_VERSION="SSH-2.0-BashSSH_0.1"

echo "[SERVER] Starting..." >&2
echo -en "${SERVER_VERSION}\r\n"
echo "[SERVER] Sent version: $SERVER_VERSION" >&2

echo "[SERVER] Waiting for client..." >&2
read -r CLIENT_VERSION
CLIENT_VERSION="${CLIENT_VERSION%$'\r'}"

echo "[SERVER] Received client version: $CLIENT_VERSION" >&2

if [[ "$CLIENT_VERSION" =~ ^SSH-2\.0- ]]; then
    echo "[SERVER] Version exchange OK!" >&2
    echo "OK"
else
    echo "[SERVER] Invalid version!" >&2
    echo "ERROR"
fi
