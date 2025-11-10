#!/bin/bash
# Test MAC computation with known vectors from actual SSH session

TESTDIR="/tmp/mac_test_$$"
mkdir -p "$TESTDIR"

# Source the MAC library
source "$(dirname "$0")/mac_lib.sh"
WORKDIR="$TESTDIR"

echo "=== MAC Test Vectors from Actual SSH Session ==="
echo ""

# Test Vector 1: SERVICE_REQUEST from actual session
echo "Test Vector 1: SERVICE_REQUEST packet"
echo "--------------------------------------"

SEQ=0
MAC_KEY_C2S="5c13acb747535a8b2b53a7626ce8fa9aa5d11a8129182f31f0628d4163acbbff"

# Plaintext packet (32 bytes: 4-byte length + 28-byte packet)
# From debug log: 00 00 00 1c 0a 05 00 00 00 0c 73 73 68 2d 75 73 65 72 61 75 74 68 32 ab 1d 53 77 4d a9 1d d0 cd
printf "\\x00\\x00\\x00\\x1c\\x0a\\x05\\x00\\x00\\x00\\x0c\\x73\\x73\\x68\\x2d\\x75\\x73\\x65\\x72\\x61\\x75\\x74\\x68\\x32\\xab\\x1d\\x53\\x77\\x4d\\xa9\\x1d\\xd0\\xcd" > "$TESTDIR/packet1"

# Expected MAC from client
printf "\\xb8\\x4d\\x18\\x7d\\x06\\x2a\\x8b\\x8a\\x0d\\x74\\xde\\x76\\xdd\\x4d\\xf1\\xbe\\xff\\x46\\x3a\\x89\\x1c\\x5e\\xff\\xac\\x45\\xb9\\x0d\\x8f\\xb7\\x59\\x3c\\x24" > "$TESTDIR/expected1"

echo "Input data:"
echo "  Sequence: $SEQ"
echo "  Packet (32 bytes):"
od -An -tx1 "$TESTDIR/packet1" | head -2
echo "  MAC Key (32 bytes): $MAC_KEY_C2S"
echo ""

# Compute MAC using our library
compute_mac_standard "$SEQ" "$TESTDIR/packet1" "$MAC_KEY_C2S" "$TESTDIR/computed1"

echo "Results:"
echo "  Computed MAC:"
od -An -tx1 "$TESTDIR/computed1"
echo "  Expected MAC:"
od -An -tx1 "$TESTDIR/expected1"
echo ""

if verify_mac "$TESTDIR/computed1" "$TESTDIR/expected1"; then
    echo "  ✅ MATCH!"
else
    echo "  ❌ MISMATCH"
fi

echo ""
echo "=== Export Test Vector for C Program ==="
echo ""

# Export in C-friendly format
cat > "$TESTDIR/test_vector.h" << 'EOFHEADER'
// Test vector from actual SSH session
#ifndef TEST_VECTOR_H
#define TEST_VECTOR_H

#include <stdint.h>

// Test Vector 1: SERVICE_REQUEST
static const uint32_t seq_num_1 = 0;

static const uint8_t packet_1[] = {
    0x00, 0x00, 0x00, 0x1c, 0x0a, 0x05, 0x00, 0x00,
    0x00, 0x0c, 0x73, 0x73, 0x68, 0x2d, 0x75, 0x73,
    0x65, 0x72, 0x61, 0x75, 0x74, 0x68, 0x32, 0xab,
    0x1d, 0x53, 0x77, 0x4d, 0xa9, 0x1d, 0xd0, 0xcd
};
static const size_t packet_1_len = 32;

static const uint8_t mac_key_1[] = {
    0x5c, 0x13, 0xac, 0xb7, 0x47, 0x53, 0x5a, 0x8b,
    0x2b, 0x53, 0xa7, 0x62, 0x6c, 0xe8, 0xfa, 0x9a,
    0xa5, 0xd1, 0x1a, 0x81, 0x29, 0x18, 0x2f, 0x31,
    0xf0, 0x62, 0x8d, 0x41, 0x63, 0xac, 0xbb, 0xff
};
static const size_t mac_key_1_len = 32;

static const uint8_t expected_mac_1[] = {
    0xb8, 0x4d, 0x18, 0x7d, 0x06, 0x2a, 0x8b, 0x8a,
    0x0d, 0x74, 0xde, 0x76, 0xdd, 0x4d, 0xf1, 0xbe,
    0xff, 0x46, 0x3a, 0x89, 0x1c, 0x5e, 0xff, 0xac,
    0x45, 0xb9, 0x0d, 0x8f, 0xb7, 0x59, 0x3c, 0x24
};
static const size_t expected_mac_1_len = 32;

#endif // TEST_VECTOR_H
EOFHEADER

echo "Created: $TESTDIR/test_vector.h"
echo ""

# Create a simple test data file for easier analysis
cat > "$TESTDIR/test_data.txt" << EOFDATA
Test Vector 1: SERVICE_REQUEST

Sequence Number: $SEQ (0x$(printf "%08x" $SEQ))

Packet (32 bytes):
$(od -An -tx1 "$TESTDIR/packet1")

MAC Key (32 bytes):
$(echo -n "$MAC_KEY_C2S" | sed 's/../0x& /g' | fold -w 48)

Expected MAC (32 bytes):
$(od -An -tx1 "$TESTDIR/expected1")

MAC Input (36 bytes = 4-byte seq + 32-byte packet):
$(compute_mac_standard "$SEQ" "$TESTDIR/packet1" "$MAC_KEY_C2S" "$TESTDIR/tmp_mac" && \
  { write_uint32 "$SEQ"; cat "$TESTDIR/packet1"; } | od -An -tx1)
EOFDATA

echo "Created: $TESTDIR/test_data.txt"
echo ""
echo "Files created in: $TESTDIR"
ls -la "$TESTDIR"

echo ""
echo "To use in C program:"
echo "  cp $TESTDIR/test_vector.h /home/user/nano_ssh_server/v0-vanilla/"
