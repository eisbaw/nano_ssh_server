#!/bin/bash
# Sourced library for MAC computation - for testing and comparison

WORKDIR="${WORKDIR:-/tmp/mac_lib_$$}"
mkdir -p "$WORKDIR"

# === Binary Utilities ===

bin_to_hex() {
    od -An -tx1 | tr -d ' \n'
}

hex_to_bin() {
    local hex="$1"
    echo -n "$hex" | sed 's/../\\x&/g' | xargs -0 printf
}

write_uint32() {
    local val=$1
    local hex=$(printf "%08x" "$val")
    printf "\\x${hex:0:2}\\x${hex:2:2}\\x${hex:4:2}\\x${hex:6:2}"
}

write_string() {
    local str="$1"
    local len=${#str}
    write_uint32 "$len"
    printf "%s" "$str"
}

write_string_from_file() {
    local file="$1"
    local len=$(wc -c < "$file")
    write_uint32 "$len"
    cat "$file"
}

write_mpint_from_file() {
    local input_file="$1"
    local size=$(wc -c < "$input_file")

    # Check if high bit is set (would make it negative)
    local first_byte=$(head -c 1 "$input_file" | bin_to_hex)
    if [ $((0x$first_byte & 0x80)) -ne 0 ]; then
        # Add zero padding byte
        write_uint32 $((size + 1))
        printf "\\x00"
        cat "$input_file"
    else
        write_uint32 "$size"
        cat "$input_file"
    fi
}

# === Key Derivation ===

derive_key() {
    local K_file="$1"      # Shared secret file
    local H_file="$2"      # Exchange hash file
    local letter="$3"      # Key letter (A-F)
    local session_id="$4"  # Session ID file
    local output="$5"      # Output file

    # Key = HASH(K || H || letter || session_id)
    # Note: K must be encoded as mpint!
    {
        write_mpint_from_file "$K_file"
        cat "$H_file"
        printf "%s" "$letter"
        cat "$session_id"
    } | openssl dgst -sha256 -binary > "$output"
}

# === MAC Computation ===

# Standard SSH MAC: MAC(sequence_number || unencrypted_packet)
compute_mac_standard() {
    local seq_num="$1"      # Sequence number (uint32)
    local packet_file="$2"  # Packet file (with 4-byte length prefix)
    local mac_key_hex="$3"  # MAC key in hex
    local output="$4"       # Output file

    # Create MAC input: sequence || packet
    {
        write_uint32 "$seq_num"
        cat "$packet_file"
    } > "$WORKDIR/mac_input"

    # Compute HMAC-SHA-256
    cat "$WORKDIR/mac_input" | \
        openssl dgst -sha256 -mac HMAC -macopt "hexkey:$mac_key_hex" -binary > "$output"
}

# ETM (Encrypt-Then-MAC): MAC(sequence_number || encrypted_packet)
compute_mac_etm() {
    local seq_num="$1"      # Sequence number (uint32)
    local packet_file="$2"  # Encrypted packet file (with 4-byte length prefix)
    local mac_key_hex="$3"  # MAC key in hex
    local output="$4"       # Output file

    # Create MAC input: sequence || encrypted_packet
    {
        write_uint32 "$seq_num"
        cat "$packet_file"
    } > "$WORKDIR/mac_input_etm"

    # Compute HMAC-SHA-256
    cat "$WORKDIR/mac_input_etm" | \
        openssl dgst -sha256 -mac HMAC -macopt "hexkey:$mac_key_hex" -binary > "$output"
}

# Verify MAC
verify_mac() {
    local computed_file="$1"
    local received_file="$2"

    if cmp -s "$computed_file" "$received_file"; then
        return 0  # Match
    else
        return 1  # Mismatch
    fi
}

# === Debug Helpers ===

debug_mac_input() {
    local seq_num="$1"
    local packet_file="$2"

    {
        write_uint32 "$seq_num"
        cat "$packet_file"
    } > "$WORKDIR/debug_mac_input"

    echo "Sequence: $seq_num (hex: $(printf "%08x" $seq_num))"
    echo "Packet size: $(wc -c < "$packet_file") bytes"
    echo "MAC input size: $(wc -c < "$WORKDIR/debug_mac_input") bytes"
    echo "MAC input (hex):"
    od -An -tx1 "$WORKDIR/debug_mac_input" | head -5
}

debug_mac_computation() {
    local seq_num="$1"
    local packet_file="$2"
    local mac_key_hex="$3"

    echo "=== MAC Computation Debug ==="
    echo "Sequence number: $seq_num"
    echo "Packet file: $packet_file ($(wc -c < "$packet_file") bytes)"
    echo "MAC key: $mac_key_hex"
    echo ""

    debug_mac_input "$seq_num" "$packet_file"

    echo ""
    echo "Computing MAC..."
    compute_mac_standard "$seq_num" "$packet_file" "$mac_key_hex" "$WORKDIR/debug_mac_output"

    echo "Computed MAC:"
    od -An -tx1 "$WORKDIR/debug_mac_output"
}
