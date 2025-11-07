#include <stdio.h>
#include <stdint.h>
#include "bignum_tiny.h"
#include "diffie_hellman.h"

int main() {
    bn_t a, b, result, prime;

    printf("Testing bn_mod iteration safety limit...\n\n");

    bn_from_bytes(&prime, dh_group14_prime, 256);

    /* Test: 2 * prime mod prime */
    printf("Test: 2 * prime mod prime\n");
    a = prime;
    bn_add(&a, &a, &prime);  /* a = 2 * prime */

    /* Manually inline bn_mod with iteration counting */
    result = a;
    int iterations = 0;
    while (bn_cmp(&result, &prime) >= 0) {
        bn_sub(&result, &result, &prime);
        iterations++;
        if (iterations > BN_WORDS * 32) {
            printf("ERROR: Hit iteration limit at %d iterations!\n", iterations);
            printf("This means bn_mod is returning zero incorrectly.\n");
            return 1;
        }
    }

    printf("Iterations: %d\n", iterations);
    printf("Result is zero: %s (should be YES)\n", bn_is_zero(&result) ? "YES" : "NO");

    /* Test: 4 mod prime */
    printf("\nTest: 4 mod prime\n");
    bn_zero(&a);
    a.array[0] = 4;

    result = a;
    iterations = 0;
    while (bn_cmp(&result, &prime) >= 0) {
        bn_sub(&result, &result, &prime);
        iterations++;
        if (iterations > BN_WORDS * 32) {
            printf("ERROR: Hit iteration limit!\n");
            return 1;
        }
    }

    printf("Iterations: %d\n", iterations);
    printf("Result: %u (should be 4)\n", result.array[0]);

    /* Test: 2 * 2 = 4 */
    printf("\nTest: Multiply 2 * 2 and check result\n");
    bn_zero(&a);
    a.array[0] = 2;
    bn_zero(&b);
    b.array[0] = 2;

    bn_mul(&result, &a, &b);
    printf("2 * 2 = %u (expected 4)\n", result.array[0]);

    /* Test multiplication with mod */
    bn_mod(&result, &result, &prime);
    printf("4 mod prime = %u (expected 4)\n", result.array[0]);

    /* Test: square of large number */
    printf("\nTest: Large number squared\n");
    bn_zero(&a);
    a.array[63] = 1;  /* Set a high bit */

    printf("Input array[63] = %u\n", a.array[63]);

    bn_mul(&result, &a, &a);
    printf("Result array[63] = %u\n", result.array[63]);
    printf("Result array[62] = %u\n", result.array[62]);

    printf("\nIf result is all zeros, multiplication is broken for large numbers!\n");

    return 0;
}
