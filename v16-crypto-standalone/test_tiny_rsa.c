#include <stdio.h>
#include <stdint.h>
#include "bignum_adapter.h"
#include "rsa.h"

int main() {
    rsa_key_t key;
    bn_t m, c;

    rsa_init_key(&key);

    /* Test: 42^65537 mod n */
    bn_zero(&m);
    bignum_from_int((struct bn*)&m, 42);

    printf("Computing 42^65537 mod n with tiny-bignum-c\n\n");

    bn_modexp(&c, &m, &key.e, &key.n);

    printf("Result:\n");
    printf("  c.array[0] = 0x%08x (%u)\n", c.array[0], c.array[0]);

    printf("\nExpected from Python:\n");
    printf("  c.array[0] = 0xf91e0d65 (4179496293)\n");

    if (c.array[0] == 0xf91e0d65) {
        printf("\n✓ CORRECT!\n");
        return 0;
    } else {
        printf("\n✗ Wrong result\n");
        return 1;
    }
}
