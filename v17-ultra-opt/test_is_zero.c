#include <stdio.h>
#include <stdint.h>
#include "bignum_tiny.h"
#include "diffie_hellman.h"

void print_all_words(const char *label, const bn_t *a) {
    printf("%s:\n", label);
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        printf("  [%2d] = %08x", i, a->array[i]);
        if (i % 4 == 0) printf("\n");
    }
}

int main() {
    bn_t a, b, prime;
    bn_double_t product;
    extern const uint8_t dh_group14_prime[256];

    printf("=== Testing bn_mul_full ===\n\n");

    /* Load prime */
    bn_from_bytes(&prime, dh_group14_prime, 256);

    /* Test 1: 2^64 * 2^64 = 2^128 */
    printf("Test 1: (2^64) * (2^64)\n");
    bn_zero(&a);
    a.array[2] = 1;  /* a = 2^64 */
    bn_zero(&b);
    b.array[2] = 1;  /* b = 2^64 */

    printf("Input a:\n");
    print_all_words("", &a);
    printf("Input b:\n");
    print_all_words("", &b);

    bn_mul_full(&product, &a, &b);

    printf("\nProduct (expected: bit at position 128 = word[4]):\n");
    for (int i = BN_WORDS * 2 - 1; i >= 0; i--) {
        if (product.array[i] != 0) {
            printf("  [%3d] = %08x (non-zero)\n", i, product.array[i]);
        }
    }

    /* Check word[4] specifically */
    printf("\nWord[4] (should be 1 for 2^128): %08x\n", product.array[4]);

    if (product.array[4] == 1) {
        printf("✓ PASS\n");
    } else {
        printf("✗ FAIL\n");
    }

    return 0;
}
