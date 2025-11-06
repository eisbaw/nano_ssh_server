/*
 * Debug modular exponentiation
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "bignum_simple.h"

void print_bn(const char *name, const bn_t *x) {
    uint8_t bytes[32];
    bn_to_bytes(x, bytes, 32);
    printf("%s (first 32 bytes): ", name);
    for (int i = 0; i < 32; i++) {
        printf("%02x", bytes[i]);
    }
    printf("\n");
}

int main() {
    bn_t base, exp, mod, result;

    /* Test: 5^3 mod 13 = 125 mod 13 = 8 */
    bn_zero(&base);
    base.array[0] = 5;

    bn_zero(&exp);
    exp.array[0] = 3;

    bn_zero(&mod);
    mod.array[0] = 13;

    printf("=== Simple Test: 5^3 mod 13 (expected: 8) ===\n");
    print_bn("base", &base);
    print_bn("exp", &exp);
    print_bn("mod", &mod);

    bn_modexp(&result, &base, &exp, &mod);
    print_bn("result", &result);
    printf("Result value: %u\n", result.array[0]);

    if (result.array[0] == 8) {
        printf("✓ Simple test PASSED\n\n");
    } else {
        printf("✗ Simple test FAILED (expected 8, got %u)\n\n", result.array[0]);
    }

    /* Test: 2^8 mod 17 = 256 mod 17 = 1 */
    bn_zero(&base);
    base.array[0] = 2;

    bn_zero(&exp);
    exp.array[0] = 8;

    bn_zero(&mod);
    mod.array[0] = 17;

    printf("=== Test: 2^8 mod 17 (expected: 1) ===\n");
    print_bn("base", &base);
    print_bn("exp", &exp);
    print_bn("mod", &mod);

    bn_modexp(&result, &base, &exp, &mod);
    print_bn("result", &result);
    printf("Result value: %u\n", result.array[0]);

    if (result.array[0] == 1) {
        printf("✓ Test PASSED\n\n");
    } else {
        printf("✗ Test FAILED (expected 1, got %u)\n\n", result.array[0]);
    }

    /* Test: 3^7 mod 23 = 2187 mod 23 = 3 */
    bn_zero(&base);
    base.array[0] = 3;

    bn_zero(&exp);
    exp.array[0] = 7;

    bn_zero(&mod);
    mod.array[0] = 23;

    printf("=== Test: 3^7 mod 23 (expected: 3) ===\n");
    print_bn("base", &base);
    print_bn("exp", &exp);
    print_bn("mod", &mod);

    bn_modexp(&result, &base, &exp, &mod);
    print_bn("result", &result);
    printf("Result value: %u\n", result.array[0]);

    if (result.array[0] == 3) {
        printf("✓ Test PASSED\n");
    } else {
        printf("✗ Test FAILED (expected 3, got %u)\n", result.array[0]);
    }

    return 0;
}
