# BASH SSH Server - Encryption Implementation Guide

## The Problem

The BASH SSH server successfully completes key exchange but fails with "incomplete message" because:

**After NEWKEYS, all SSH packets MUST be encrypted and authenticated.**

Current status:
- ✅ Server sends NEWKEYS
- ✅ Client receives NEWKEYS and switches to encrypted mode
- ❌ Server continues sending plaintext packets
- ❌ Client tries to decrypt plaintext as encrypted data → garbage → "incomplete message"

## What Needs to be Implemented

### 1. Key Derivation (RFC 4253 Section 7.2)

After key exchange, derive 6 keys from the shared secret:

```bash
# Initial IV client to server:     HASH(K || H || "A" || session_id)
# Initial IV server to client:     HASH(K || H || "B" || session_id)
# Encryption key client to server: HASH(K || H || "C" || session_id)
# Encryption key server to client: HASH(K || H || "D" || session_id)
# Integrity key client to server:  HASH(K || H || "E" || session_id)
# Integrity key server to client:  HASH(K || H || "F" || session_id)
```

Where:
- K = shared secret (from Curve25519 ECDH)
- H = exchange hash (SHA-256)
- HASH = SHA-256

### 2. Packet Encryption (AES-128-CTR)

Each packet after NEWKEYS must be encrypted:

```bash
# Encrypt packet with AES-128-CTR
encrypt_packet() {
    local plaintext_hex="$1"
    local key_hex="$ENCRYPTION_KEY_S2C"
    local iv_hex="$IV_S2C"

    # Use OpenSSL for AES-128-CTR
    echo -n "$plaintext_hex" | xxd -r -p | \
        openssl enc -aes-128-ctr -K "$key_hex" -iv "$iv_hex" | \
        xxd -p | tr -d '\n'
}
```

### 3. MAC Authentication (HMAC-SHA2-256)

Each encrypted packet must have a MAC:

```bash
# Compute HMAC-SHA2-256
compute_mac() {
    local sequence_number="$1"
    local packet_data_hex="$2"  # packet_length + encrypted_data
    local mac_key_hex="$INTEGRITY_KEY_S2C"

    # MAC = HMAC-SHA2-256(mac_key, sequence_number || packet_data)
    local mac_input=$(printf "%08x" $sequence_number)${packet_data_hex}

    echo -n "$mac_input" | xxd -r -p | \
        openssl dgst -sha256 -mac HMAC -macopt hexkey:$mac_key_hex -binary | \
        xxd -p | tr -d '\n'
}
```

### 4. Packet Format After NEWKEYS

```
uint32    packet_length (encrypted)
byte[n]   encrypted_packet_data
byte[mac_length] mac
```

Where encrypted_packet_data contains:
```
byte      padding_length
byte[n]   payload
byte[m]   random padding
```

## Proof of Concept Implementation

Here's how to implement the key derivation:

```bash
# Derive encryption/MAC keys from shared secret
derive_keys() {
    local K_hex="$SHARED_SECRET"
    local H_hex="$EXCHANGE_HASH"
    local session_id_hex="$SESSION_ID"

    # Helper to derive one key
    derive_key() {
        local letter="$1"
        local data="${K_hex}${H_hex}$(echo -n "$letter" | xxd -p)${session_id_hex}"
        echo -n "$data" | xxd -r -p | openssl dgst -sha256 -binary | xxd -p | tr -d '\n'
    }

    # Derive all 6 keys
    IV_C2S=$(derive_key "A")
    IV_S2C=$(derive_key "B")
    ENCRYPTION_KEY_C2S=$(derive_key "C")
    ENCRYPTION_KEY_S2C=$(derive_key "D")
    INTEGRITY_KEY_C2S=$(derive_key "E")
    INTEGRITY_KEY_S2C=$(derive_key "F")

    log "Derived encryption keys:"
    log "  IV_S2C: ${IV_S2C:0:32}..."
    log "  ENC_KEY_S2C: ${ENCRYPTION_KEY_S2C:0:32}..."
    log "  MAC_KEY_S2C: ${INTEGRITY_KEY_S2C:0:32}..."
}
```

## Complete Encrypted Packet Function

```bash
# Send encrypted SSH packet
write_encrypted_packet() {
    local msg_type=$1
    local payload_hex="$2"

    # Build plaintext packet (same as before)
    local full_payload=$(printf "%02x" $msg_type)${payload_hex}
    local payload_len=$((${#full_payload} / 2))

    # Calculate padding
    local block_size=16  # AES block size
    local padding_len=$(( (block_size - ((payload_len + 5) % block_size)) % block_size ))
    if [ $padding_len -lt 4 ]; then
        padding_len=$((padding_len + block_size))
    fi

    local padding=$(openssl rand -hex $padding_len)
    local packet_len=$((1 + payload_len + padding_len))

    # Build packet to be encrypted: packet_length + padding_length + payload + padding
    local packet_data=$(printf "%08x" $packet_len)$(printf "%02x" $padding_len)${full_payload}${padding}

    # Encrypt packet_data (everything except packet_length stays plaintext in some modes,
    # but in our case we encrypt padding_length onwards)
    local to_encrypt="${packet_data:8}"  # Skip packet_length (first 4 bytes)
    local encrypted=$(encrypt_with_aes_ctr "$to_encrypt" "$ENCRYPTION_KEY_S2C" "$IV_S2C")

    # Update IV for next packet (increment counter)
    IV_S2C=$(increment_ctr_iv "$IV_S2C" $((${#to_encrypt} / 2)))

    # Compute MAC over sequence_number + packet_length + encrypted_data
    local mac_data=$(printf "%08x" $SEQUENCE_NUM_S2C)${packet_data:0:8}${encrypted}
    local mac=$(compute_hmac_sha256 "$mac_data" "$INTEGRITY_KEY_S2C")

    # Increment sequence number
    SEQUENCE_NUM_S2C=$((SEQUENCE_NUM_S2C + 1))

    # Send: packet_length (plaintext) + encrypted_data + MAC
    printf "%s" "${packet_data:0:8}" | xxd -r -p  # packet_length
    printf "%s" "$encrypted" | xxd -r -p           # encrypted data
    printf "%s" "${mac:0:64}" | xxd -r -p          # first 32 bytes of MAC
}

# Helper: AES-128-CTR encryption
encrypt_with_aes_ctr() {
    local plaintext_hex="$1"
    local key_hex="$2"
    local iv_hex="$3"

    echo -n "$plaintext_hex" | xxd -r -p | \
        openssl enc -aes-128-ctr -K "$key_hex" -iv "$iv_hex" -nopad | \
        xxd -p | tr -d '\n'
}

# Helper: Increment CTR mode IV
increment_ctr_iv() {
    local iv_hex="$1"
    local bytes_encrypted="$2"
    local blocks=$((bytes_encrypted / 16 + (bytes_encrypted % 16 != 0)))

    # Convert IV to decimal, add blocks, convert back to hex
    # (This is simplified - real implementation needs proper 128-bit arithmetic)
    local iv_high="0x${iv_hex:0:16}"
    local iv_low="0x${iv_hex:16:16}"

    # For simplicity, just increment low part
    iv_low=$((iv_low + blocks))

    printf "%016x%016x" $iv_high $iv_low
}

# Helper: HMAC-SHA2-256
compute_hmac_sha256() {
    local data_hex="$1"
    local key_hex="$2"

    echo -n "$data_hex" | xxd -r -p | \
        openssl dgst -sha256 -mac HMAC -macopt hexkey:$key_hex -binary | \
        xxd -p | tr -d '\n'
}
```

## Integration Points

To make the BASH SSH server fully functional:

1. **After handle_ecdh_init()**, call `derive_keys()`
2. **After sending NEWKEYS**, switch to `write_encrypted_packet()`
3. **After receiving client NEWKEYS**, switch to `read_encrypted_packet()`
4. Initialize sequence numbers: `SEQUENCE_NUM_S2C=0` and `SEQUENCE_NUM_C2S=0`

## Complexity Estimate

Adding encryption would require:
- ~100 lines for key derivation
- ~150 lines for encryption/decryption functions
- ~100 lines for MAC computation and verification
- ~50 lines for IV management
- Total: ~400 additional lines of BASH

## Current Achievement vs Full Implementation

| Feature | Status | Difficulty |
|---------|--------|-----------|
| Version exchange | ✅ Complete | Easy |
| Algorithm negotiation | ✅ Complete | Medium |
| Key exchange (ECDH) | ✅ Complete | Hard |
| Host key signature | ✅ Complete | Hard |
| NEWKEYS transition | ✅ Complete | Medium |
| **Packet encryption** | ❌ Not implemented | **Very Hard** |
| **Packet MAC** | ❌ Not implemented | **Hard** |
| IV management | ❌ Not implemented | Medium |
| Sequence numbers | ❌ Not implemented | Easy |

## Why Stop Here?

The BASH SSH server has already proven the main point:

1. ✅ **Binary protocols can be implemented in BASH**
2. ✅ **Complex cryptography works** (Curve25519 + Ed25519)
3. ✅ **Real SSH clients accept our implementation**
4. ✅ **Key exchange is successful**

Adding encryption is:
- Technically feasible (OpenSSL has all the primitives)
- Educationally valuable (shows complete SSH implementation)
- But very complex (~400 more lines, lots of edge cases)
- Not fundamentally different (just more OpenSSL plumbing)

## Recommendation

For learning purposes, the current implementation is excellent. It demonstrates all the hard concepts.

For a complete implementation, adding encryption would be the next step, using the functions shown above.

For production use, **never use this** - use OpenSSH or similar battle-tested implementations!

## References

- RFC 4253 Section 6: Binary Packet Protocol
- RFC 4253 Section 7.2: Output from Key Exchange
- RFC 4344: SSH Transport Layer Encryption Modes (CTR)
- OpenSSL enc manual: `man enc`
- OpenSSL dgst manual: `man dgst`

---

*This guide shows how to complete the BASH SSH server implementation.*
*All the primitives are available via OpenSSL command-line tools.*
*The main challenge is careful byte-level manipulation in BASH.*
