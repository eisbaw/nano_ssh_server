#!/bin/bash
#
# Nano SSH Server - Pure BASH Implementation
# World's first (?) SSH server written entirely in BASH!
#
# Dependencies: bash, xxd, openssl, bc, nc/socat
# Usage: ./nano_ssh.sh [port]
#

set -uo pipefail

# Configuration
PORT="${1:-2222}"
SERVER_VERSION="SSH-2.0-BashSSH_0.1"
VALID_USERNAME="user"
VALID_PASSWORD="password123"

# SSH Protocol Constants
SSH_MSG_DISCONNECT=1
SSH_MSG_KEXINIT=20
SSH_MSG_NEWKEYS=21
SSH_MSG_KEX_ECDH_INIT=30
SSH_MSG_KEX_ECDH_REPLY=31
SSH_MSG_SERVICE_REQUEST=5
SSH_MSG_SERVICE_ACCEPT=6
SSH_MSG_USERAUTH_REQUEST=50
SSH_MSG_USERAUTH_FAILURE=51
SSH_MSG_USERAUTH_SUCCESS=52
SSH_MSG_CHANNEL_OPEN=90
SSH_MSG_CHANNEL_OPEN_CONFIRMATION=91
SSH_MSG_CHANNEL_REQUEST=98
SSH_MSG_CHANNEL_SUCCESS=99
SSH_MSG_CHANNEL_DATA=94
SSH_MSG_CHANNEL_EOF=96
SSH_MSG_CHANNEL_CLOSE=97

# Global state
CLIENT_VERSION=""
CLIENT_KEXINIT=""
SERVER_KEXINIT=""
SHARED_SECRET=""
EXCHANGE_HASH=""
SESSION_ID=""

# Encryption state (set after NEWKEYS)
# RFC 4253: Encryption is asymmetric - each direction activates independently
ENCRYPTION_S2C_ENABLED=0  # Server-to-client (outgoing) - enabled after WE send NEWKEYS
ENCRYPTION_C2S_ENABLED=0  # Client-to-server (incoming) - enabled after WE receive client's NEWKEYS
ENCRYPTION_KEY_C2S=""
ENCRYPTION_KEY_S2C=""
IV_C2S=""
IV_S2C=""
MAC_KEY_C2S=""
MAC_KEY_S2C=""
SEQUENCE_NUM_C2S=0
SEQUENCE_NUM_S2C=0

# Packet counters (track ALL packets for sequence number initialization)
PACKETS_SENT=0
PACKETS_RECEIVED=0

# Temporary directory for keys and state
TMPDIR=$(mktemp -d /tmp/bash-ssh.XXXXXX)
trap "rm -rf $TMPDIR" EXIT

# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" >&2
}

error() {
    log "ERROR: $*"
    exit 1
}

# Convert hex string to binary
hex2bin() {
    echo -n "$1" | xxd -r -p
}

# Convert binary to hex string
bin2hex() {
    xxd -p | tr -d '\n'
}

# Read N bytes from stdin and output as hex
read_bytes() {
    local count=$1
    dd bs=1 count=$count 2>/dev/null | bin2hex
}

# Read uint32 (4 bytes, big-endian)
read_uint32() {
    local hex=$(read_bytes 4)
    if [ -z "$hex" ]; then
        echo "0"
        return 1
    fi
    echo $((16#$hex))
}

# Write uint32 (big-endian) as binary
write_uint32() {
    local val=$1
    printf "%08x" $val | xxd -r -p
}

# Read SSH string (uint32 length + data)
read_ssh_string() {
    local len=$(read_uint32)
    read_bytes $len
}

# Write SSH string (uint32 length + data)
write_ssh_string() {
    local data="$1"
    local len=$((${#data} / 2))  # hex string length / 2
    write_uint32 $len
    hex2bin "$data"
}

# Read an SSH packet (RFC 4253 section 6)
# Returns: message type and payload as hex
read_ssh_packet() {
    local packet_len=$(read_uint32)
    if [ $packet_len -eq 0 ]; then
        log "Connection closed (packet_len=0)"
        return 1
    fi

    if [ $ENCRYPTION_C2S_ENABLED -eq 1 ]; then
        log "Reading ENCRYPTED packet: packet_len=$packet_len, seq=$SEQUENCE_NUM_C2S"

        # Read encrypted data
        local encrypted=$(read_bytes $packet_len)
        if [ -z "$encrypted" ]; then
            log "Connection closed (no encrypted data)"
            return 1
        fi

        # Read MAC (32 bytes)
        local mac_received=$(read_bytes 32)
        if [ -z "$mac_received" ]; then
            log "Connection closed (no MAC)"
            return 1
        fi

        # Verify MAC
        local mac_input=$(printf "%08x" $SEQUENCE_NUM_C2S)$(printf "%08x" $packet_len)${encrypted}
        local mac_expected=$(compute_hmac "$mac_input" "$MAC_KEY_C2S")

        if [ "${mac_received}" != "${mac_expected:0:64}" ]; then
            log "MAC verification failed!"
            log "  Expected: ${mac_expected:0:64}"
            log "  Received: ${mac_received}"
            return 1
        fi

        # Decrypt data
        local decrypted=$(decrypt_aes_ctr "$encrypted" "$ENCRYPTION_KEY_C2S" "$IV_C2S")

        # Parse decrypted packet
        local padding_len=$((16#${decrypted:0:2}))
        local payload_len=$((packet_len - padding_len - 1))
        local payload="${decrypted:2:$((payload_len * 2))}"

        # Extract message type
        if [ ${#payload} -lt 2 ]; then
            log "Invalid payload (too short)"
            return 1
        fi
        local msg_type=$((16#${payload:0:2}))
        local msg_data="${payload:2}"

        log "Received ENCRYPTED packet: type=$msg_type, payload_len=$payload_len"

        # Increment sequence number
        SEQUENCE_NUM_C2S=$((SEQUENCE_NUM_C2S + 1))

        # Increment packet counter
        PACKETS_RECEIVED=$((PACKETS_RECEIVED + 1))

        echo "$msg_type:$msg_data"
    else
        # Plaintext mode (original code)
        local padding_len_hex=$(read_bytes 1)
        if [ -z "$padding_len_hex" ]; then
            log "Connection closed (no padding_len)"
            return 1
        fi
        local padding_len=$((16#$padding_len_hex))
        local payload_len=$((packet_len - padding_len - 1))

        local payload=$(read_bytes $payload_len)
        local padding=$(read_bytes $padding_len)

        # Extract message type (first byte of payload)
        if [ ${#payload} -lt 2 ]; then
            log "Invalid payload (too short)"
            return 1
        fi
        local msg_type=$((16#${payload:0:2}))
        local msg_data="${payload:2}"

        log "Received plaintext packet: type=$msg_type, payload_len=$payload_len, packet_len=$packet_len"
        log "  Payload hex (first 40 chars): ${payload:0:40}..."

        # Increment packet counter
        PACKETS_RECEIVED=$((PACKETS_RECEIVED + 1))

        echo "$msg_type:$msg_data"
    fi
}

# Write an SSH packet
write_ssh_packet() {
    local msg_type=$1
    local payload_hex="$2"

    # Message type + payload
    local full_payload=$(printf "%02x" $msg_type)${payload_hex}
    local payload_len=$((${#full_payload} / 2))

    # Calculate padding (minimum 4 bytes, block size 16 for AES)
    local block_size=16
    if [ $ENCRYPTION_S2C_ENABLED -eq 0 ]; then
        block_size=8  # Unencrypted uses block size 8
    fi

    local padding_len=$(( (block_size - ((payload_len + 5) % block_size)) % block_size ))
    if [ $padding_len -lt 4 ]; then
        padding_len=$((padding_len + block_size))
    fi

    # Generate random padding
    local padding=$(openssl rand -hex $padding_len)

    # packet_length = padding_length + payload + padding
    local packet_len=$((1 + payload_len + padding_len))

    if [ $ENCRYPTION_S2C_ENABLED -eq 1 ]; then
        log "Sending ENCRYPTED packet: type=$msg_type, payload_len=$payload_len, seq=$SEQUENCE_NUM_S2C"

        # Build data to encrypt: padding_length + payload + padding
        local data_to_encrypt=$(printf "%02x" $padding_len)${full_payload}${padding}

        # Encrypt the data
        local encrypted=$(encrypt_aes_ctr "$data_to_encrypt" "$ENCRYPTION_KEY_S2C" "$IV_S2C")

        # Compute MAC over sequence_number + packet_length + encrypted_data
        local mac_input=$(printf "%08x" $SEQUENCE_NUM_S2C)$(printf "%08x" $packet_len)${encrypted}
        local mac=$(compute_hmac "$mac_input" "$MAC_KEY_S2C")

        # Send: packet_length (plaintext) + encrypted_data + MAC (first 32 bytes)
        write_uint32 $packet_len
        hex2bin "$encrypted"
        hex2bin "${mac:0:64}"  # 32 bytes = 64 hex chars

        # Increment sequence number
        SEQUENCE_NUM_S2C=$((SEQUENCE_NUM_S2C + 1))

        # Increment packet counter
        PACKETS_SENT=$((PACKETS_SENT + 1))
    else
        log "Sending plaintext packet: type=$msg_type, payload_len=$payload_len"

        # Build complete packet as single hex string for atomic write
        local complete_packet=""
        complete_packet="${complete_packet}$(printf "%08x" $packet_len)"
        complete_packet="${complete_packet}$(printf "%02x" $padding_len)"
        complete_packet="${complete_packet}${full_payload}"
        complete_packet="${complete_packet}${padding}"

        log "  Complete packet hex (first 60 chars): ${complete_packet:0:60}..."
        log "  Packet structure: len=$packet_len, padding=$padding_len, total_hex_len=${#complete_packet}"

        # Write entire packet as single atomic operation to avoid buffering issues
        hex2bin "$complete_packet"

        log "  Packet sent successfully"

        # Increment packet counter
        PACKETS_SENT=$((PACKETS_SENT + 1))
    fi
}

# ============================================================================
# CRYPTOGRAPHY FUNCTIONS
# ============================================================================

# Generate host key (Ed25519)
generate_host_key() {
    log "Generating Ed25519 host key..."
    openssl genpkey -algorithm ED25519 -out "$TMPDIR/host_key.pem" 2>/dev/null
    openssl pkey -in "$TMPDIR/host_key.pem" -pubout -out "$TMPDIR/host_key_pub.pem" 2>/dev/null

    # Extract raw public key (32 bytes)
    openssl pkey -in "$TMPDIR/host_key_pub.pem" -pubin -outform DER 2>/dev/null | \
        tail -c 32 | bin2hex > "$TMPDIR/host_pubkey.hex"
}

# Generate ephemeral key for Curve25519 key exchange
generate_ephemeral_key() {
    log "Generating ephemeral Curve25519 key..."
    openssl genpkey -algorithm X25519 -out "$TMPDIR/ephemeral.pem" 2>/dev/null
    openssl pkey -in "$TMPDIR/ephemeral.pem" -pubout -out "$TMPDIR/ephemeral_pub.pem" 2>/dev/null

    # Extract raw public key (32 bytes)
    openssl pkey -in "$TMPDIR/ephemeral_pub.pem" -pubin -outform DER 2>/dev/null | \
        tail -c 32 | bin2hex > "$TMPDIR/ephemeral_pubkey.hex"
}

# Compute Curve25519 shared secret
compute_shared_secret() {
    local client_pubkey_hex="$1"

    log "Computing shared secret..."

    # Save client's public key in DER format
    # X25519 public key DER format: 30 2a 30 05 06 03 2b 65 6e 03 21 00 + 32 bytes
    local der_header="302a300506032b656e032100"
    echo "${der_header}${client_pubkey_hex}" | xxd -r -p > "$TMPDIR/client_ephemeral.der"

    # Compute shared secret using OpenSSL
    openssl pkeyutl -derive -inkey "$TMPDIR/ephemeral.pem" \
        -peerkey "$TMPDIR/client_ephemeral.der" -keyform DER 2>/dev/null | bin2hex
}

# Sign data with Ed25519 host key
sign_data() {
    local data_hex="$1"
    hex2bin "$data_hex" | openssl pkeyutl -sign -inkey "$TMPDIR/host_key.pem" 2>/dev/null | bin2hex
}

# Compute SHA256 hash
sha256() {
    local data_hex="$1"
    hex2bin "$data_hex" | openssl dgst -sha256 -binary | bin2hex
}

# ============================================================================
# ENCRYPTION FUNCTIONS
# ============================================================================

# Derive encryption keys from shared secret (RFC 4253 Section 7.2)
derive_keys() {
    log "Deriving encryption keys..."

    # Helper to derive one key: HASH(K || H || letter || session_id)
    derive_key() {
        local letter="$1"
        local letter_hex=$(printf "%s" "$letter" | xxd -p)
        local data="${SHARED_SECRET}${EXCHANGE_HASH}${letter_hex}${SESSION_ID}"
        sha256 "$data"
    }

    # Derive all 6 keys
    IV_C2S=$(derive_key "A")
    IV_S2C=$(derive_key "B")
    ENCRYPTION_KEY_C2S=$(derive_key "C")
    ENCRYPTION_KEY_S2C=$(derive_key "D")
    MAC_KEY_C2S=$(derive_key "E")
    MAC_KEY_S2C=$(derive_key "F")

    # For AES-128, we only need 16 bytes (32 hex chars)
    ENCRYPTION_KEY_C2S="${ENCRYPTION_KEY_C2S:0:32}"
    ENCRYPTION_KEY_S2C="${ENCRYPTION_KEY_S2C:0:32}"

    log "Keys derived successfully"
    log "  ENC_KEY_S2C: ${ENCRYPTION_KEY_S2C}"
    log "  IV_S2C: ${IV_S2C:0:32}..."
    log "  MAC_KEY_S2C: ${MAC_KEY_S2C:0:32}..."
}

# Compute HMAC-SHA2-256
compute_hmac() {
    local data_hex="$1"
    local key_hex="$2"

    hex2bin "$data_hex" | \
        openssl dgst -sha256 -mac HMAC -macopt hexkey:$key_hex -binary | bin2hex
}

# Encrypt data with AES-128-CTR (simplified - no IV increment for now)
encrypt_aes_ctr() {
    local plaintext_hex="$1"
    local key_hex="$2"
    local iv_hex="$3"

    hex2bin "$plaintext_hex" | \
        openssl enc -aes-128-ctr -K "$key_hex" -iv "$iv_hex" -nopad 2>/dev/null | bin2hex
}

# Decrypt data with AES-128-CTR
decrypt_aes_ctr() {
    local ciphertext_hex="$1"
    local key_hex="$2"
    local iv_hex="$3"

    # AES-CTR encryption and decryption are the same operation
    encrypt_aes_ctr "$ciphertext_hex" "$key_hex" "$iv_hex"
}

# ============================================================================
# SSH PROTOCOL HANDLERS
# ============================================================================

# Handle version exchange
handle_version_exchange() {
    log "Sending server version: $SERVER_VERSION"
    echo -en "${SERVER_VERSION}\r\n"

    log "Waiting for client version..."
    read -r CLIENT_VERSION
    CLIENT_VERSION="${CLIENT_VERSION%$'\r'}"  # Remove \r

    log "Client version: $CLIENT_VERSION"

    if [[ ! "$CLIENT_VERSION" =~ ^SSH-2\.0- ]]; then
        error "Invalid client version: $CLIENT_VERSION"
    fi
}

# Build KEXINIT payload
build_kexinit() {
    local payload=""

    # Random cookie (16 bytes)
    local cookie=$(openssl rand -hex 16)
    payload="${payload}${cookie}"

    # Algorithm name-lists (all as SSH strings)
    local kex_alg="curve25519-sha256"
    local host_key_alg="ssh-ed25519"
    local cipher="aes128-ctr"
    local mac="hmac-sha2-256"
    local compress="none"
    local lang=""

    # Helper to add name-list
    add_namelist() {
        local names="$1"
        local hex=$(echo -n "$names" | bin2hex)
        local len=${#names}
        payload="${payload}$(printf "%08x" $len)${hex}"
    }

    add_namelist "$kex_alg"          # kex_algorithms
    add_namelist "$host_key_alg"     # server_host_key_algorithms
    add_namelist "$cipher"           # encryption_algorithms_client_to_server
    add_namelist "$cipher"           # encryption_algorithms_server_to_client
    add_namelist "$mac"              # mac_algorithms_client_to_server
    add_namelist "$mac"              # mac_algorithms_server_to_client
    add_namelist "$compress"         # compression_algorithms_client_to_server
    add_namelist "$compress"         # compression_algorithms_server_to_client
    add_namelist "$lang"             # languages_client_to_server
    add_namelist "$lang"             # languages_server_to_client

    # First packet follows (boolean): false
    payload="${payload}00"

    # Reserved (uint32): 0
    payload="${payload}00000000"

    echo "$payload"
}

# Handle key exchange
handle_kexinit() {
    local client_packet="$1"

    log "Handling KEXINIT..."

    # Save client's KEXINIT payload for exchange hash
    CLIENT_KEXINIT="$client_packet"

    # Build and send server's KEXINIT
    SERVER_KEXINIT=$(build_kexinit)
    write_ssh_packet $SSH_MSG_KEXINIT "$SERVER_KEXINIT"
}

# Handle ECDH Init
handle_ecdh_init() {
    local payload_hex="$1"

    log "Handling KEX_ECDH_INIT..."

    # Read client's ephemeral public key (Q_C)
    # Format: SSH string (uint32 len + 32 bytes)
    local client_pubkey_len=$((16#${payload_hex:0:8}))
    local client_pubkey="${payload_hex:8:64}"  # 32 bytes = 64 hex chars

    log "Client ephemeral key: $client_pubkey"

    # Compute shared secret K
    SHARED_SECRET=$(compute_shared_secret "$client_pubkey")
    log "Shared secret: ${SHARED_SECRET:0:16}..."

    # Build exchange hash H
    # H = SHA256(V_C || V_S || I_C || I_S || K_S || Q_C || Q_S || K)

    local V_C=$(echo -n "$CLIENT_VERSION" | bin2hex)
    local V_S=$(echo -n "$SERVER_VERSION" | bin2hex)
    local I_C="$CLIENT_KEXINIT"
    local I_S="$SERVER_KEXINIT"

    # K_S: server host public key as SSH string
    local host_pubkey=$(cat "$TMPDIR/host_pubkey.hex")
    local K_S_inner=$(printf "%08x" 11; echo -n "ssh-ed25519" | bin2hex; printf "%08x" 32; echo "$host_pubkey")

    # Q_C: client ephemeral public key
    local Q_C="$client_pubkey"

    # Q_S: server ephemeral public key
    local Q_S=$(cat "$TMPDIR/ephemeral_pubkey.hex")

    # K: shared secret as mpint
    local K_mpint=$(printf "%08x" 32)${SHARED_SECRET}

    # Build hash input (as SSH strings where appropriate)
    local hash_input=""
    hash_input="${hash_input}$(printf "%08x" $((${#V_C} / 2)))${V_C}"
    hash_input="${hash_input}$(printf "%08x" $((${#V_S} / 2)))${V_S}"
    hash_input="${hash_input}$(printf "%08x" $((${#I_C} / 2)))${I_C}"
    hash_input="${hash_input}$(printf "%08x" $((${#I_S} / 2)))${I_S}"
    hash_input="${hash_input}$(printf "%08x" $((${#K_S_inner} / 2)))${K_S_inner}"
    hash_input="${hash_input}$(printf "%08x" 32)${Q_C}"
    hash_input="${hash_input}$(printf "%08x" 32)${Q_S}"
    hash_input="${hash_input}${K_mpint}"

    EXCHANGE_HASH=$(sha256 "$hash_input")
    SESSION_ID="$EXCHANGE_HASH"  # First exchange hash is session ID

    log "Exchange hash: ${EXCHANGE_HASH:0:16}..."

    # Derive encryption keys NOW (before sending NEWKEYS)
    derive_keys

    # Sign the exchange hash
    local signature=$(sign_data "$EXCHANGE_HASH")
    log "Signature: ${signature:0:16}..."

    # Build SSH_MSG_KEX_ECDH_REPLY
    local reply=""

    # K_S (server host public key)
    reply="${reply}$(printf "%08x" $((${#K_S_inner} / 2)))${K_S_inner}"

    # Q_S (server ephemeral public key)
    reply="${reply}$(printf "%08x" 32)${Q_S}"

    # Signature of H
    local sig_blob=$(printf "%08x" 11; echo -n "ssh-ed25519" | bin2hex; printf "%08x" 64; echo "$signature")
    reply="${reply}$(printf "%08x" $((${#sig_blob} / 2)))${sig_blob}"

    write_ssh_packet $SSH_MSG_KEX_ECDH_REPLY "$reply"
    log "DEBUG: About to send NEWKEYS"

    # Send NEWKEYS (still plaintext)
    write_ssh_packet $SSH_MSG_NEWKEYS ""

    # RFC 4253 Section 7.3: "All messages sent after this message MUST use the new keys"
    # Enable S2C encryption immediately after sending NEWKEYS
    # Set sequence number to the count of packets we've already sent (KEXINIT, KEX_ECDH_REPLY, NEWKEYS)
    SEQUENCE_NUM_S2C=$PACKETS_SENT
    ENCRYPTION_S2C_ENABLED=1
    log "NEWKEYS sent - S2C encryption ENABLED for all outgoing packets (seq starts at $SEQUENCE_NUM_S2C)"
    log "Waiting for client NEWKEYS in main loop..."
    # Note: We DON'T read client's NEWKEYS here - let the main loop handle it
    # This avoids buffering issues with BASH I/O
}

# Handle service request
handle_service_request() {
    local payload_hex="$1"

    log "Handling SERVICE_REQUEST..."
    log "  Payload hex: ${payload_hex:0:60}..."

    # Read service name
    local service_len=$((16#${payload_hex:0:8}))
    local service_hex="${payload_hex:8:$((service_len * 2))}"
    local service=$(hex2bin "$service_hex")

    log "Service requested: $service (len=$service_len)"

    # Send SERVICE_ACCEPT
    local accept_payload=$(printf "%08x" ${#service})$(echo -n "$service" | bin2hex)
    log "  Accept payload hex: $accept_payload"
    write_ssh_packet $SSH_MSG_SERVICE_ACCEPT "$accept_payload"
}

# Handle userauth request
handle_userauth_request() {
    local payload_hex="$1"

    log "Handling USERAUTH_REQUEST..."

    # Parse: username, service, method, [method-specific fields]
    local offset=0

    # Read username
    local user_len=$((16#${payload_hex:$offset:8}))
    offset=$((offset + 8))
    local username=$(hex2bin "${payload_hex:$offset:$((user_len * 2))}")
    offset=$((offset + user_len * 2))

    # Read service
    local svc_len=$((16#${payload_hex:$offset:8}))
    offset=$((offset + 8))
    local service=$(hex2bin "${payload_hex:$offset:$((svc_len * 2))}")
    offset=$((offset + svc_len * 2))

    # Read method
    local method_len=$((16#${payload_hex:$offset:8}))
    offset=$((offset + 8))
    local method=$(hex2bin "${payload_hex:$offset:$((method_len * 2))}")
    offset=$((offset + method_len * 2))

    log "Auth: user=$username, service=$service, method=$method"

    if [ "$method" = "password" ]; then
        # Skip change password flag
        offset=$((offset + 2))

        # Read password
        local pass_len=$((16#${payload_hex:$offset:8}))
        offset=$((offset + 8))
        local password=$(hex2bin "${payload_hex:$offset:$((pass_len * 2))}")

        log "Password authentication: $username"

        if [ "$username" = "$VALID_USERNAME" ] && [ "$password" = "$VALID_PASSWORD" ]; then
            log "Authentication successful!"
            write_ssh_packet $SSH_MSG_USERAUTH_SUCCESS ""
        else
            log "Authentication failed!"
            local failure_payload=$(printf "%08x" 8)$(echo -n "password" | bin2hex)"00"
            write_ssh_packet $SSH_MSG_USERAUTH_FAILURE "$failure_payload"
        fi
    else
        log "Unsupported auth method: $method"
        local failure_payload=$(printf "%08x" 8)$(echo -n "password" | bin2hex)"00"
        write_ssh_packet $SSH_MSG_USERAUTH_FAILURE "$failure_payload"
    fi
}

# Handle channel open
handle_channel_open() {
    local payload_hex="$1"

    log "Handling CHANNEL_OPEN..."

    # Parse channel type, sender channel, initial window, max packet
    local offset=0

    local type_len=$((16#${payload_hex:$offset:8}))
    offset=$((offset + 8))
    local channel_type=$(hex2bin "${payload_hex:$offset:$((type_len * 2))}")
    offset=$((offset + type_len * 2))

    local sender_channel=$((16#${payload_hex:$offset:8}))
    offset=$((offset + 8))
    local initial_window=$((16#${payload_hex:$offset:8}))
    offset=$((offset + 8))
    local max_packet=$((16#${payload_hex:$offset:8}))

    log "Channel: type=$channel_type, sender=$sender_channel"

    # Send CHANNEL_OPEN_CONFIRMATION
    local confirm=""
    confirm="${confirm}$(printf "%08x" $sender_channel)"      # recipient channel
    confirm="${confirm}$(printf "%08x" 0)"                    # sender channel
    confirm="${confirm}$(printf "%08x" 32768)"                # initial window
    confirm="${confirm}$(printf "%08x" 32768)"                # max packet

    write_ssh_packet $SSH_MSG_CHANNEL_OPEN_CONFIRMATION "$confirm"
}

# Handle channel request
handle_channel_request() {
    local payload_hex="$1"

    log "Handling CHANNEL_REQUEST..."

    # Parse recipient channel, request type, want reply
    local offset=0

    local recipient_channel=$((16#${payload_hex:$offset:8}))
    offset=$((offset + 8))

    local req_len=$((16#${payload_hex:$offset:8}))
    offset=$((offset + 8))
    local request_type=$(hex2bin "${payload_hex:$offset:$((req_len * 2))}")
    offset=$((offset + req_len * 2))

    local want_reply=$((16#${payload_hex:$offset:2}))

    log "Channel request: type=$request_type, want_reply=$want_reply, channel=$recipient_channel"

    if [ $want_reply -eq 1 ]; then
        # Send success for any request
        write_ssh_packet $SSH_MSG_CHANNEL_SUCCESS "$(printf "%08x" $recipient_channel)"
    fi

    # Send "Hello World" message
    log "Sending 'Hello World' to client..."

    local message="Hello World"$'\r\n'
    local msg_hex=$(echo -n "$message" | bin2hex)
    local data_payload="$(printf "%08x" $recipient_channel)$(printf "%08x" ${#message})${msg_hex}"

    write_ssh_packet $SSH_MSG_CHANNEL_DATA "$data_payload"

    # Send EOF
    write_ssh_packet $SSH_MSG_CHANNEL_EOF "$(printf "%08x" $recipient_channel)"

    # Send CLOSE
    write_ssh_packet $SSH_MSG_CHANNEL_CLOSE "$(printf "%08x" $recipient_channel)"
}

# ============================================================================
# MAIN PROTOCOL HANDLER
# ============================================================================

handle_connection() {
    log "New connection!"

    # Generate keys
    generate_host_key
    generate_ephemeral_key

    # Version exchange
    handle_version_exchange

    # Main protocol loop
    local authenticated=0
    local channel_open=0

    while true; do
        local packet=$(read_ssh_packet) || {
            log "Failed to read packet, connection closed"
            break
        }
        local msg_type="${packet%%:*}"
        local msg_data="${packet#*:}"

        case $msg_type in
            $SSH_MSG_KEXINIT)
                handle_kexinit "$msg_data"
                ;;
            $SSH_MSG_KEX_ECDH_INIT)
                handle_ecdh_init "$msg_data"
                # After handle_ecdh_init, S2C encryption is enabled, waiting for client NEWKEYS
                ;;
            $SSH_MSG_NEWKEYS)
                # Enable C2S encryption after receiving client's NEWKEYS
                # Set sequence number to the count of packets we've received so far
                SEQUENCE_NUM_C2S=$PACKETS_RECEIVED
                ENCRYPTION_C2S_ENABLED=1
                log "Client NEWKEYS received - C2S encryption ENABLED (seq starts at $SEQUENCE_NUM_C2S)"
                log "Full duplex encryption active (S2C seq=$SEQUENCE_NUM_S2C, C2S seq=$SEQUENCE_NUM_C2S)"
                ;;
            $SSH_MSG_SERVICE_REQUEST)
                handle_service_request "$msg_data"
                ;;
            $SSH_MSG_USERAUTH_REQUEST)
                handle_userauth_request "$msg_data"
                ;;
            $SSH_MSG_CHANNEL_OPEN)
                handle_channel_open "$msg_data"
                ;;
            $SSH_MSG_CHANNEL_REQUEST)
                handle_channel_request "$msg_data"
                # Exit after sending response
                log "Connection complete!"
                break
                ;;
            $SSH_MSG_CHANNEL_CLOSE)
                log "Client closed channel"
                break
                ;;
            *)
                log "Unknown message type: $msg_type"
                ;;
        esac
    done

    log "Connection closed"
}

# ============================================================================
# SERVER MAIN
# ============================================================================

main() {
    log "Starting Nano SSH Server (BASH implementation) on port $PORT"
    log "Temporary directory: $TMPDIR"

    # Use socat for TCP server
    if command -v socat &> /dev/null; then
        log "Using socat for server socket"
        socat TCP-LISTEN:$PORT,reuseaddr,fork,max-children=10 EXEC:"bash $0 --handle-connection" &
        SERVER_PID=$!
        log "Server PID: $SERVER_PID"
        log "Server ready! Connect with: ssh -p $PORT user@localhost"
        log "Password: $VALID_PASSWORD"
        wait $SERVER_PID
    elif command -v nc &> /dev/null; then
        log "Using netcat for server socket"
        while true; do
            nc -l -p $PORT -c "bash '$0' --handle-connection"
        done
    else
        error "Neither socat nor nc found. Please install one of them."
    fi
}

# Check if we're handling a connection (called by socat/nc)
if [ "${1:-}" = "--handle-connection" ]; then
    handle_connection
else
    main
fi
