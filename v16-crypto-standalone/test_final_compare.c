#include <stdio.h>
#include <stdint.h>
#include "bignum_simple.h"
#include "rsa.h"

int main() {
    rsa_key_t key;
    bn_t m, c;

    rsa_init_key(&key);

    /* Test: 42^65537 mod n */
    bn_zero(&m);
    m.array[0] = 42;

    printf("Computing 42^65537 mod n\n\n");

    bn_modexp(&c, &m, &key.e, &key.n);

    printf("Result:\n");
    printf("  c.array[0] = 0x%08x (%u)\n", c.array[0], c.array[0]);
    
    /* Print first 8 words */
    printf("\nFirst 8 words:\n");
    for (int i = 0; i < 8; i++) {
        printf("  c.array[%d] = 0x%08x\n", i, c.array[i]);
    }

    printf("\nExpected from Python:\n");
    printf("  c.array[0] = 0xf91e0d65 (4179496293)\n");
    printf("  Full hex: 0x179df294edfb9215...f91e0d65\n");

    /* Check if it's all zeros */
    int all_zero = 1;
    for (int i = 0; i < BN_WORDS; i++) {
        if (c.array[i] != 0) {
            all_zero = 0;
            break;
        }
    }

    if (all_zero) {
        printf("\n✗ Result is ALL ZEROS - BUG\n");
    } else if (c.array[0] == 0xf91e0d65) {
        printf("\n✓ CORRECT!\n");
    } else {
        printf("\n✗ Wrong result\n");
    }

    return 0;
}
