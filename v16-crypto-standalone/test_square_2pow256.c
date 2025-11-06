/*
 * Isolate the exact case that's failing: squaring 2^256
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "bignum_tiny.h"

int main() {
    bn_t temp_base, temp;

    printf("========================================\n");
    printf("Test: Square 2^256\n");
    printf("========================================\n\n");

    /* Set temp_base = 2^256 (word[8] = 1) */
    bn_zero(&temp_base);
    temp_base.array[8] = 1;

    printf("Before squaring:\n");
    printf("  temp_base.array[8] = %u\n", temp_base.array[8]);
    printf("  temp_base.array[7] = %u\n", temp_base.array[7]);
    printf("  temp_base.array[9] = %u\n", temp_base.array[9]);

    /* Verify temp_base is not zero */
    int is_zero_before = bn_is_zero(&temp_base);
    printf("  bn_is_zero(&temp_base) = %d\n", is_zero_before);
    assert(is_zero_before == 0 && "temp_base should not be zero");

    printf("\nSquaring: bn_mul(&temp, &temp_base, &temp_base)...\n");

    /* Square it: 2^256 * 2^256 = 2^512 */
    bn_mul(&temp, &temp_base, &temp_base);

    printf("\nAfter bn_mul:\n");

    /* Check every word manually */
    printf("  Checking temp word by word:\n");
    int nonzero_count = 0;
    for (int i = 0; i < BN_WORDS; i++) {
        if (temp.array[i] != 0) {
            printf("    temp.array[%d] = 0x%08x\n", i, temp.array[i]);
            nonzero_count++;
        }
    }

    printf("  Total non-zero words: %d\n", nonzero_count);

    /* Check specifically word[16] (where 2^512 should be) */
    printf("  temp.array[16] = 0x%08x (expected: 0x00000001)\n", temp.array[16]);
    printf("  temp.array[15] = 0x%08x (expected: 0x00000000)\n", temp.array[15]);
    printf("  temp.array[17] = 0x%08x (expected: 0x00000000)\n", temp.array[17]);

    /* Now call bn_is_zero */
    printf("\nCalling bn_is_zero(&temp)...\n");
    int is_zero_after = bn_is_zero(&temp);
    printf("  bn_is_zero(&temp) = %d (expected: 0)\n", is_zero_after);

    if (is_zero_after == 1) {
        printf("\n❌ ERROR: bn_is_zero returned 1 (zero) but we found %d non-zero words!\n", nonzero_count);
        printf("This is impossible - investigating further...\n");

        /* Manual check inline */
        printf("\nManual bn_is_zero implementation:\n");
        int manual_is_zero = 1;
        for (int i = 0; i < BN_WORDS; i++) {
            if (temp.array[i] != 0) {
                printf("  Found non-zero at word[%d] = 0x%08x\n", i, temp.array[i]);
                manual_is_zero = 0;
                break;
            }
        }
        printf("  Manual result: %d\n", manual_is_zero);

        return 1;
    }

    /* Verify expected result */
    if (temp.array[16] == 1 && temp.array[15] == 0 && temp.array[17] == 0) {
        printf("\n✅ PASS: 2^256 squared = 2^512 (word[16] = 1)\n");
        return 0;
    } else {
        printf("\n❌ FAIL: Unexpected result\n");
        return 1;
    }
}
