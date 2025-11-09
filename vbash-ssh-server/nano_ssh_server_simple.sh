#!/bin/bash
#
# Nano SSH Server - Simplified BASH Implementation
# Demonstrates SSH protocol in BASH
#
# This version focuses on demonstrating the SSH protocol structure
# rather than full functionality due to BASH's binary data limitations.
#

set -euo pipefail

# Configuration
readonly PORT="${1:-2222}"
readonly SERVER_VERSION="SSH-2.0-BashSSH_0.1"
readonly FIFO="/tmp/bash_ssh_fifo_$$"

# Cleanup on exit
cleanup() {
    rm -f "$FIFO"
    log "Server shutdown"
}
trap cleanup EXIT INT TERM

# Logging
log() {
    echo "[$(date '+%H:%M:%S')] $*" >&2
}

# Create FIFO for bidirectional communication
mkfifo "$FIFO"

# Main server loop
server_loop() {
    log "BASH SSH Server listening on port $PORT"
    log "Press Ctrl+C to stop"
    echo ""

    while true; do
        log "Waiting for connection..."

        # Use nc to listen and pipe to our handler
        # OpenBSD nc syntax: nc -l port
        nc -l "$PORT" < "$FIFO" | handle_connection > "$FIFO"

        log "Connection closed"
        echo ""
    done
}

# Handle a single SSH connection
handle_connection() {
    log "New connection from client"

    # Phase 1: Version Exchange
    # Send our version
    echo "$SERVER_VERSION"
    log "Sent: $SERVER_VERSION"

    # Read client version
    local client_version
    read -r client_version
    log "Received: $client_version"

    # Validate client version
    if [[ ! "$client_version" =~ ^SSH-2\.0- ]]; then
        log "ERROR: Invalid SSH version from client"
        return 1
    fi

    # Phase 2: Binary Protocol Begins
    # From here, we need to send binary SSH packets
    # This is where BASH limitations become apparent

    log "Binary protocol phase - sending KEXINIT"

    # Build a minimal KEXINIT packet
    # Structure: uint32(length) + byte(padding) + byte(type=20) + data + padding

    # For demonstration, we'll send a minimal packet
    # In a real implementation, this would be much more complex

    # Send some binary data to show the structure
    # SSH_MSG_KEXINIT = 20 (0x14)
    printf "\x00\x00\x00\x0c"  # packet length = 12
    printf "\x04"               # padding length = 4
    printf "\x14"               # message type = SSH_MSG_KEXINIT (20)

    # In real SSH, here would be:
    # - 16 bytes random cookie
    # - Algorithm name lists
    # - Boolean first_kex_packet_follows
    # - uint32 reserved

    # For now, just send minimal data + padding
    printf "minimal\x00\x00\x00\x00"

    log "Sent KEXINIT packet (simplified)"

    # Read client response (we'll just display it in hex)
    log "Waiting for client KEXINIT..."

    # Try to read 4 bytes (packet length)
    local length_bytes
    length_bytes=$(dd bs=1 count=4 2>/dev/null | od -An -tx1 | tr -d ' ')

    if [ -n "$length_bytes" ]; then
        log "Received packet length bytes: $length_bytes"

        # Calculate length
        local packet_length
        packet_length=$((16#$length_bytes))
        log "Packet length: $packet_length"

        # Read the rest of the packet
        dd bs=1 count="$packet_length" 2>/dev/null > /dev/null

        log "Received client KEXINIT packet"
    fi

    # At this point, a real SSH server would:
    # 1. Parse KEXINIT and negotiate algorithms
    # 2. Perform key exchange (curve25519)
    # 3. Send NEWKEYS
    # 4. Enable encryption
    # 5. Handle service requests
    # 6. Authenticate user
    # 7. Open channel
    # 8. Send data

    log "Protocol demonstration complete"
    log "Note: Full SSH implementation in BASH is limited by:"
    log "  - Binary data handling"
    log "  - Crypto operations (need external tools)"
    log "  - Streaming encryption/decryption"

    # Send a friendly disconnect
    echo "Thank you for connecting to BASH SSH Server demo!"
}

# Start server
server_loop
