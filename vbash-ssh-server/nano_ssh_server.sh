#!/bin/bash
#
# Nano SSH Server - BASH Implementation
# A minimal SSH server written entirely in BASH
# Uses: bash, openssl, xxd, od, dd, nc/socat
#
# Port: 2222
# User: user
# Password: password123
#
# ⚠️  WARNING: THIS VERSION HAS A CRITICAL BUG ⚠️
#
# This implementation stores binary SSH packets in BASH variables, which
# cannot contain null bytes (0x00). This causes packet truncation and
# malformed packets, preventing proper SSH handshake.
#
# ISSUE: In send_packet(), the payload is passed as a string parameter:
#   send_packet "$payload"
# Any null bytes in $payload are truncated, and ${#payload} gives wrong length.
#
# SOLUTION: Use nano_ssh_server_complete.sh instead, which uses file-based
# binary processing to avoid this issue. All binary data is stored in files
# and processed with tools like openssl, dd, and cat.
#
# See: vbash-ssh-server/nano_ssh_server_complete.sh (working version)
# See: vbash-ssh-server/FINAL_ACHIEVEMENT.md (proof it works end-to-end)
#

set -euo pipefail

# Configuration
readonly PORT=2222
readonly SERVER_VERSION="SSH-2.0-BashSSH_0.1"
readonly VALID_USERNAME="user"
readonly VALID_PASSWORD="password123"

# SSH Message Types
readonly SSH_MSG_DISCONNECT=1
readonly SSH_MSG_IGNORE=2
readonly SSH_MSG_KEXINIT=20
readonly SSH_MSG_NEWKEYS=21
readonly SSH_MSG_KEX_ECDH_INIT=30
readonly SSH_MSG_KEX_ECDH_REPLY=31
readonly SSH_MSG_SERVICE_REQUEST=5
readonly SSH_MSG_SERVICE_ACCEPT=6
readonly SSH_MSG_USERAUTH_REQUEST=50
readonly SSH_MSG_USERAUTH_FAILURE=51
readonly SSH_MSG_USERAUTH_SUCCESS=52
readonly SSH_MSG_CHANNEL_OPEN=90
readonly SSH_MSG_CHANNEL_OPEN_CONFIRMATION=91
readonly SSH_MSG_CHANNEL_WINDOW_ADJUST=93
readonly SSH_MSG_CHANNEL_DATA=94
readonly SSH_MSG_CHANNEL_EOF=96
readonly SSH_MSG_CHANNEL_CLOSE=97
readonly SSH_MSG_CHANNEL_REQUEST=98
readonly SSH_MSG_CHANNEL_SUCCESS=99

# Working directory for temporary files
WORKDIR="/tmp/bash_ssh_$$"
mkdir -p "$WORKDIR"
trap 'rm -rf "$WORKDIR"' EXIT

# Logging
log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" >&2
}

# Binary data helpers using od, printf, and dd (no xxd dependency)
hex_to_bin() {
    # Convert hex string to binary using printf
    local hex="$1"
    local i
    for ((i=0; i<${#hex}; i+=2)); do
        printf "\\x${hex:$i:2}"
    done
}

bin_to_hex() {
    # Convert binary to hex string using od
    od -An -tx1 -v | tr -d ' \n'
}

read_bytes() {
    # Read N bytes from stdin
    local n=$1
    dd bs=1 count=$n 2>/dev/null
}

# SSH packet helpers
read_uint32() {
    # Read 4 bytes as big-endian uint32
    local bytes
    bytes=$(read_bytes 4 | bin_to_hex)
    printf "%d" "0x$bytes"
}

write_uint32() {
    # Write uint32 as 4 bytes big-endian
    local val=$1
    local hex
    hex=$(printf "%08x" "$val")
    printf "\\x${hex:0:2}\\x${hex:2:2}\\x${hex:4:2}\\x${hex:6:2}"
}

write_uint8() {
    # Write uint8 as 1 byte
    local val=$1
    printf "\\x$(printf "%02x" "$val")"
}

write_string() {
    # Write SSH string: uint32 length + data
    local str="$1"
    local len=${#str}
    write_uint32 "$len"
    echo -n "$str"
}

write_name_list() {
    # Write SSH name-list (comma-separated algorithms)
    write_string "$1"
}

# Generate server keys on startup
generate_keys() {
    log "Generating server keys..."

    # Generate Ed25519 host key
    openssl genpkey -algorithm ED25519 -out "$WORKDIR/host_key.pem" 2>/dev/null

    # Extract public key
    openssl pkey -in "$WORKDIR/host_key.pem" -pubout -out "$WORKDIR/host_key.pub.pem" 2>/dev/null

    # Generate ephemeral Curve25519 key for key exchange
    openssl genpkey -algorithm X25519 -out "$WORKDIR/server_ephemeral.pem" 2>/dev/null

    log "Keys generated successfully"
}

# Send SSH packet (unencrypted for initial handshake)
send_packet() {
    local payload="$1"
    local payload_len=${#payload}

    # Calculate padding
    local block_size=8
    local padding_len=$(( (block_size - (payload_len + 5) % block_size) % block_size ))
    if [ $padding_len -lt 4 ]; then
        padding_len=$((padding_len + block_size))
    fi

    # Generate random padding
    local padding
    padding=$(dd if=/dev/urandom bs=1 count=$padding_len 2>/dev/null | bin_to_hex)

    # Packet structure: uint32(packet_length) + uint8(padding_length) + payload + padding
    local packet_length=$((1 + payload_len + padding_len))

    {
        write_uint32 "$packet_length"
        write_uint8 "$padding_len"
        echo -n "$payload"
        hex_to_bin "$padding"
    } > "$WORKDIR/packet_out"

    cat "$WORKDIR/packet_out"
}

# Build KEXINIT message
build_kexinit() {
    local output="$WORKDIR/kexinit_payload"

    {
        # Message type
        write_uint8 $SSH_MSG_KEXINIT

        # Cookie (16 random bytes)
        dd if=/dev/urandom bs=16 count=1 2>/dev/null

        # Algorithm lists
        write_name_list "curve25519-sha256,curve25519-sha256@libssh.org"
        write_name_list "ssh-ed25519"
        write_name_list "aes128-ctr,aes256-ctr"
        write_name_list "aes128-ctr,aes256-ctr"
        write_name_list "hmac-sha2-256"
        write_name_list "hmac-sha2-256"
        write_name_list "none"
        write_name_list "none"
        write_name_list ""
        write_name_list ""

        # First kex packet follows (boolean)
        write_uint8 0

        # Reserved
        write_uint32 0

    } > "$output"

    cat "$output"
}

# Protocol version exchange
do_version_exchange() {
    log "Sending server version: $SERVER_VERSION"
    echo "$SERVER_VERSION"

    # Read client version
    local client_version
    read -r client_version
    log "Received client version: $client_version"

    # Store for exchange hash
    echo -n "$client_version" > "$WORKDIR/client_version"
    echo -n "$SERVER_VERSION" > "$WORKDIR/server_version"
}

# Key exchange
do_key_exchange() {
    log "Starting key exchange..."

    # Send server KEXINIT
    local kexinit_payload
    kexinit_payload=$(build_kexinit)
    send_packet "$kexinit_payload"

    # Save our KEXINIT for exchange hash
    echo -n "$kexinit_payload" > "$WORKDIR/server_kexinit"

    log "Waiting for client KEXINIT..."

    # Read client packet (KEXINIT)
    local packet_len
    packet_len=$(read_uint32)
    log "Client packet length: $packet_len"

    local packet_data
    packet_data=$(read_bytes "$packet_len")

    # Extract padding length
    local padding_len
    padding_len=$(echo -n "$packet_data" | head -c 1 | bin_to_hex)
    padding_len=$((0x$padding_len))

    # Extract payload (skip padding_len byte, then get payload)
    local payload_len=$((packet_len - 1 - padding_len))
    local client_kexinit
    client_kexinit=$(echo -n "$packet_data" | tail -c +2 | head -c "$payload_len")

    echo -n "$client_kexinit" > "$WORKDIR/client_kexinit"

    log "Key exchange complete (simplified)"
}

# Simplified authentication (accept any password for now)
do_authentication() {
    log "Waiting for service request..."

    # In a real implementation, we'd parse SSH_MSG_SERVICE_REQUEST
    # For simplicity, just read and send SSH_MSG_SERVICE_ACCEPT

    # Read client packets in a loop until authentication
    local authenticated=0
    local attempts=0

    while [ $authenticated -eq 0 ] && [ $attempts -lt 10 ]; do
        attempts=$((attempts + 1))

        # Try to read a packet
        local packet_len
        packet_len=$(read_uint32 || echo "0")

        if [ "$packet_len" -eq 0 ]; then
            log "No more data from client"
            break
        fi

        log "Auth packet length: $packet_len"

        local packet_data
        packet_data=$(read_bytes "$packet_len")

        # Get message type (first byte of payload)
        local msg_type
        msg_type=$(echo -n "$packet_data" | tail -c +2 | head -c 1 | bin_to_hex)
        msg_type=$((0x$msg_type))

        log "Message type: $msg_type"

        if [ $msg_type -eq $SSH_MSG_SERVICE_REQUEST ]; then
            log "Received SERVICE_REQUEST, sending SERVICE_ACCEPT"

            # Send SSH_MSG_SERVICE_ACCEPT
            local service_accept
            service_accept=$(
                write_uint8 $SSH_MSG_SERVICE_ACCEPT
                write_string "ssh-userauth"
            )
            send_packet "$service_accept"

        elif [ $msg_type -eq $SSH_MSG_USERAUTH_REQUEST ]; then
            log "Received USERAUTH_REQUEST, sending SUCCESS"

            # Send SSH_MSG_USERAUTH_SUCCESS
            local auth_success
            auth_success=$(write_uint8 $SSH_MSG_USERAUTH_SUCCESS)
            send_packet "$auth_success"

            authenticated=1
        fi
    done

    log "Authentication phase complete"
}

# Channel establishment and data transfer
do_channel() {
    log "Waiting for channel open..."

    # Read packets until we get CHANNEL_OPEN
    local attempts=0

    while [ $attempts -lt 10 ]; do
        attempts=$((attempts + 1))

        local packet_len
        packet_len=$(read_uint32 || echo "0")

        if [ "$packet_len" -eq 0 ]; then
            log "No more data from client"
            break
        fi

        log "Channel packet length: $packet_len"

        local packet_data
        packet_data=$(read_bytes "$packet_len")

        # Get message type
        local msg_type
        msg_type=$(echo -n "$packet_data" | tail -c +2 | head -c 1 | bin_to_hex)
        msg_type=$((0x$msg_type))

        log "Message type: $msg_type"

        if [ $msg_type -eq $SSH_MSG_CHANNEL_OPEN ]; then
            log "Received CHANNEL_OPEN, sending CONFIRMATION"

            # Parse channel data (simplified - use fixed values)
            local sender_channel=0
            local initial_window=2097152
            local max_packet=32768

            # Send SSH_MSG_CHANNEL_OPEN_CONFIRMATION
            local channel_confirm
            channel_confirm=$(
                write_uint8 $SSH_MSG_CHANNEL_OPEN_CONFIRMATION
                write_uint32 "$sender_channel"      # recipient channel
                write_uint32 0                       # sender channel
                write_uint32 "$initial_window"      # initial window size
                write_uint32 "$max_packet"          # maximum packet size
            )
            send_packet "$channel_confirm"

        elif [ $msg_type -eq $SSH_MSG_CHANNEL_REQUEST ]; then
            log "Received CHANNEL_REQUEST"

            # Send SSH_MSG_CHANNEL_SUCCESS
            local channel_success
            channel_success=$(
                write_uint8 $SSH_MSG_CHANNEL_SUCCESS
                write_uint32 0  # recipient channel
            )
            send_packet "$channel_success"

            # Send "Hello World" message
            log "Sending Hello World data..."
            local data_msg
            data_msg=$(
                write_uint8 $SSH_MSG_CHANNEL_DATA
                write_uint32 0  # recipient channel
                write_string "Hello World
"
            )
            send_packet "$data_msg"

            # Send EOF
            local eof_msg
            eof_msg=$(
                write_uint8 $SSH_MSG_CHANNEL_EOF
                write_uint32 0  # recipient channel
            )
            send_packet "$eof_msg"

            break
        fi
    done

    log "Channel session complete"
}

# Main SSH session handler
handle_session() {
    log "New connection"

    # SSH protocol phases:
    # 1. Version exchange
    do_version_exchange

    # 2. Key exchange
    do_key_exchange

    # 3. Service request / Authentication
    do_authentication

    # 4. Channel open and data transfer
    do_channel

    log "Session complete"
}

# Main server loop
main() {
    log "BASH SSH Server starting on port $PORT"

    # Generate keys
    generate_keys

    log "Server ready, waiting for connections..."
    log "Test with: ssh -p $PORT $VALID_USERNAME@localhost"

    # Use socat to listen and handle connections
    # Each connection is handled by a new instance of this script
    if command -v socat >/dev/null 2>&1; then
        socat TCP-LISTEN:$PORT,reuseaddr,fork EXEC:"$0 --handle-session"
    elif command -v nc >/dev/null 2>&1; then
        # Fallback to nc (less reliable)
        while true; do
            nc -l -p $PORT -c "$0 --handle-session"
        done
    else
        log "ERROR: Neither socat nor nc found. Please install socat."
        exit 1
    fi
}

# Entry point
if [ "${1:-}" = "--handle-session" ]; then
    handle_session
else
    main
fi
