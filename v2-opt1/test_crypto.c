/*
 * Crypto test - verify libsodium functions work correctly
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sodium.h>

void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

int test_sha256() {
    uint8_t hash[32];
    const char *msg = "abc";

    /* Expected SHA-256("abc") */
    uint8_t expected[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };

    crypto_hash_sha256(hash, (const uint8_t *)msg, strlen(msg));

    print_hex("Computed", hash, 32);
    print_hex("Expected", expected, 32);

    if (memcmp(hash, expected, 32) == 0) {
        printf("✓ SHA-256 test PASSED\n");
        return 0;
    } else {
        printf("✗ SHA-256 test FAILED\n");
        return 1;
    }
}

int test_curve25519() {
    uint8_t private1[32], public1[32];
    uint8_t private2[32], public2[32];
    uint8_t shared1[32], shared2[32];

    printf("\n=== Testing Curve25519 ===\n");

    /* Generate two key pairs */
    randombytes_buf(private1, 32);
    crypto_scalarmult_base(public1, private1);

    randombytes_buf(private2, 32);
    crypto_scalarmult_base(public2, private2);

    /* Compute shared secrets (should be equal) */
    crypto_scalarmult(shared1, private1, public2);
    crypto_scalarmult(shared2, private2, public1);

    print_hex("Shared 1", shared1, 32);
    print_hex("Shared 2", shared2, 32);

    if (memcmp(shared1, shared2, 32) == 0) {
        printf("✓ Curve25519 DH test PASSED\n");
        return 0;
    } else {
        printf("✗ Curve25519 DH test FAILED\n");
        return 1;
    }
}

int test_ed25519() {
    uint8_t public_key[32], private_key[64];
    uint8_t signature[64];
    unsigned long long siglen;
    const char *msg = "test message";

    printf("\n=== Testing Ed25519 ===\n");

    /* Generate key pair */
    crypto_sign_keypair(public_key, private_key);

    /* Sign message */
    crypto_sign_detached(signature, &siglen, (const uint8_t *)msg, strlen(msg), private_key);

    print_hex("Signature", signature, 64);

    /* Verify signature */
    int result = crypto_sign_verify_detached(signature, (const uint8_t *)msg, strlen(msg), public_key);

    if (result == 0) {
        printf("✓ Ed25519 sign/verify test PASSED\n");
        return 0;
    } else {
        printf("✗ Ed25519 sign/verify test FAILED\n");
        return 1;
    }
}

int main() {
    int failures = 0;

    if (sodium_init() < 0) {
        fprintf(stderr, "Failed to initialize libsodium\n");
        return 1;
    }

    printf("=== Crypto Library Tests ===\n\n");
    printf("=== Testing SHA-256 ===\n");
    failures += test_sha256();
    failures += test_curve25519();
    failures += test_ed25519();

    printf("\n=== Summary ===\n");
    if (failures == 0) {
        printf("✓ All tests PASSED\n");
        return 0;
    } else {
        printf("✗ %d test(s) FAILED\n", failures);
        return 1;
    }
}
