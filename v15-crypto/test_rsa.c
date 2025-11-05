/*
 * Test program for RSA implementation
 */

#include <stdio.h>
#include <string.h>
#include "rsa.h"

void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < len && i < 32; i++) {
        printf("%02x", data[i]);
    }
    if (len > 32) printf("...");
    printf("\n");
}

int test_rsa_sign_verify() {
    printf("\n=== Test: RSA Sign and Verify ===\n");

    rsa_key_t key;
    uint8_t message[32];
    uint8_t signature[256];

    /* Initialize key */
    rsa_init_key(&key);
    printf("RSA key initialized\n");

    /* Create test message (SHA-256 hash) */
    memset(message, 0xAB, 32);
    print_hex("Message (hash)", message, 32);

    /* Sign */
    if (rsa_sign(signature, message, &key) < 0) {
        printf("❌ FAIL: RSA signing failed\n");
        return 0;
    }
    printf("✓ RSA signature created\n");
    print_hex("Signature", signature, 256);

    /* Verify */
    if (rsa_verify(signature, message, &key) == 0) {
        printf("✓ PASS: RSA verification successful\n");
        return 1;
    } else {
        printf("❌ FAIL: RSA verification failed\n");
        return 0;
    }
}

int test_rsa_public_key_export() {
    printf("\n=== Test: RSA Public Key Export ===\n");

    rsa_key_t key;
    uint8_t public_key[512];
    size_t public_key_len;

    /* Initialize key */
    rsa_init_key(&key);

    /* Export public key */
    if (rsa_export_public_key_ssh(public_key, &public_key_len, &key) < 0) {
        printf("❌ FAIL: Public key export failed\n");
        return 0;
    }

    printf("✓ Public key exported\n");
    printf("Public key length: %zu bytes\n", public_key_len);
    print_hex("Public key", public_key, public_key_len);

    /* Verify format starts with "ssh-rsa" */
    if (public_key_len > 11 &&
        memcmp(public_key + 4, "ssh-rsa", 7) == 0) {
        printf("✓ PASS: Public key format correct\n");
        return 1;
    } else {
        printf("❌ FAIL: Public key format incorrect\n");
        return 0;
    }
}

int test_rsa_wrong_message() {
    printf("\n=== Test: RSA Verify Wrong Message ===\n");

    rsa_key_t key;
    uint8_t message1[32];
    uint8_t message2[32];
    uint8_t signature[256];

    /* Initialize key */
    rsa_init_key(&key);

    /* Create two different messages */
    memset(message1, 0xAA, 32);
    memset(message2, 0xBB, 32);

    /* Sign message1 */
    rsa_sign(signature, message1, &key);

    /* Try to verify with message2 (should fail) */
    if (rsa_verify(signature, message2, &key) != 0) {
        printf("✓ PASS: Verification correctly rejected wrong message\n");
        return 1;
    } else {
        printf("❌ FAIL: Verification accepted wrong message!\n");
        return 0;
    }
}

int main() {
    int passed = 0;
    int total = 3;

    printf("RSA-2048 Implementation Tests\n");
    printf("==============================\n");

    passed += test_rsa_sign_verify();
    passed += test_rsa_public_key_export();
    passed += test_rsa_wrong_message();

    printf("\n==============================\n");
    printf("Tests passed: %d/%d\n", passed, total);
    printf("==============================\n");

    return (passed == total) ? 0 : 1;
}
