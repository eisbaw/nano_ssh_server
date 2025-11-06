#include <stdio.h>
#include <stdint.h>
#include "rsa.h"

int main() {
    rsa_key_t key;
    bn_t a, b, result;

    rsa_init_key(&key);

    /* Test: 42 * 42 mod n */
    bn_zero(&a);
    a.array[0] = 42;

    bn_zero(&b);
    b.array[0] = 42;

    printf("Testing: 42 * 42 mod n\n");
    printf("a.array[0] = %u\n", a.array[0]);
    printf("b.array[0] = %u\n", b.array[0]);
    printf("n.array[63] = %08x (highest word)\n", key.n.array[63]);

    bn_mulmod(&result, &a, &b, &key.n);

    printf("\nResult:\n");
    int found_nonzero = 0;
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (result.array[i] != 0 || i == 0) {
            printf("result.array[%d] = %u\n", i, result.array[i]);
            found_nonzero = 1;
            if (found_nonzero && i > 0) break;
        }
    }

    /* Expected: 42 * 42 = 1764 (since 1764 << n) */
    if (result.array[0] == 1764) {
        printf("✓ Correct: 42 * 42 = 1764\n");
    } else {
        printf("✗ Wrong: expected 1764, got %u\n", result.array[0]);
    }

    return 0;
}
