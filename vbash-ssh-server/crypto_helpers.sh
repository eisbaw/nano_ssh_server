#!/bin/bash
#
# Cryptographic helper functions for BASH SSH server
# Uses OpenSSL for all cryptographic operations
#

# Generate Ed25519 host key pair
generate_host_key() {
    local key_file="$1"
    local pub_file="$2"

    # Generate private key
    openssl genpkey -algorithm ED25519 -out "$key_file" 2>/dev/null

    # Extract public key in raw format (32 bytes)
    # Ed25519 public keys are 32 bytes
    openssl pkey -in "$key_file" -pubout -outform DER 2>/dev/null | tail -c 32 > "$pub_file"
}

# Generate Curve25519 ephemeral key pair for key exchange
generate_curve25519_keypair() {
    local priv_file="$1"
    local pub_file="$2"

    # Generate private key
    openssl genpkey -algorithm X25519 -out "$priv_file" 2>/dev/null

    # Extract public key (32 bytes)
    openssl pkey -in "$priv_file" -pubout -outform DER 2>/dev/null | tail -c 32 > "$pub_file"
}

# Perform Curve25519 key exchange
# Returns: 32-byte shared secret
curve25519_exchange() {
    local our_private="$1"    # Our X25519 private key (PEM format)
    local their_public="$2"   # Their X25519 public key (32 bytes raw)
    local output="$3"         # Output file for shared secret

    # Convert their public key to PEM format
    local their_pem="/tmp/their_pub_$$.pem"

    # Create a DER formatted public key (X25519 OID + 32 byte key)
    # This is a hack - in real implementation, we'd properly encode DER
    {
        # X25519 public key DER structure (42 bytes header + 32 bytes key)
        # Simplified: just use the raw key and wrap it
        echo -n "-----BEGIN PUBLIC KEY-----"
        {
            # DER header for X25519 public key (12 bytes)
            printf '\x30\x2a\x30\x05\x06\x03\x2b\x65\x6e\x03\x21\x00'
            cat "$their_public"
        } | base64
        echo "-----END PUBLIC KEY-----"
    } > "$their_pem"

    # Derive shared secret
    openssl pkeyutl -derive -inkey "$our_private" -peerkey "$their_pem" -out "$output" 2>/dev/null

    # Cleanup
    rm -f "$their_pem"
}

# Compute SHA-256 hash
sha256() {
    local input="$1"
    echo -n "$input" | openssl dgst -sha256 -binary
}

# Compute SHA-256 hash of file
sha256_file() {
    local file="$1"
    openssl dgst -sha256 -binary < "$file"
}

# Compute HMAC-SHA256
hmac_sha256() {
    local key="$1"      # Key (hex string)
    local message="$2"  # Message (binary data)
    local output="$3"   # Output file

    # Convert hex key to binary
    echo -n "$key" | xxd -r -p > /tmp/hmac_key_$$

    # Compute HMAC
    echo -n "$message" | openssl dgst -sha256 -mac HMAC -macopt hexkey:"$key" -binary > "$output"

    rm -f /tmp/hmac_key_$$
}

# Sign data with Ed25519 private key
ed25519_sign() {
    local private_key="$1"  # PEM format Ed25519 private key
    local data="$2"         # Data to sign (file)
    local signature="$3"    # Output signature file

    # Sign with Ed25519
    openssl pkeyutl -sign -inkey "$private_key" -rawin -in "$data" -out "$signature" 2>/dev/null
}

# Verify Ed25519 signature
ed25519_verify() {
    local public_key="$1"   # PEM format Ed25519 public key
    local data="$2"         # Data that was signed (file)
    local signature="$3"    # Signature file

    # Verify signature
    openssl pkeyutl -verify -pubin -inkey "$public_key" -rawin -in "$data" -sigfile "$signature" 2>/dev/null
    return $?
}

# Generate random bytes
random_bytes() {
    local count="$1"
    local output="$2"

    dd if=/dev/urandom bs=1 count="$count" of="$output" 2>/dev/null
}

# AES-128-CTR encryption (for SSH packets after NEWKEYS)
aes128_ctr_encrypt() {
    local key="$1"      # 16 bytes (hex)
    local iv="$2"       # 16 bytes (hex)
    local input="$3"    # Input file
    local output="$4"   # Output file

    # Convert key and IV from hex to binary
    echo -n "$key" | xxd -r -p > /tmp/aes_key_$$
    echo -n "$iv" | xxd -r -p > /tmp/aes_iv_$$

    # Encrypt with AES-128-CTR
    openssl enc -aes-128-ctr -K "$key" -iv "$iv" -in "$input" -out "$output" 2>/dev/null

    rm -f /tmp/aes_key_$$ /tmp/aes_iv_$$
}

# AES-128-CTR decryption (same as encryption for CTR mode)
aes128_ctr_decrypt() {
    aes128_ctr_encrypt "$@"
}

# Derive encryption/MAC keys from shared secret (simplified)
derive_keys() {
    local shared_secret="$1"   # 32-byte shared secret (file)
    local exchange_hash="$2"   # Exchange hash H (file)
    local session_id="$3"      # Session ID (usually same as H for first exchange)
    local output_dir="$4"      # Directory to store derived keys

    mkdir -p "$output_dir"

    # In real SSH, keys are derived as:
    # - Initial IV client to server: HASH(K || H || "A" || session_id)
    # - Initial IV server to client: HASH(K || H || "B" || session_id)
    # - Encryption key client to server: HASH(K || H || "C" || session_id)
    # - Encryption key server to client: HASH(K || H || "D" || session_id)
    # - Integrity key client to server: HASH(K || H || "E" || session_id)
    # - Integrity key server to client: HASH(K || H || "F" || session_id)

    # Simplified: just use SHA-256 of shared secret + letter
    for letter in A B C D E F; do
        cat "$shared_secret" "$exchange_hash" <(echo -n "$letter") "$session_id" | sha256_file - > "$output_dir/key_$letter"
    done

    # Extract specific keys
    head -c 16 "$output_dir/key_C" > "$output_dir/enc_key_c2s"  # Client to server encryption
    head -c 16 "$output_dir/key_D" > "$output_dir/enc_key_s2c"  # Server to client encryption
    head -c 32 "$output_dir/key_E" > "$output_dir/mac_key_c2s"  # Client to server MAC
    head -c 32 "$output_dir/key_F" > "$output_dir/mac_key_s2c"  # Server to client MAC
    head -c 16 "$output_dir/key_A" > "$output_dir/iv_c2s"       # Client to server IV
    head -c 16 "$output_dir/key_B" > "$output_dir/iv_s2c"       # Server to client IV
}

# Export functions if sourced
if [ "${BASH_SOURCE[0]}" != "${0}" ]; then
    export -f generate_host_key
    export -f generate_curve25519_keypair
    export -f curve25519_exchange
    export -f sha256
    export -f sha256_file
    export -f hmac_sha256
    export -f ed25519_sign
    export -f ed25519_verify
    export -f random_bytes
    export -f aes128_ctr_encrypt
    export -f aes128_ctr_decrypt
    export -f derive_keys
fi
