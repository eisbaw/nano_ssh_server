#include <stdio.h>
#include <string.h>
#include "bignum_tiny.h"

int main() {
    bn_t temp_base, temp;

    printf("Exact reproduction of the bug:\n\n");

    /* Start with 2^32 (same as iteration j=4 ending state) */
    bn_zero(&temp_base);
    temp_base.array[0] = 0;
    temp_base.array[1] = 1;

    printf("temp_base before mul:\n");
    printf("  array[0] = %u\n", temp_base.array[0]);
    printf("  array[1] = %u\n", temp_base.array[1]);
    printf("  array[2] = %u\n", temp_base.array[2]);

    /* Square it: 2^32 × 2^32 = 2^64 */
    bn_mul(&temp, &temp_base, &temp_base);

    printf("\ntemp after bn_mul(&temp, &temp_base, &temp_base):\n");
    printf("  array[0] = %u (expected: 0)\n", temp.array[0]);
    printf("  array[1] = %u (expected: 0)\n", temp.array[1]);
    printf("  array[2] = %u (expected: 1)\n", temp.array[2]);
    printf("  array[3] = %u (expected: 0)\n", temp.array[3]);

    if (temp.array[0] == 0 && temp.array[1] == 0 &&
        temp.array[2] == 1 && temp.array[3] == 0) {
        printf("\n✅ CORRECT\n");
        return 0;
    }

    printf("\n❌ BUG REPRODUCED!\n");

    if (bn_is_zero(&temp)) {
        printf("temp is completely ZERO!\n");
    }

    return 1;
}
