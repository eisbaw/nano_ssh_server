#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "bignum_tiny.h"

void print_bn(const char *label, const bn_t *a) {
    printf("%s: ", label);
    int started = 0;
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (a->array[i] != 0 || started || i == 0) {
            if (started) printf("%08x", a->array[i]);
            else printf("%x", a->array[i]);
            started = 1;
        }
    }
    printf("\n");
}

int main() {
    bn_t a, b, result;

    printf("=== Testing Basic Operations ===\n\n");

    /* Test 1: Simple multiplication */
    printf("Test 1: 123 * 456\n");
    bn_zero(&a);
    a.array[0] = 123;
    bn_zero(&b);
    b.array[0] = 456;

    bn_mul(&result, &a, &b);
    printf("Result: %u (expected %u)\n", result.array[0], 123 * 456);
    printf("Match: %s\n\n", result.array[0] == 123 * 456 ? "YES" : "NO");

    /* Test 2: Larger multiplication */
    printf("Test 2: 1000000 * 1000000\n");
    bn_zero(&a);
    a.array[0] = 1000000;
    bn_zero(&b);
    b.array[0] = 1000000;

    bn_mul(&result, &a, &b);
    uint64_t expected = 1000000ULL * 1000000ULL;
    printf("Result: %u %u (as 64-bit)\n", result.array[1], result.array[0]);
    uint64_t got = ((uint64_t)result.array[1] << 32) | result.array[0];
    printf("Expected: %llu, Got: %llu\n", expected, got);
    printf("Match: %s\n\n", got == expected ? "YES" : "NO");

    /* Test 3: Modular reduction with repeated subtraction */
    printf("Test 3: 1024 mod 100 (repeated subtraction)\n");
    bn_zero(&a);
    a.array[0] = 1024;
    bn_zero(&b);
    b.array[0] = 100;

    bn_mod(&result, &a, &b);
    printf("Result: %u (expected 24)\n", result.array[0]);
    printf("Match: %s\n\n", result.array[0] == 24 ? "YES" : "NO");

    /* Test 4: Check if comparison works for large numbers */
    printf("Test 4: Comparison of large equal numbers\n");
    bn_zero(&a);
    a.array[63] = 0x12345678;
    a.array[0] = 0xABCDEF00;

    bn_zero(&b);
    b.array[63] = 0x12345678;
    b.array[0] = 0xABCDEF00;

    int cmp = bn_cmp(&a, &b);
    printf("Compare equal: %d (expected 0)\n", cmp);
    printf("Match: %s\n\n", cmp == 0 ? "YES" : "NO");

    /* Test 5: Check if comparison works when a > b */
    printf("Test 5: Comparison where a > b\n");
    b.array[0] = 0xABCDEE00;  /* Make b slightly smaller */

    cmp = bn_cmp(&a, &b);
    printf("Compare a>b: %d (expected 1)\n", cmp);
    printf("Match: %s\n\n", cmp == 1 ? "YES" : "NO");

    /* Test 6: Squaring */
    printf("Test 6: 2^32 squared\n");
    bn_zero(&a);
    a.array[1] = 1;  /* This is 2^32 */

    bn_mul(&result, &a, &a);
    print_bn("2^32 squared", &result);
    printf("Should be 2^64 = array[2]=1\n");
    printf("Match: %s\n\n", result.array[2] == 1 && result.array[1] == 0 && result.array[0] == 0 ? "YES" : "NO");

    return 0;
}
