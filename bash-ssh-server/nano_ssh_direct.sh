#!/bin/bash
#
# Nano SSH Server - Direct Socket Version
# Eliminates socat EXEC wrapper to provide more direct socket I/O
#
# Usage with systemd-socket-activate (if available):
#   systemd-socket-activate -l 2222 --inetd ./nano_ssh_direct.sh
#
# Usage with socat (simpler mode):
#   socat TCP-LISTEN:2222,reuseaddr,fork SYSTEM:"./nano_ssh_direct.sh",nofork
#
# Usage with nc (if -e flag available):
#   while true; do nc -l -p 2222 -e ./nano_ssh_direct.sh; done
#

set -uo pipefail

# Configuration
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

# Encryption state
ENCRYPTION_S2C_ENABLED=0
ENCRYPTION_C2S_ENABLED=0
ENCRYPTION_KEY_C2S=""
ENCRYPTION_KEY_S2C=""
IV_C2S=""
IV_S2C=""
MAC_KEY_C2S=""
MAC_KEY_S2C=""
SEQUENCE_NUM_C2S=0
SEQUENCE_NUM_S2C=0

# Packet counters
PACKETS_SENT=0
PACKETS_RECEIVED=0

# Temporary directory for keys
TMPDIR=$(mktemp -d /tmp/bash-ssh.XXXXXX)
trap "rm -rf $TMPDIR" EXIT

# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

log() {
    echo "[$(date '+%H:%M:%S')] $*" >&2
}

error() {
    log "ERROR: $*"
    exit 1
}

hex2bin() {
    echo -n "$1" | xxd -r -p
}

bin2hex() {
    xxd -p | tr -d '\n'
}

# Read N bytes from stdin and output as hex
# Use dd with direct I/O for more reliable reading
read_bytes() {
    local count=$1
    log "DEBUG read_bytes: Reading $count bytes"

    # Read directly, more efficient than previous version
    local result=$(dd bs=$count count=1 iflag=fullblock 2>/dev/null | bin2hex)
    local bytes_read=$((${#result} / 2))

    if [ $bytes_read -lt $count ]; then
        log "DEBUG read_bytes: Only got $bytes_read bytes (expected $count)"
        if [ $bytes_read -eq 0 ]; then
            return 1
        fi
    fi

    echo "$result"
}

# Read 32-bit unsigned integer (big-endian)
read_uint32() {
    local hex=$(read_bytes 4) || return 1
    echo $((16#$hex))
}

# Write 32-bit unsigned integer (big-endian)
write_uint32() {
    printf "%08x" $1 | xxd -r -p
}

# SHA-256 hash function
sha256() {
    local data_hex="$1"
    hex2bin "$data_hex" | openssl dgst -sha256 -binary | bin2hex
}

# ============================================================================
# SSH PACKET I/O
# ============================================================================

# Read SSH packet
read_ssh_packet() {
    log "Reading packet (C2S encryption=$ENCRYPTION_C2S_ENABLED)..."

    local packet_len=$(read_uint32) || return 1

    if [ $packet_len -eq 0 ] || [ $packet_len -gt 35000 ]; then
        log "Invalid packet_len=$packet_len"
        return 1
    fi

    if [ $ENCRYPTION_C2S_ENABLED -eq 1 ]; then
        log "Reading encrypted packet: len=$packet_len, seq=$SEQUENCE_NUM_C2S"

        local encrypted=$(read_bytes $packet_len) || return 1
        local mac_received=$(read_bytes 32) || return 1

        # Verify MAC
        local mac_input=$(printf "%08x" $SEQUENCE_NUM_C2S)$(printf "%08x" $packet_len)${encrypted}
        local mac_expected=$(compute_hmac "$mac_input" "$MAC_KEY_C2S")

        if [ "${mac_received}" != "${mac_expected:0:64}" ]; then
            log "MAC verification failed!"
            return 1
        fi

        # Decrypt
        local decrypted=$(decrypt_aes_ctr "$encrypted" "$ENCRYPTION_KEY_C2S" "$IV_C2S")

        # Parse
        local padding_len=$((16#${decrypted:0:2}))
        local payload_len=$((packet_len - padding_len - 1))
        local payload="${decrypted:2:$((payload_len * 2))}"

        local msg_type=$((16#${payload:0:2}))
        local msg_payload="${payload:2}"

        SEQUENCE_NUM_C2S=$((SEQUENCE_NUM_C2S + 1))
        PACKETS_RECEIVED=$((PACKETS_RECEIVED + 1))

        echo "$msg_type $msg_payload"
    else
        log "Reading plaintext packet: len=$packet_len"

        local padding_len_hex=$(read_bytes 1) || return 1
        local padding_len=$((16#$padding_len_hex))

        local payload_len=$((packet_len - padding_len - 1))
        local payload=$(read_bytes $payload_len) || return 1
        local padding=$(read_bytes $padding_len) || return 1

        local msg_type=$((16#${payload:0:2}))
        local msg_payload="${payload:2}"

        PACKETS_RECEIVED=$((PACKETS_RECEIVED + 1))

        echo "$msg_type $msg_payload"
    fi
}

# Write SSH packet with atomic output
write_ssh_packet() {
    local msg_type=$1
    local payload_hex="$2"

    local full_payload=$(printf "%02x" $msg_type)${payload_hex}
    local payload_len=$((${#full_payload} / 2))

    local block_size=16
    if [ $ENCRYPTION_S2C_ENABLED -eq 0 ]; then
        block_size=8
    fi

    local padding_len=$(( (block_size - ((payload_len + 5) % block_size)) % block_size ))
    if [ $padding_len -lt 4 ]; then
        padding_len=$((padding_len + block_size))
    fi

    local padding=$(openssl rand -hex $padding_len)
    local packet_len=$((1 + payload_len + padding_len))

    if [ $ENCRYPTION_S2C_ENABLED -eq 1 ]; then
        log "Sending encrypted: type=$msg_type, seq=$SEQUENCE_NUM_S2C"

        local data_to_encrypt=$(printf "%02x" $padding_len)${full_payload}${padding}
        local encrypted=$(encrypt_aes_ctr "$data_to_encrypt" "$ENCRYPTION_KEY_S2C" "$IV_S2C")

        local mac_input=$(printf "%08x" $SEQUENCE_NUM_S2C)$(printf "%08x" $packet_len)${encrypted}
        local mac=$(compute_hmac "$mac_input" "$MAC_KEY_S2C")

        # Atomic write: packet_length + encrypted_data + MAC
        {
            write_uint32 $packet_len
            hex2bin "$encrypted"
            hex2bin "${mac:0:64}"
        } 2>/dev/null

        SEQUENCE_NUM_S2C=$((SEQUENCE_NUM_S2C + 1))
        PACKETS_SENT=$((PACKETS_SENT + 1))
    else
        log "Sending plaintext: type=$msg_type"

        # Atomic write: all at once
        {
            write_uint32 $packet_len
            printf "%02x" $padding_len | xxd -r -p
            hex2bin "$full_payload"
            hex2bin "$padding"
        } 2>/dev/null

        PACKETS_SENT=$((PACKETS_SENT + 1))
    fi
}

# ============================================================================
# CRYPTOGRAPHY FUNCTIONS
# ============================================================================

compute_hmac() {
    local data_hex="$1"
    local key_hex="$2"
    hex2bin "$data_hex" | openssl dgst -sha256 -mac HMAC -macopt hexkey:$key_hex -binary | bin2hex
}

encrypt_aes_ctr() {
    local plaintext_hex="$1"
    local key_hex="$2"
    local iv_hex="$3"
    hex2bin "$plaintext_hex" | openssl enc -aes-128-ctr -K "$key_hex" -iv "$iv_hex" -nopad 2>/dev/null | bin2hex
}

decrypt_aes_ctr() {
    local ciphertext_hex="$1"
    local key_hex="$2"
    local iv_hex="$3"
    encrypt_aes_ctr "$ciphertext_hex" "$key_hex" "$iv_hex"
}

derive_keys() {
    log "Deriving keys..."

    derive_key() {
        local letter="$1"
        local letter_hex=$(printf "%s" "$letter" | xxd -p)
        local data="${SHARED_SECRET}${EXCHANGE_HASH}${letter_hex}${SESSION_ID}"
        sha256 "$data"
    }

    IV_C2S=$(derive_key "A")
    IV_S2C=$(derive_key "B")
    ENCRYPTION_KEY_C2S=$(derive_key "C")
    ENCRYPTION_KEY_S2C=$(derive_key "D")
    MAC_KEY_C2S=$(derive_key "E")
    MAC_KEY_S2C=$(derive_key "F")

    ENCRYPTION_KEY_C2S="${ENCRYPTION_KEY_C2S:0:32}"
    ENCRYPTION_KEY_S2C="${ENCRYPTION_KEY_S2C:0:32}"
    IV_C2S="${IV_C2S:0:32}"
    IV_S2C="${IV_S2C:0:32}"

    log "Keys derived: ENC_S2C=${ENCRYPTION_KEY_S2C}, IV_S2C=${IV_S2C}"
}

# ============================================================================
# SSH PROTOCOL HANDLERS
# ============================================================================

handle_version_exchange() {
    log "Version exchange starting..."

    # Send server version with explicit flush
    echo -en "${SERVER_VERSION}\r\n"

    log "Waiting for client version..."
    read -r CLIENT_VERSION || return 1
    CLIENT_VERSION="${CLIENT_VERSION%$'\r'}"

    log "Client version: $CLIENT_VERSION"

    if [[ ! "$CLIENT_VERSION" =~ ^SSH-2\.0- ]]; then
        error "Invalid client version"
    fi
}

build_kexinit() {
    local payload=""

    local cookie=$(openssl rand -hex 16)
    payload="${payload}${cookie}"

    local kex_alg="curve25519-sha256,kex-strict-s-v00@openssh.com"
    local host_key_alg="ssh-ed25519"
    local cipher="aes128-ctr"
    local mac="hmac-sha2-256"
    local compress="none"
    local lang=""

    add_namelist() {
        local names="$1"
        local hex=$(echo -n "$names" | bin2hex)
        local len=${#names}
        payload="${payload}$(printf "%08x" $len)${hex}"
    }

    add_namelist "$kex_alg"
    add_namelist "$host_key_alg"
    add_namelist "$cipher"
    add_namelist "$cipher"
    add_namelist "$mac"
    add_namelist "$mac"
    add_namelist "$compress"
    add_namelist "$compress"
    add_namelist "$lang"
    add_namelist "$lang"

    payload="${payload}00"
    payload="${payload}00000000"

    echo "$payload"
}

handle_kexinit() {
    local client_packet="$1"

    log "Handling KEXINIT..."
    CLIENT_KEXINIT="$client_packet"

    SERVER_KEXINIT=$(build_kexinit)
    write_ssh_packet $SSH_MSG_KEXINIT "$SERVER_KEXINIT"

    log "KEXINIT sent"
}

handle_ecdh_init() {
    local payload_hex="$1"

    log "Handling KEX_ECDH_INIT..."

    # Generate host keys if needed
    if [ ! -f "$TMPDIR/ssh_host_ed25519_key" ]; then
        ssh-keygen -t ed25519 -f "$TMPDIR/ssh_host_ed25519_key" -N "" -q
    fi

    # Parse client's public key
    local client_pubkey_len=$((16#${payload_hex:0:8}))
    local client_pubkey="${payload_hex:8:$((client_pubkey_len * 2))}"

    log "  Client pubkey: ${client_pubkey:0:60}..."

    # Generate ephemeral Curve25519 key pair
    openssl genpkey -algorithm X25519 -out "$TMPDIR/server_x25519.pem" 2>/dev/null
    openssl pkey -in "$TMPDIR/server_x25519.pem" -pubout -out "$TMPDIR/server_x25519_pub.pem" 2>/dev/null

    # Extract public key
    local server_pubkey=$(openssl pkey -in "$TMPDIR/server_x25519_pub.pem" -pubin -text -noout | \
        grep -A 3 "pub:" | tail -n 3 | tr -d ' \n:' | xxd -r -p | bin2hex)

    log "  Server pubkey: ${server_pubkey:0:60}..."

    # Compute shared secret
    hex2bin "$client_pubkey" > "$TMPDIR/client_x25519_pub.bin"
    openssl pkeyutl -derive -inkey "$TMPDIR/server_x25519.pem" \
        -peerkey <(echo "-----BEGIN PUBLIC KEY-----"; \
                   base64 < <(cat <(echo -n "302a300506032b656e032100") <(cat "$TMPDIR/client_x25519_pub.bin") | xxd -r -p); \
                   echo "-----END PUBLIC KEY-----") \
        -out "$TMPDIR/shared_secret.bin" 2>/dev/null

    SHARED_SECRET=$(cat "$TMPDIR/shared_secret.bin" | bin2hex)
    log "  Shared secret: ${SHARED_SECRET:0:40}..."

    # Get Ed25519 host public key
    local host_pubkey_der=$(ssh-keygen -e -f "$TMPDIR/ssh_host_ed25519_key.pub" -m pkcs8 | \
        openssl pkey -pubin -outform DER 2>/dev/null | bin2hex)
    local host_pubkey="${host_pubkey_der: -64}"

    # Build exchange hash H
    local V_C=$(echo -n "$CLIENT_VERSION" | bin2hex)
    local V_S=$(echo -n "$SERVER_VERSION" | bin2hex)
    local I_C="$CLIENT_KEXINIT"
    local I_S="$SERVER_KEXINIT"

    local K_S_data="$(printf "%08x" 11)$(echo -n "ssh-ed25519" | bin2hex)$(printf "%08x" 32)${host_pubkey}"
    local K_S="$(printf "%08x" $((${#K_S_data} / 2)))${K_S_data}"

    local Q_C="$(printf "%08x" $((client_pubkey_len)))${client_pubkey}"
    local Q_S="$(printf "%08x" 32)${server_pubkey}"
    local K="$(printf "%08x" 32)${SHARED_SECRET}"

    local hash_input=""
    hash_input="${hash_input}$(printf "%08x" ${#CLIENT_VERSION})${V_C}"
    hash_input="${hash_input}$(printf "%08x" ${#SERVER_VERSION})${V_S}"
    hash_input="${hash_input}$(printf "%08x" $((${#I_C} / 2)))${I_C}"
    hash_input="${hash_input}$(printf "%08x" $((${#I_S} / 2)))${I_S}"
    hash_input="${hash_input}${K_S}"
    hash_input="${hash_input}${Q_C}"
    hash_input="${hash_input}${Q_S}"
    hash_input="${hash_input}${K}"

    EXCHANGE_HASH=$(sha256 "$hash_input")
    log "  Exchange hash: ${EXCHANGE_HASH:0:40}..."

    if [ -z "$SESSION_ID" ]; then
        SESSION_ID="$EXCHANGE_HASH"
    fi

    # Sign H with Ed25519
    hex2bin "$EXCHANGE_HASH" > "$TMPDIR/hash_to_sign.bin"
    local signature=$(ssh-keygen -Y sign -f "$TMPDIR/ssh_host_ed25519_key" \
        -n "" "$TMPDIR/hash_to_sign.bin" 2>/dev/null | \
        grep -v "BEGIN\|END" | base64 -d | tail -c 64 | bin2hex)

    log "  Signature: ${signature:0:40}..."

    # Build KEX_ECDH_REPLY
    local reply=""
    reply="${reply}${K_S}"
    reply="${reply}$(printf "%08x" 32)${server_pubkey}"

    local sig_blob=$(printf "%08x" 11; echo -n "ssh-ed25519" | bin2hex; printf "%08x" 64; echo "$signature")
    reply="${reply}$(printf "%08x" $((${#sig_blob} / 2)))${sig_blob}"

    write_ssh_packet $SSH_MSG_KEX_ECDH_REPLY "$reply"
    log "KEX_ECDH_REPLY sent"

    # Derive keys before NEWKEYS
    derive_keys

    # Send NEWKEYS
    write_ssh_packet $SSH_MSG_NEWKEYS ""
    log "NEWKEYS sent"

    # Enable S2C encryption
    SEQUENCE_NUM_S2C=$PACKETS_SENT
    ENCRYPTION_S2C_ENABLED=1
    log "S2C encryption ENABLED (seq=$SEQUENCE_NUM_S2C)"
}

handle_service_request() {
    local payload_hex="$1"

    log "Handling SERVICE_REQUEST..."

    local service_len=$((16#${payload_hex:0:8}))
    local service_hex="${payload_hex:8:$((service_len * 2))}"
    local service=$(hex2bin "$service_hex")

    log "  Service: $service"

    local response=$(echo -n "$service" | bin2hex)
    local response_with_len="$(printf "%08x" ${#service})${response}"

    write_ssh_packet $SSH_MSG_SERVICE_ACCEPT "$response_with_len"
    log "SERVICE_ACCEPT sent"
}

handle_userauth_request() {
    local payload_hex="$1"

    log "Handling USERAUTH_REQUEST..."

    # Parse username
    local pos=0
    local user_len=$((16#${payload_hex:$pos:8}))
    pos=$((pos + 8))
    local username=$(hex2bin "${payload_hex:$pos:$((user_len * 2))}")
    pos=$((pos + user_len * 2))

    log "  Username: $username"

    # Skip service name
    local service_len=$((16#${payload_hex:$pos:8}))
    pos=$((pos + 8 + service_len * 2))

    # Get auth method
    local method_len=$((16#${payload_hex:$pos:8}))
    pos=$((pos + 8))
    local method=$(hex2bin "${payload_hex:$pos:$((method_len * 2))}")
    pos=$((pos + method_len * 2))

    log "  Method: $method"

    if [ "$method" = "password" ]; then
        pos=$((pos + 2))  # Skip boolean
        local pass_len=$((16#${payload_hex:$pos:8}))
        pos=$((pos + 8))
        local password=$(hex2bin "${payload_hex:$pos:$((pass_len * 2))}")

        if [ "$username" = "$VALID_USERNAME" ] && [ "$password" = "$VALID_PASSWORD" ]; then
            log "Authentication SUCCESS"
            write_ssh_packet $SSH_MSG_USERAUTH_SUCCESS ""
        else
            log "Authentication FAILED"
            local methods="password"
            local methods_hex=$(echo -n "$methods" | bin2hex)
            local response="$(printf "%08x" ${#methods})${methods_hex}00"
            write_ssh_packet $SSH_MSG_USERAUTH_FAILURE "$response"
        fi
    else
        log "Unsupported auth method: $method"
        local methods="password"
        local methods_hex=$(echo -n "$methods" | bin2hex)
        local response="$(printf "%08x" ${#methods})${methods_hex}00"
        write_ssh_packet $SSH_MSG_USERAUTH_FAILURE "$response"
    fi
}

handle_channel_open() {
    local payload_hex="$1"

    log "Handling CHANNEL_OPEN..."

    local type_len=$((16#${payload_hex:0:8}))
    local pos=$((8 + type_len * 2))
    local sender_channel=$((16#${payload_hex:$pos:8}))

    log "  Sender channel: $sender_channel"

    local response=""
    response="${response}$(printf "%08x" $sender_channel)"
    response="${response}$(printf "%08x" 0)"
    response="${response}$(printf "%08x" 32768)"
    response="${response}$(printf "%08x" 32768)"

    write_ssh_packet $SSH_MSG_CHANNEL_OPEN_CONFIRMATION "$response"
    log "CHANNEL_OPEN_CONFIRMATION sent"
}

handle_channel_request() {
    local payload_hex="$1"

    log "Handling CHANNEL_REQUEST..."

    local recipient=$((16#${payload_hex:0:8}))
    local pos=8

    local req_len=$((16#${payload_hex:$pos:8}))
    pos=$((pos + 8))
    local request=$(hex2bin "${payload_hex:$pos:$((req_len * 2))}")

    log "  Request: $request"

    if [ "$request" = "shell" ]; then
        write_ssh_packet $SSH_MSG_CHANNEL_SUCCESS "$(printf "%08x" $recipient)"

        # Send "Hello World" message
        local message="Hello World"
        local message_hex=$(echo -n "$message" | bin2hex)
        local data="$(printf "%08x" $recipient)$(printf "%08x" ${#message})${message_hex}"
        write_ssh_packet $SSH_MSG_CHANNEL_DATA "$data"

        log "Sent: $message"

        # Send EOF and close
        write_ssh_packet $SSH_MSG_CHANNEL_EOF "$(printf "%08x" $recipient)"
        write_ssh_packet $SSH_MSG_CHANNEL_CLOSE "$(printf "%08x" $recipient)"
    else
        write_ssh_packet $SSH_MSG_CHANNEL_SUCCESS "$(printf "%08x" $recipient)"
    fi
}

# ============================================================================
# MAIN CONNECTION HANDLER
# ============================================================================

handle_connection() {
    log "=== Connection started ==="
    log "Transport: Direct socket (no socat EXEC wrapper)"

    # If running under systemd-socket-activate with --inetd,
    # stdin/stdout are already connected to the socket
    # Just ensure binary mode
    exec 2>>"$TMPDIR/connection.log"

    # Version exchange
    handle_version_exchange || exit 1

    # Main protocol loop
    while true; do
        read -t 30 msg_type msg_payload <<< $(read_ssh_packet) || {
            log "Connection closed or timeout"
            break
        }

        log "Received message type: $msg_type"

        case $msg_type in
            $SSH_MSG_KEXINIT)
                handle_kexinit "$msg_payload"
                ;;
            $SSH_MSG_KEX_ECDH_INIT)
                handle_ecdh_init "$msg_payload"
                ;;
            $SSH_MSG_NEWKEYS)
                SEQUENCE_NUM_C2S=$PACKETS_RECEIVED
                ENCRYPTION_C2S_ENABLED=1
                log "Client NEWKEYS - C2S encryption ENABLED (seq=$SEQUENCE_NUM_C2S)"
                ;;
            $SSH_MSG_SERVICE_REQUEST)
                handle_service_request "$msg_payload"
                ;;
            $SSH_MSG_USERAUTH_REQUEST)
                handle_userauth_request "$msg_payload"
                ;;
            $SSH_MSG_CHANNEL_OPEN)
                handle_channel_open "$msg_payload"
                ;;
            $SSH_MSG_CHANNEL_REQUEST)
                handle_channel_request "$msg_payload"
                break
                ;;
            $SSH_MSG_CHANNEL_CLOSE)
                log "Client closed channel"
                break
                ;;
            *)
                log "Unknown message: $msg_type"
                ;;
        esac
    done

    log "=== Connection complete ==="
}

# Entry point
handle_connection
