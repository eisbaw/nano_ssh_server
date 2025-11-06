#include <stdio.h>
#include <stdint.h>
#include "bignum_simple.h"
#include "rsa.h"

int main() {
    rsa_key_t key;
    bn_t base_copy, temp;

    rsa_init_key(&key);

    bn_zero(&base_copy);
    base_copy.array[0] = 42;

    /* Iterate to iteration 7 */
    for (int i = 0; i < 7; i++) {
        bn_mulmod(&temp, &base_copy, &base_copy, &key.n);
        base_copy = temp;
    }

    printf("After iteration 6 (before iteration 7):\n");
    int found_nonzero = 0;
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (base_copy.array[i] != 0) {
            printf("  base_copy.array[%d] = 0x%08x\n", i, base_copy.array[i]);
            found_nonzero = 1;
        }
    }
    if (!found_nonzero) {
        printf("  ALL ZEROS!\n");
    }

    /* Do iteration 7 */
    printf("\nDoing iteration 7 (squaring)...\n");
    bn_2x_t product;
    bn_mul_wide(&product, &base_copy, &base_copy);
    
    printf("After bn_mul_wide (before bn_mod_simple):\n");
    found_nonzero = 0;
    for (int i = BN_2X_WORDS - 1; i >= 0; i--) {
        if (product.array[i] != 0) {
            printf("  product.array[%d] = 0x%08x\n", i, product.array[i]);
            found_nonzero = 1;
        }
    }
    if (!found_nonzero) {
        printf("  ALL ZEROS!\n");
    }

    bn_mod_simple(&base_copy, &product, &key.n);
    
    printf("\nAfter bn_mod_simple (after iteration 7):\n");
    found_nonzero = 0;
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (base_copy.array[i] != 0) {
            printf("  base_copy.array[%d] = 0x%08x\n", i, base_copy.array[i]);
            found_nonzero = 1;
        }
    }
    if (!found_nonzero) {
        printf("  ALL ZEROS!\n");
    }

    return 0;
}
