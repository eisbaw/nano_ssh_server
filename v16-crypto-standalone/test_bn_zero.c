#include <stdio.h>
#include <string.h>
#include "bignum_tiny.h"

int main() {
    bn_t test;

    /* Fill with garbage first */
    memset(&test, 0xFF, sizeof(test));

    printf("Before bn_zero:\n");
    printf("  word[0] = 0x%08x\n", test.array[0]);
    printf("  word[63] = 0x%08x\n", test.array[63]);

    /* Call bn_zero */
    bn_zero(&test);

    printf("After bn_zero:\n");
    int all_zero = 1;
    for (int i = 0; i < BN_WORDS; i++) {
        if (test.array[i] != 0) {
            printf("  ❌ word[%d] = 0x%08x (NOT ZERO!)\n", i, test.array[i]);
            all_zero = 0;
        }
    }

    if (all_zero) {
        printf("  ✅ All words are zero\n");
    }

    return !all_zero;
}
