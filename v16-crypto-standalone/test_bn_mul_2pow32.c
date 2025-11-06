#include <stdio.h>
#include <string.h>
#include "bignum_tiny.h"

int main() {
    bn_t a, result;

    /* Test: 2^32 × 2^32 = 2^64 */
    printf("Test: 2^32 × 2^32 = 2^64\n\n");

    bn_zero(&a);
    a.array[0] = 0;
    a.array[1] = 1;  /* 2^32 */

    printf("Input:\n");
    printf("  a.array[0] = %u\n", a.array[0]);
    printf("  a.array[1] = %u\n", a.array[1]);
    printf("  a.array[2] = %u\n", a.array[2]);

    bn_mul(&result, &a, &a);

    printf("\nAfter bn_mul(&result, &a, &a):\n");
    printf("  result.array[0] = %u (expected: 0)\n", result.array[0]);
    printf("  result.array[1] = %u (expected: 0)\n", result.array[1]);
    printf("  result.array[2] = %u (expected: 1)\n", result.array[2]);
    printf("  result.array[3] = %u (expected: 0)\n", result.array[3]);

    /* 2^64 should be: word[2] = 1, all others = 0 */
    if (result.array[0] == 0 && result.array[1] == 0 &&
        result.array[2] == 1 && result.array[3] == 0) {
        printf("\n✅ CORRECT: 2^32 × 2^32 = 2^64\n");
        return 0;
    }

    printf("\n❌ WRONG!\n");

    if (bn_is_zero(&result)) {
        printf("Result is completely ZERO!\n");
    } else {
        printf("Result has unexpected values:\n");
        for (int i = 0; i < 10; i++) {
            if (result.array[i] != 0) {
                printf("  word[%d] = %u\n", i, result.array[i]);
            }
        }
    }

    return 1;
}
