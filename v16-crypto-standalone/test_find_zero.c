#include <stdio.h>
#include <stdint.h>
#include "bignum_simple.h"
#include "rsa.h"

int is_zero(const bn_t *x) {
    for (int i = 0; i < BN_WORDS; i++) {
        if (x->array[i] != 0) return 0;
    }
    return 1;
}

int main() {
    rsa_key_t key;
    bn_t base_copy, temp;

    rsa_init_key(&key);

    bn_zero(&base_copy);
    base_copy.array[0] = 42;

    /* Iterate and find when it becomes zero */
    for (int i = 0; i <= 20; i++) {
        bn_mulmod(&temp, &base_copy, &base_copy, &key.n);
        base_copy = temp;

        printf("After iteration %d: ", i);
        if (is_zero(&base_copy)) {
            printf("ALL ZEROS ← FIRST ZERO!\n");
            
            /* Show previous iteration */
            printf("\nRewinding to check iteration %d...\n", i-1);
            bn_zero(&base_copy);
            base_copy.array[0] = 42;
            for (int j = 0; j < i; j++) {
                bn_mulmod(&temp, &base_copy, &base_copy, &key.n);
                base_copy = temp;
            }
            
            printf("Before iteration %d:\n", i);
            for (int k = 0; k < 8; k++) {
                printf("  base_copy.array[%d] = 0x%08x\n", k, base_copy.array[k]);
            }
            
            break;
        } else {
            printf("base_copy.array[0] = 0x%08x\n", base_copy.array[0]);
        }
    }

    return 0;
}
