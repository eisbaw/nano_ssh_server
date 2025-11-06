/*
 * Test RSA signature generation and verification
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "rsa.h"
#include "sha256_minimal.h"

int main() {
    rsa_key_t key;
    uint8_t message[32];
    uint8_t signature[256];

    /* Initialize key */
    rsa_init_key(&key);

    /* Create a test message (SHA-256 hash) */
    memset(message, 0xAA, 32);
    printf("Test message (32 bytes): ");
    for (int i = 0; i < 32; i++) {
        printf("%02x", message[i]);
    }
    printf("\n\n");

    /* Sign the message */
    printf("Signing message...\n");
    if (rsa_sign(signature, message, &key) != 0) {
        printf("ERROR: RSA signing failed\n");
        return 1;
    }
    printf("Signature generated successfully\n");

    printf("Signature (first 32 bytes): ");
    for (int i = 0; i < 32; i++) {
        printf("%02x", signature[i]);
    }
    printf("\n\n");

    /* Verify the signature */
    printf("Verifying signature...\n");
    if (rsa_verify(signature, message, &key) == 0) {
        printf("✓ Signature verification PASSED\n");
    } else {
        printf("✗ Signature verification FAILED\n");
        return 1;
    }

    /* Test with wrong message */
    printf("\nTesting with tampered message...\n");
    message[0] ^= 0x01;  /* Flip one bit */
    if (rsa_verify(signature, message, &key) == 0) {
        printf("✗ ERROR: Verification passed with wrong message!\n");
        return 1;
    } else {
        printf("✓ Correctly rejected tampered message\n");
    }

    printf("\n=== RSA IMPLEMENTATION TEST PASSED ===\n");
    return 0;
}
