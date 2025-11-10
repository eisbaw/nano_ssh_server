/*
 * test_mac.c - Compare MAC computation with BASH implementation
 *
 * This program computes HMAC-SHA-256 MAC using the same test vectors
 * as the BASH implementation, allowing us to compare results byte-by-byte.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>

#include "test_vector.h"

// Write uint32 in network byte order (big-endian)
static void write_uint32(uint8_t *buf, uint32_t val) {
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >> 8) & 0xFF;
    buf[3] = val & 0xFF;
}

// Compute SSH MAC: HMAC-SHA-256(sequence_number || packet)
static void compute_ssh_mac(
    uint32_t seq_num,
    const uint8_t *packet,
    size_t packet_len,
    const uint8_t *mac_key,
    size_t mac_key_len,
    uint8_t *mac_out
) {
    uint8_t mac_input[4 + packet_len];

    // Build MAC input: sequence_number || packet
    write_uint32(mac_input, seq_num);
    memcpy(mac_input + 4, packet, packet_len);

    // Compute HMAC-SHA-256
    unsigned int mac_len = 0;
    HMAC(EVP_sha256(),
         mac_key, mac_key_len,
         mac_input, sizeof(mac_input),
         mac_out, &mac_len);

    if (mac_len != 32) {
        fprintf(stderr, "ERROR: MAC length is %u, expected 32\n", mac_len);
    }
}

// Print hex bytes
static void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("%s", label);
    for (size_t i = 0; i < len; i++) {
        if (i > 0 && i % 16 == 0) {
            printf("\n%*s", (int)strlen(label), "");
        }
        printf(" %02x", data[i]);
    }
    printf("\n");
}

int main(void) {
    printf("=== MAC Computation Test - C Reference ===\n\n");

    // Test Vector 1
    printf("Test Vector 1: SERVICE_REQUEST\n");
    printf("-------------------------------\n\n");

    printf("Input:\n");
    printf("  Sequence: %u (0x%08x)\n", seq_num_1, seq_num_1);
    print_hex("  Packet:  ", packet_1, packet_1_len);
    print_hex("  MAC Key: ", mac_key_1, mac_key_1_len);
    printf("\n");

    // Compute MAC
    uint8_t computed_mac[32];
    compute_ssh_mac(seq_num_1, packet_1, packet_1_len,
                    mac_key_1, mac_key_1_len, computed_mac);

    printf("Results:\n");
    print_hex("  Computed:", computed_mac, 32);
    print_hex("  Expected:", expected_mac_1, expected_mac_1_len);
    printf("\n");

    // Compare
    if (memcmp(computed_mac, expected_mac_1, 32) == 0) {
        printf("  ✅ MATCH! C implementation matches expected MAC\n");
        return 0;
    } else {
        printf("  ❌ MISMATCH - C implementation differs from expected\n\n");

        // Show differences
        printf("Byte-by-byte comparison:\n");
        printf("  Offset  Computed  Expected  Diff\n");
        for (int i = 0; i < 32; i++) {
            if (computed_mac[i] != expected_mac_1[i]) {
                printf("  [%2d]    %02x        %02x        %s\n",
                       i, computed_mac[i], expected_mac_1[i], "DIFF");
            } else {
                printf("  [%2d]    %02x        %02x        ok\n",
                       i, computed_mac[i], expected_mac_1[i]);
            }
        }
        return 1;
    }
}
