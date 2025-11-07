/*
 * Test RSA key loading and byte conversion
 * Verify that keys are loaded correctly into bignum format
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "bignum_adapter.h"
#include "rsa.h"

static void print_bn(const char *name, const bn_t *n) {
    printf("%s (first 8 words, little-endian):\n", name);
    for (int i = 0; i < 8; i++) {
        printf("  array[%d] = 0x%08x\n", i, n->array[i]);
    }
    printf("\n");
}

int main() {
    rsa_key_t key;

    printf("Testing RSA key loading and byte conversion\n\n");

    /* Initialize RSA key */
    rsa_init_key(&key);

    /* Print loaded values */
    print_bn("Modulus (n)", &key.n);
    print_bn("Private exponent (d)", &key.d);
    printf("Public exponent (e):\n");
    printf("  array[0] = 0x%08x (%u)\n\n", key.e.array[0], key.e.array[0]);

    /* Expected values from rsa.h (last 4 bytes of modulus in big-endian) */
    printf("Expected modulus (last 4 bytes, big-endian from rsa.h):\n");
    printf("  0x4a, 0xe4, 0x86, 0xbf\n\n");

    /* In little-endian word order, these should be in array[0] */
    printf("Expected modulus array[0] (bytes reversed in word):\n");
    printf("  0xbf86e44a\n\n");

    printf("Actual modulus array[0]:\n");
    printf("  0x%08x\n\n", key.n.array[0]);

    if (key.n.array[0] == 0xbf86e44a) {
        printf("✓ Modulus loaded correctly!\n\n");
    } else {
        printf("✗ Modulus loading WRONG - byte conversion bug!\n\n");
        printf("Let's check the first word from rsa_modulus array:\n");
        printf("rsa_modulus[252-255] (big-endian): 0x%02x %02x %02x %02x\n",
               rsa_modulus[252], rsa_modulus[253], rsa_modulus[254], rsa_modulus[255]);
    }

    /* Test simple conversion */
    printf("\nTesting simple byte conversion:\n");
    uint8_t test_bytes[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    bn_t test_bn;

    printf("Input bytes (big-endian): ");
    for (int i = 0; i < 8; i++) {
        printf("%02x ", test_bytes[i]);
    }
    printf("\n");

    bn_from_bytes(&test_bn, test_bytes, 8);

    printf("Output bn.array[0]: 0x%08x\n", test_bn.array[0]);
    printf("Output bn.array[1]: 0x%08x\n", test_bn.array[1]);

    /* Expected: bytes are big-endian [01 02 03 04 05 06 07 08]
     * In little-endian word order, LSW comes first
     * array[0] should contain bytes [7, 6, 5, 4] = 0x08070605
     * array[1] should contain bytes [3, 2, 1, 0] = 0x04030201
     */
    printf("\nExpected (for big-endian input 0x0102030405060708):\n");
    printf("  array[0] = 0x08070605 (LSW - rightmost 4 bytes)\n");
    printf("  array[1] = 0x04030201 (next 4 bytes)\n");

    return 0;
}
