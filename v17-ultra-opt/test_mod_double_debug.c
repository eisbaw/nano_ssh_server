#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "bignum_tiny.h"
#include "diffie_hellman.h"

/* Copy of bn_mod_double with debug output */
static void bn_mod_double_debug(bn_t *r, const bn_double_t *a, const bn_t *m);

/* External functions we need */
extern int bn_cmp_double_shifted(const bn_double_t *a, const bn_t *b, int shift_words);
extern void bn_sub_double_shifted(bn_double_t *a, const bn_t *b, int shift_words);

void print_double(const char *label, const bn_double_t *a) {
    printf("%s (high 4, low 4 words):\n  High: ", label);
    for (int i = BN_WORDS * 2 - 1; i >= BN_WORDS * 2 - 4; i--) {
        printf("%08x ", a->array[i]);
    }
    printf("\n  Low:  ");
    for (int i = 3; i >= 0; i--) {
        printf("%08x ", a->array[i]);
    }
    printf("\n");
}

int main() {
    bn_t a, b, result, prime;
    bn_double_t product;
    extern const uint8_t dh_group14_prime[256];

    printf("=== Debug bn_mod_double ===\n\n");

    /* Load prime */
    bn_from_bytes(&prime, dh_group14_prime, 256);

    /* Create a moderately large number: 2^64 */
    bn_zero(&a);
    a.array[2] = 1;  /* 2^64 */

    printf("Test: (2^64)^2 mod p\n\n");

    /* Square it: (2^64)^2 = 2^128 */
    bn_mul_full(&product, &a, &a);

    print_double("Product (2^128)", &product);

    /* Check if it's zero before reduction */
    int all_zero = 1;
    for (int i = 0; i < BN_WORDS * 2; i++) {
        if (product.array[i] != 0) {
            all_zero = 0;
            break;
        }
    }
    printf("Product is zero before reduction: %s\n\n", all_zero ? "YES (BUG!)" : "NO");

    /* Now reduce */
    printf("Calling bn_mod_double...\n");
    bn_mod_double(&result, &product, &prime);

    printf("\nResult after bn_mod_double:\n");
    for (int i = 63; i >= 60; i--) {
        printf("  array[%d] = %08x\n", i, result.array[i]);
    }
    for (int i = 3; i >= 0; i--) {
        printf("  array[%d] = %08x\n", i, result.array[i]);
    }

    if (bn_is_zero(&result)) {
        printf("\n✗ FAIL: Result is zero\n");
        return 1;
    } else {
        printf("\n✓ PASS: Result is non-zero\n");
        return 0;
    }
}
