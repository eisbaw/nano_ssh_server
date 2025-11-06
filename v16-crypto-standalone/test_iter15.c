#include <stdio.h>
#include <stdint.h>
#include "bignum_simple.h"
#include "rsa.h"

int main() {
    rsa_key_t key;
    bn_t base_copy, temp;

    rsa_init_key(&key);

    /* Start from iteration 14 */
    bn_zero(&base_copy);
    base_copy.array[0] = 42;

    /* Do iterations 0-14 */
    for (int i = 0; i <= 14; i++) {
        bn_mulmod(&temp, &base_copy, &base_copy, &key.n);
        base_copy = temp;
    }

    printf("After iteration 14:\n");
    int found_nonzero = 0;
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (base_copy.array[i] != 0) {
            printf("  base_copy.array[%d] = 0x%08x\n", i, base_copy.array[i]);
            found_nonzero = 1;
            if (found_nonzero && i < BN_WORDS - 10) break;  /* Show first 10 non-zero */
        }
    }

    if (!found_nonzero) {
        printf("  ALL ZEROS!\n");
    }

    /* Now do iteration 15 */
    printf("\nDoing iteration 15 (squaring)...\n");
    bn_mulmod(&temp, &base_copy, &base_copy, &key.n);
    base_copy = temp;

    printf("\nAfter iteration 15:\n");
    found_nonzero = 0;
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (base_copy.array[i] != 0) {
            printf("  base_copy.array[%d] = 0x%08x\n", i, base_copy.array[i]);
            found_nonzero = 1;
            if (found_nonzero && i < BN_WORDS - 10) break;
        }
    }

    if (!found_nonzero) {
        printf("  ALL ZEROS! ← BUG\n");
    }

    return 0;
}
