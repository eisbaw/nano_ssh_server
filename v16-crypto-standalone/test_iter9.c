#include <stdio.h>
#include <stdint.h>
#include "bignum_simple.h"
#include "rsa.h"

int main() {
    rsa_key_t key;
    bn_t base, temp;

    rsa_init_key(&key);

    /* Start with 42 and square 8 times to get to iteration 8 */
    bn_zero(&base);
    base.array[0] = 42;

    for (int i = 0; i < 8; i++) {
        bn_mulmod(&temp, &base, &base, &key.n);
        base = temp;
    }

    printf("After iteration 8:\n");
    printf("base (first 16 words):\n");
    for (int i = 0; i < 16; i++) {
        if (base.array[i] != 0) {
            printf("  [%d] = 0x%08x\n", i, base.array[i]);
        }
    }

    printf("\nNow squaring (iteration 8->9)...\n");

    bn_2x_t product;
    bn_mul_wide(&product, &base, &base);

    printf("\nAfter bn_mul_wide (product):\n");
    int found = 0;
    for (int i = 0; i < 20 && i < BN_2X_WORDS; i++) {
        if (product.array[i] != 0 || found) {
            printf("  [%d] = 0x%08x\n", i, product.array[i]);
            found = 1;
            if (found && i > 10 && product.array[i+1] == 0) break;
        }
    }

    bn_mod_simple(&temp, &product, &key.n);

    printf("\nAfter bn_mod_simple (result):\n");
    found = 0;
    for (int i = 0; i < 16; i++) {
        if (temp.array[i] != 0 || found) {
            printf("  [%d] = 0x%08x\n", i, temp.array[i]);
            found = 1;
            if (found && i > 5 && temp.array[i+1] == 0) break;
        }
    }

    printf("\nExpected from Python: temp.array[0] = 0x0ee1c7e7\n");
    printf("Got from C: temp.array[0] = 0x%08x\n", temp.array[0]);

    if (temp.array[0] == 0x0ee1c7e7) {
        printf("✓ CORRECT\n");
    } else {
        printf("✗ WRONG\n");
    }

    return 0;
}
