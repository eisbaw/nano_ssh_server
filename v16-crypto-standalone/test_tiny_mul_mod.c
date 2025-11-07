#include <stdio.h>
#include <stdint.h>
#include "bn_tinybignum.h"

static void print_bn(const char *name, const struct bn *n) {
    printf("%s: ", name);
    int found_nonzero = 0;
    for (int i = BN_ARRAY_SIZE - 1; i >= 0; i--) {
        if (n->array[i] != 0 || found_nonzero || i == 0) {
            if (!found_nonzero && n->array[i] != 0) {
                printf("0x%x", n->array[i]);
                found_nonzero = 1;
            } else if (found_nonzero) {
                printf("%08x", n->array[i]);
            }
        }
    }
    if (!found_nonzero) printf("0");
    printf("\n");
}

int main() {
    struct bn a, b, result, mod;

    printf("Test 1: Simple multiplication\n");
    bignum_init(&a);
    bignum_init(&b);
    bignum_from_int(&a, 42);
    bignum_from_int(&b, 42);

    bignum_mul(&a, &b, &result);
    print_bn("42 * 42", &result);
    printf("Expected: 1764 (0x6e4)\n\n");

    printf("Test 2: Modular reduction\n");
    bignum_init(&mod);
    bignum_from_int(&mod, 100);

    bignum_mod(&result, &mod, &result);
    print_bn("1764 mod 100", &result);
    printf("Expected: 64\n\n");

    printf("Test 3: Large squaring\n");
    bignum_init(&a);
    a.array[0] = 0x34c10000;  /* From our debug trace */

    bignum_mul(&a, &a, &result);
    print_bn("0x34c10000 * 0x34c10000", &result);
    printf("Expected: 0xade69eac1000000\n");
    printf("array[0] = 0x%08x (expected 0xc1000000)\n", result.array[0]);
    printf("array[1] = 0x%08x (expected 0x0ade69ea)\n", result.array[1]);

    return 0;
}
