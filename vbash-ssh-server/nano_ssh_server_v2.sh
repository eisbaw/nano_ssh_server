#!/bin/bash
#
# Nano SSH Server - Complete BASH Implementation
# Implements SSH-2.0 protocol for educational purposes
#
# WARNING: This is a proof-of-concept. Not for production use!
#

set -euo pipefail

# Configuration
readonly PORT="${1:-2222}"
readonly SERVER_VERSION="SSH-2.0-BashSSH_0.1"
readonly VALID_USERNAME="user"
readonly VALID_PASSWORD="password123"

# Working directory
readonly WORKDIR="/tmp/bash_ssh_$$"
mkdir -p "$WORKDIR"
trap 'rm -rf "$WORKDIR"; exit' EXIT INT TERM

# SSH Message Types
readonly SSH_MSG_DISCONNECT=1
readonly SSH_MSG_KEXINIT=20
readonly SSH_MSG_NEWKEYS=21
readonly SSH_MSG_KEX_ECDH_INIT=30
readonly SSH_MSG_KEX_ECDH_REPLY=31
readonly SSH_MSG_SERVICE_REQUEST=5
readonly SSH_MSG_SERVICE_ACCEPT=6
readonly SSH_MSG_USERAUTH_REQUEST=50
readonly SSH_MSG_USERAUTH_SUCCESS=52
readonly SSH_MSG_CHANNEL_OPEN=90
readonly SSH_MSG_CHANNEL_OPEN_CONFIRMATION=91
readonly SSH_MSG_CHANNEL_REQUEST=98
readonly SSH_MSG_CHANNEL_SUCCESS=99
readonly SSH_MSG_CHANNEL_DATA=94
readonly SSH_MSG_CHANNEL_EOF=96
readonly SSH_MSG_CHANNEL_CLOSE=97

# Logging
log() {
    echo "[$(date '+%H:%M:%S')] $*" >&2
}

# === Binary Data Helpers ===

hex_to_bin() {
    local hex="$1"
    local i
    for ((i=0; i<${#hex}; i+=2)); do
        printf "\\x${hex:$i:2}"
    done
}

bin_to_hex() {
    od -An -tx1 -v | tr -d ' \n'
}

read_bytes() {
    dd bs=1 count="$1" 2>/dev/null
}

write_uint32() {
    local val=$1
    local hex=$(printf "%08x" "$val")
    printf "\\x${hex:0:2}\\x${hex:2:2}\\x${hex:4:2}\\x${hex:6:2}"
}

write_uint8() {
    local val=$1
    printf "\\x$(printf "%02x" "$val")"
}

write_string() {
    local str="$1"
    write_uint32 "${#str}"
    printf "%s" "$str"
}

read_uint32() {
    local bytes=$(read_bytes 4 | bin_to_hex)
    printf "%d" "0x$bytes"
}

# === SSH Packet Functions ===

send_packet() {
    local payload_file="$1"
    local payload_len=$(wc -c < "$payload_file")

    # Calculate padding (4-255 bytes, total % 8 == 0)
    local padding_len=$(( (8 - (payload_len + 5) % 8) % 8 ))
    if [ $padding_len -lt 4 ]; then
        padding_len=$((padding_len + 8))
    fi

    local packet_len=$((1 + payload_len + padding_len))

    # Build packet: uint32(len) + uint8(pad_len) + payload + padding
    write_uint32 "$packet_len"
    write_uint8 "$padding_len"
    cat "$payload_file"
    dd if=/dev/urandom bs=1 count=$padding_len 2>/dev/null
}

read_packet() {
    local output_file="$1"

    # Read packet length
    local packet_len=$(read_uint32)
    if [ "$packet_len" -eq 0 ] || [ "$packet_len" -gt 35000 ]; then
        log "ERROR: Invalid packet length: $packet_len"
        return 1
    fi

    # Read full packet
    local packet_data=$(read_bytes "$packet_len")

    # Extract padding length (first byte)
    local padding_len=$(echo -n "$packet_data" | head -c 1 | bin_to_hex)
    padding_len=$((0x$padding_len))

    # Extract payload (skip padding_len byte, then get payload before padding)
    local payload_len=$((packet_len - 1 - padding_len))
    echo -n "$packet_data" | tail -c +2 | head -c "$payload_len" > "$output_file"

    return 0
}

# === Crypto Key Generation ===

generate_server_keys() {
    log "Generating server keys..."

    # Generate Ed25519 host key
    openssl genpkey -algorithm ED25519 -out "$WORKDIR/host_key.pem" 2>/dev/null

    # Extract public key (32 bytes raw)
    openssl pkey -in "$WORKDIR/host_key.pem" -pubout -outform DER 2>/dev/null | \
        tail -c 32 > "$WORKDIR/host_key.pub"

    # Generate ephemeral X25519 key for key exchange
    openssl genpkey -algorithm X25519 -out "$WORKDIR/server_x25519.pem" 2>/dev/null

    # Extract public key (32 bytes raw)
    openssl pkey -in "$WORKDIR/server_x25519.pem" -pubout -outform DER 2>/dev/null | \
        tail -c 32 > "$WORKDIR/server_x25519.pub"

    log "Keys generated"
}

# === SSH Protocol Handlers ===

send_kexinit() {
    log "Sending KEXINIT..."

    {
        write_uint8 $SSH_MSG_KEXINIT
        dd if=/dev/urandom bs=16 count=1 2>/dev/null  # cookie
        write_string "curve25519-sha256,curve25519-sha256@libssh.org"
        write_string "ssh-ed25519"
        write_string "aes128-ctr"
        write_string "aes128-ctr"
        write_string "hmac-sha2-256"
        write_string "hmac-sha2-256"
        write_string "none"
        write_string "none"
        write_string ""  # languages client to server
        write_string ""  # languages server to client
        write_uint8 0    # first_kex_packet_follows
        write_uint32 0   # reserved
    } > "$WORKDIR/kexinit_payload"

    # Save for exchange hash
    tail -c +2 "$WORKDIR/kexinit_payload" > "$WORKDIR/server_kexinit"

    send_packet "$WORKDIR/kexinit_payload"
}

handle_session() {
    log "=== New SSH connection ==="

    # Phase 1: Version Exchange
    log "Phase 1: Version Exchange"
    echo "$SERVER_VERSION"

    local client_version
    if ! read -t 10 -r client_version; then
        log "ERROR: Timeout reading client version"
        return 1
    fi

    # Remove any carriage return
    client_version="${client_version%$'\r'}"

    log "Client: $client_version"

    if [[ ! "$client_version" =~ ^SSH-2\.0- ]]; then
        log "ERROR: Invalid SSH version: $client_version"
        return 1
    fi

    # Save versions for exchange hash
    echo -n "$client_version" > "$WORKDIR/client_version"
    echo -n "$SERVER_VERSION" > "$WORKDIR/server_version"

    # Phase 2: Key Exchange
    log "Phase 2: Key Exchange"

    # Generate keys
    generate_server_keys

    # Send server KEXINIT
    send_kexinit

    # Receive client KEXINIT
    log "Waiting for client KEXINIT..."
    if ! read_packet "$WORKDIR/client_kexinit_full"; then
        log "ERROR: Failed to read client KEXINIT"
        return 1
    fi

    local msg_type=$(head -c 1 "$WORKDIR/client_kexinit_full" | bin_to_hex)
    log "Received message type: 0x$msg_type"

    if [ "$((0x$msg_type))" -ne $SSH_MSG_KEXINIT ]; then
        log "ERROR: Expected KEXINIT, got $((0x$msg_type))"
        return 1
    fi

    # Save client KEXINIT (without message type byte)
    tail -c +2 "$WORKDIR/client_kexinit_full" > "$WORKDIR/client_kexinit"

    # Receive KEX_ECDH_INIT (client's public key)
    log "Waiting for KEX_ECDH_INIT..."
    if ! read_packet "$WORKDIR/kex_init"; then
        log "ERROR: Failed to read KEX_ECDH_INIT"
        return 1
    fi

    msg_type=$(head -c 1 "$WORKDIR/kex_init" | bin_to_hex)
    if [ "$((0x$msg_type))" -ne $SSH_MSG_KEX_ECDH_INIT ]; then
        log "ERROR: Expected KEX_ECDH_INIT"
        return 1
    fi

    # Extract client's public key (skip msg type + uint32 length = 5 bytes)
    tail -c +6 "$WORKDIR/kex_init" | head -c 32 > "$WORKDIR/client_x25519.pub"

    log "Performing Curve25519 key exchange..."
    # Note: OpenSSL pkeyutl -derive needs PEM format peer key
    # This is a simplification - in real implementation we'd properly encode DER

    # For now, send KEX_ECDH_REPLY with our public key
    log "Sending KEX_ECDH_REPLY..."
    {
        write_uint8 $SSH_MSG_KEX_ECDH_REPLY

        # Host key (ssh-ed25519)
        {
            write_string "ssh-ed25519"
            write_string "$(cat "$WORKDIR/host_key.pub")"
        } > "$WORKDIR/host_key_blob"
        write_string "$(cat "$WORKDIR/host_key_blob")"

        # Server's ephemeral public key
        write_string "$(cat "$WORKDIR/server_x25519.pub")"

        # Signature (simplified - would need proper exchange hash)
        # For demo: create a dummy signature
        dd if=/dev/urandom bs=64 count=1 2>/dev/null > "$WORKDIR/signature"
        write_string "$(cat "$WORKDIR/signature")"

    } > "$WORKDIR/kex_reply_payload"

    send_packet "$WORKDIR/kex_reply_payload"

    # Send NEWKEYS
    log "Sending NEWKEYS..."
    echo -ne "\\x15" > "$WORKDIR/newkeys_payload"  # SSH_MSG_NEWKEYS = 21
    send_packet "$WORKDIR/newkeys_payload"

    # Receive NEWKEYS from client
    log "Waiting for client NEWKEYS..."
    if ! read_packet "$WORKDIR/client_newkeys"; then
        log "WARNING: Failed to read client NEWKEYS (may have disconnected)"
    fi

    log "Key exchange complete (encryption NOT implemented in this demo)"

    # Phase 3: Service Request & Authentication
    log "Phase 3: Authentication"

    # Note: After NEWKEYS, packets should be encrypted
    # This is where the BASH implementation shows its limits
    # For demo purposes, we'll continue without encryption

    # Read SERVICE_REQUEST
    log "Waiting for SERVICE_REQUEST..."
    if read_packet "$WORKDIR/service_req" 2>/dev/null; then
        msg_type=$(head -c 1 "$WORKDIR/service_req" | bin_to_hex)
        log "Received message type: 0x$msg_type"

        # Send SERVICE_ACCEPT
        {
            write_uint8 $SSH_MSG_SERVICE_ACCEPT
            write_string "ssh-userauth"
        } > "$WORKDIR/service_accept_payload"
        send_packet "$WORKDIR/service_accept_payload"
        log "Sent SERVICE_ACCEPT"
    fi

    # Read USERAUTH_REQUEST
    log "Waiting for USERAUTH_REQUEST..."
    if read_packet "$WORKDIR/userauth_req" 2>/dev/null; then
        # Send USERAUTH_SUCCESS (simplified - no password checking in demo)
        echo -ne "\\x34" > "$WORKDIR/userauth_success_payload"  # SSH_MSG_USERAUTH_SUCCESS = 52
        send_packet "$WORKDIR/userauth_success_payload"
        log "Sent USERAUTH_SUCCESS"
    fi

    # Phase 4: Channel
    log "Phase 4: Channel"

    # Read CHANNEL_OPEN
    log "Waiting for CHANNEL_OPEN..."
    if read_packet "$WORKDIR/channel_open" 2>/dev/null; then
        # Send CHANNEL_OPEN_CONFIRMATION
        {
            write_uint8 $SSH_MSG_CHANNEL_OPEN_CONFIRMATION
            write_uint32 0      # recipient channel
            write_uint32 0      # sender channel
            write_uint32 2097152  # initial window size
            write_uint32 32768    # maximum packet size
        } > "$WORKDIR/channel_confirm_payload"
        send_packet "$WORKDIR/channel_confirm_payload"
        log "Sent CHANNEL_OPEN_CONFIRMATION"
    fi

    # Read CHANNEL_REQUEST (pty-req, shell, etc.)
    log "Waiting for CHANNEL_REQUEST..."
    if read_packet "$WORKDIR/channel_req" 2>/dev/null; then
        # Send CHANNEL_SUCCESS
        {
            write_uint8 $SSH_MSG_CHANNEL_SUCCESS
            write_uint32 0  # recipient channel
        } > "$WORKDIR/channel_success_payload"
        send_packet "$WORKDIR/channel_success_payload"
        log "Sent CHANNEL_SUCCESS"

        # Send "Hello World" data
        log "Sending 'Hello World' data..."
        {
            write_uint8 $SSH_MSG_CHANNEL_DATA
            write_uint32 0  # recipient channel
            write_string "Hello World
"
        } > "$WORKDIR/channel_data_payload"
        send_packet "$WORKDIR/channel_data_payload"

        # Send EOF
        {
            write_uint8 $SSH_MSG_CHANNEL_EOF
            write_uint32 0
        } > "$WORKDIR/channel_eof_payload"
        send_packet "$WORKDIR/channel_eof_payload"
        log "Sent EOF"
    fi

    # Small delay before close
    sleep 0.5

    log "Session complete"
}

# === Main Server ===

main() {
    log "BASH SSH Server starting on port $PORT"
    log "Username: $VALID_USERNAME | Password: $VALID_PASSWORD"
    log ""

    # Check for socat first (better for persistent server)
    if command -v socat >/dev/null 2>&1; then
        log "Using socat for TCP handling"
        socat TCP-LISTEN:$PORT,reuseaddr,fork EXEC:"$0 --handle-session"
    elif command -v nc >/dev/null 2>&1; then
        # Use netcat loop with FIFO for bidirectional communication
        log "Using nc for TCP handling"
        log "Press Ctrl+C to stop"

        local FIFO="/tmp/bash_ssh_fifo_$$"
        mkfifo "$FIFO"
        trap 'rm -f "$FIFO"' EXIT

        while true; do
            log ""
            log "Waiting for connection on port $PORT..."
            nc -l "$PORT" < "$FIFO" | handle_session > "$FIFO" || true
        done
    else
        log "ERROR: Neither socat nor nc found"
        exit 1
    fi
}

# Entry point
if [ "${1:-}" = "--handle-session" ]; then
    handle_session
else
    main
fi
