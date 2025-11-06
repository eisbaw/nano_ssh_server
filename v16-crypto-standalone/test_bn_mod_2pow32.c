#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "bignum_tiny.h"
#include "diffie_hellman.h"

int main() {
    bn_t a, prime, result;

    /* Test: 2^32 mod prime */
    printf("Test: 2^32 mod DH_GROUP14_PRIME\n\n");

    bn_zero(&a);
    a.array[0] = 0;
    a.array[1] = 1;  /* 2^32 */

    printf("Input a:\n");
    printf("  a.array[0] = %u\n", a.array[0]);
    printf("  a.array[1] = %u\n", a.array[1]);
    printf("  a.array[2] = %u\n", a.array[2]);

    bn_from_bytes(&prime, dh_group14_prime, 256);

    printf("\nPrime (first few words):\n");
    printf("  prime.array[0] = 0x%08x\n", prime.array[0]);
    printf("  prime.array[1] = 0x%08x\n", prime.array[1]);

    printf("\nCalling bn_mod(&result, &a, &prime)...\n");
    bn_mod(&result, &a, &prime);

    printf("\nResult:\n");
    printf("  result.array[0] = %u (expected: 0)\n", result.array[0]);
    printf("  result.array[1] = %u (expected: 1)\n", result.array[1]);
    printf("  result.array[2] = %u (expected: 0)\n", result.array[2]);

    /* Check if zero */
    if (bn_is_zero(&result)) {
        printf("\n❌ Result is ZERO! (BUG CONFIRMED)\n");
        return 1;
    }

    /* Check if correct */
    if (result.array[0] == 0 && result.array[1] == 1 && result.array[2] == 0) {
        printf("\n✅ CORRECT: 2^32 mod prime = 2^32\n");
        return 0;
    }

    printf("\n❌ Result is WRONG!\n");
    printf("First 10 words:\n");
    for (int i = 0; i < 10; i++) {
        if (result.array[i] != 0) {
            printf("  word[%d] = %u\n", i, result.array[i]);
        }
    }

    return 1;
}
