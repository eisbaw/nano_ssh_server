#include <stdio.h>
#include <stdint.h>
#include "bignum_simple.h"
#include "rsa.h"

int main() {
    rsa_key_t key;
    bn_t m, c;

    rsa_init_key(&key);

    /* Test: 42^3 mod n = 74088 */
    bn_zero(&m);
    m.array[0] = 42;

    bn_t exp_small;
    bn_zero(&exp_small);
    exp_small.array[0] = 3;

    printf("Computing 42^3 mod n\n");
    printf("Expected: 74088 (0x12168)\n\n");

    bn_modexp(&c, &m, &exp_small, &key.n);

    printf("Result: c.array[0] = 0x%08x (%u)\n", c.array[0], c.array[0]);

    if (c.array[0] == 74088) {
        printf("✓ CORRECT\n");
    } else {
        printf("✗ WRONG\n");
    }

    return 0;
}
