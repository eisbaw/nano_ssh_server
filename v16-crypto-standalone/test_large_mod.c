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
    bn_t base, temp;

    rsa_init_key(&key);

    /* Start with 42 and square it repeatedly */
    bn_zero(&base);
    base.array[0] = 42;

    printf("Squaring 42 repeatedly mod n:\n\n");

    for (int i = 0; i <= 10; i++) {
        printf("Iteration %d: base.array[0]=0x%08x", i, base.array[0]);
        
        if (is_zero(&base)) {
            printf(" ✗ ZERO!\n");
            break;
        } else {
            printf(" ✓\n");
        }

        /* Square */
        bn_mulmod(&temp, &base, &base, &key.n);
        base = temp;
    }

    return 0;
}
