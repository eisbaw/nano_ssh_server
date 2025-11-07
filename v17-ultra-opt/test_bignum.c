/*
 * Test program for bignum library
 * Verifies basic operations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bignum.h"

void print_bn(const char *label, const bn_t *n) {
    printf("%s: ", label);
    for (int i = BN_ARRAY_SIZE - 1; i >= 0; i--) {
        if (n->array[i] != 0 || i < 4) {
            printf("%08x", n->array[i]);
            if (i > 0) printf(" ");
        }
    }
    printf("\n");
}

int test_addition() {
    printf("\n=== Test: Addition ===\n");
    bn_t a, b, c;

    bn_zero(&a);
    bn_zero(&b);
    a.array[0] = 5;
    b.array[0] = 7;

    bn_add(&c, &a, &b);

    printf("5 + 7 = %u\n", c.array[0]);
    return (c.array[0] == 12) ? 1 : 0;
}

int test_subtraction() {
    printf("\n=== Test: Subtraction ===\n");
    bn_t a, b, c;

    bn_zero(&a);
    bn_zero(&b);
    a.array[0] = 10;
    b.array[0] = 3;

    bn_sub(&c, &a, &b);

    printf("10 - 3 = %u\n", c.array[0]);
    return (c.array[0] == 7) ? 1 : 0;
}

int test_multiplication() {
    printf("\n=== Test: Multiplication ===\n");
    bn_t a, b, c;

    bn_zero(&a);
    bn_zero(&b);
    a.array[0] = 6;
    b.array[0] = 7;

    bn_mul(&c, &a, &b);

    printf("6 * 7 = %u\n", c.array[0]);
    return (c.array[0] == 42) ? 1 : 0;
}

int test_modulo() {
    printf("\n=== Test: Modulo ===\n");
    bn_t a, m, r;

    bn_zero(&a);
    bn_zero(&m);
    a.array[0] = 17;
    m.array[0] = 5;

    bn_mod(&r, &a, &m);

    printf("17 mod 5 = %u\n", r.array[0]);
    return (r.array[0] == 2) ? 1 : 0;
}

int test_modexp() {
    printf("\n=== Test: Modular Exponentiation ===\n");
    bn_t base, exp, mod, result;

    /* Test: 2^10 mod 1000 = 1024 mod 1000 = 24 */
    bn_zero(&base);
    bn_zero(&exp);
    bn_zero(&mod);

    base.array[0] = 2;
    exp.array[0] = 10;
    mod.array[0] = 1000;

    bn_modexp(&result, &base, &exp, &mod);

    printf("2^10 mod 1000 = %u\n", result.array[0]);
    return (result.array[0] == 24) ? 1 : 0;
}

int test_comparison() {
    printf("\n=== Test: Comparison ===\n");
    bn_t a, b;

    bn_zero(&a);
    bn_zero(&b);
    a.array[0] = 5;
    b.array[0] = 5;

    int cmp1 = bn_cmp(&a, &b);
    printf("5 == 5: %s\n", cmp1 == 0 ? "PASS" : "FAIL");

    b.array[0] = 3;
    int cmp2 = bn_cmp(&a, &b);
    printf("5 > 3: %s\n", cmp2 == 1 ? "PASS" : "FAIL");

    b.array[0] = 7;
    int cmp3 = bn_cmp(&a, &b);
    printf("5 < 7: %s\n", cmp3 == -1 ? "PASS" : "FAIL");

    return (cmp1 == 0 && cmp2 == 1 && cmp3 == -1) ? 1 : 0;
}

int test_bytes() {
    printf("\n=== Test: Byte Conversion ===\n");
    bn_t n, n2;
    uint8_t bytes[32] = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    uint8_t bytes_out[32];

    bn_from_bytes(&n, bytes, 32);
    bn_to_bytes(&n, bytes_out, 32);

    int match = (memcmp(bytes, bytes_out, 32) == 0);
    printf("Byte conversion: %s\n", match ? "PASS" : "FAIL");

    return match ? 1 : 0;
}

int main() {
    int passed = 0;
    int total = 7;

    printf("Bignum Library Tests\n");
    printf("====================\n");

    passed += test_addition();
    passed += test_subtraction();
    passed += test_multiplication();
    passed += test_modulo();
    passed += test_modexp();
    passed += test_comparison();
    passed += test_bytes();

    printf("\n====================\n");
    printf("Tests passed: %d/%d\n", passed, total);
    printf("====================\n");

    return (passed == total) ? 0 : 1;
}
