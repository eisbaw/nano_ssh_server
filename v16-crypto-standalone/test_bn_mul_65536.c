#include <stdio.h>
#include <string.h>
#include "bignum_tiny.h"

int main() {
    bn_t a, result;

    /* Test: 65536 × 65536 = 4,294,967,296 = 2^32 */
    printf("Test: 65536 × 65536\n\n");

    bn_zero(&a);
    a.array[0] = 65536;

    printf("Input: a.array[0] = %u\n", a.array[0]);
    printf("Input: a.array[1] = %u\n", a.array[1]);

    bn_mul(&result, &a, &a);

    printf("\nAfter bn_mul(&result, &a, &a):\n");
    printf("result.array[0] = %u (expected: 0)\n", result.array[0]);
    printf("result.array[1] = %u (expected: 1)\n", result.array[1]);
    printf("result.array[2] = %u (expected: 0)\n", result.array[2]);

    /* Check if correct */
    if (result.array[0] == 0 && result.array[1] == 1 && result.array[2] == 0) {
        printf("\n✅ CORRECT: 65536² = 2^32\n");
        return 0;
    } else {
        printf("\n❌ WRONG!\n");

        /* Check if result is completely zero */
        int all_zero = 1;
        for (int i = 0; i < BN_WORDS; i++) {
            if (result.array[i] != 0) {
                all_zero = 0;
                break;
            }
        }

        if (all_zero) {
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
}
