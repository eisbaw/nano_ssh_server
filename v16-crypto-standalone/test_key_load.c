/*
 * Test if RSA key loading is correct
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "rsa.h"

void print_bn(const char *label, const bn_t *bn) {
    printf("%s (first 32 bytes): ", label);
    for (int i = 0; i < 8 && i < BN_WORDS; i++) {
        printf("%08x ", bn->array[i]);
    }
    printf("\n");
}

int main() {
    rsa_key_t key;
    bn_t test_result, expected_n;

    /* Initialize key */
    printf("=== Testing RSA Key Loading ===\n\n");
    rsa_init_key(&key);

    printf("Loaded modulus n:\n");
    print_bn("  n", &key.n);

    printf("\nLoaded private exponent d:\n");
    print_bn("  d", &key.d);

    printf("\nPublic exponent e:\n");
    print_bn("  e", &key.e);

    printf("\nExpected e = 65537 (0x00010001)\n");
    printf("Actual   e = %u\n", key.e.array[0]);

    if (key.e.array[0] == 65537) {
        printf("✓ Public exponent is correct\n");
    } else {
        printf("✗ Public exponent is WRONG\n");
    }

    /* Check if n is correctly loaded */
    printf("\nChecking modulus bytes:\n");
    printf("Expected first bytes: a7 3e 9d 97 8a eb a1 12\n");
    uint8_t n_bytes[256];
    bn_to_bytes(&key.n, n_bytes, 256);
    printf("Actual first bytes:   ");
    for (int i = 0; i < 8; i++) {
        printf("%02x ", n_bytes[i]);
    }
    printf("\n");

    if (n_bytes[0] == 0xa7 && n_bytes[1] == 0x3e && n_bytes[2] == 0x9d && n_bytes[3] == 0x97) {
        printf("✓ Modulus bytes match\n");
    } else {
        printf("✗ Modulus bytes DON'T match\n");
    }

    return 0;
}
