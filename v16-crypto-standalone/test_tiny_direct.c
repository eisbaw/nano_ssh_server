/*
 * Test tiny-bignum-c directly without adapter layer
 * Verify the library itself works correctly
 */
#include <stdio.h>
#include <stdint.h>
#include "bn_tinybignum.h"

int main() {
    struct bn base, exp, mod, result;

    printf("Testing tiny-bignum-c directly (no adapter)\n\n");

    /* Test simple case: 42^3 mod 100 = 88 */
    bignum_init(&base);
    bignum_init(&exp);
    bignum_init(&mod);
    bignum_init(&result);

    bignum_from_int(&base, 42);
    bignum_from_int(&exp, 3);
    bignum_from_int(&mod, 100);

    printf("Test 1: 42^3 mod 100\n");
    printf("Expected: 88\n");

    /* Compute: result = base^exp mod mod */
    /* tiny-bignum-c has bignum_pow but it doesn't do modular exponentiation */
    /* We need to implement it manually */

    struct bn temp1, temp2;
    bignum_assign(&result, &base);  /* result = base */
    bignum_mul(&result, &base, &temp1);  /* temp1 = base^2 */
    bignum_mul(&temp1, &base, &temp2);   /* temp2 = base^3 */
    bignum_mod(&temp2, &mod, &result);   /* result = base^3 mod 100 */

    int r = bignum_to_int(&result);
    printf("Result: %d\n", r);

    if (r == 88) {
        printf("✓ Basic operations work!\n\n");
    } else {
        printf("✗ Basic operations BROKEN!\n\n");
        return 1;
    }

    /* Test 2: Check if library handles larger numbers correctly */
    bignum_init(&base);
    bignum_init(&mod);
    bignum_from_int(&base, 1234567);
    bignum_from_int(&mod, 1000000);

    printf("Test 2: 1234567 mod 1000000\n");
    printf("Expected: 234567\n");

    bignum_mod(&base, &mod, &result);
    r = bignum_to_int(&result);
    printf("Result: %d\n", r);

    if (r == 234567) {
        printf("✓ Modular arithmetic works!\n\n");
    } else {
        printf("✗ Modular arithmetic BROKEN!\n\n");
        return 1;
    }

    printf("tiny-bignum-c basic functions work correctly.\n");
    printf("Issue must be in adapter layer or byte conversion.\n");

    return 0;
}
