/*
 * Debug modular exponentiation with tiny-bignum-c
 */
#include <stdio.h>
#include <stdint.h>
#include "bignum_adapter.h"
#include "rsa.h"

static void print_bn_first(const char *name, const bn_t *n) {
    printf("%s: array[0] = 0x%08x\n", name, n->array[0]);
}

int main() {
    rsa_key_t key;
    bn_t m, c;

    printf("Debug: Testing modular exponentiation step by step\n\n");

    rsa_init_key(&key);

    /* Test: 42^65537 mod n */
    bn_zero(&m);
    bignum_from_int((struct bn*)&m, 42);

    printf("Initial values:\n");
    print_bn_first("base (m=42)", &m);
    print_bn_first("exponent (e=65537)", &key.e);
    print_bn_first("modulus (n)", &key.n);
    printf("\n");

    /* Manually trace through bn_modexp logic */
    bn_t result, base_copy, temp;

    /* result = 1 */
    bignum_init(&result);
    bignum_from_int(&result, 1);
    print_bn_first("result initialized to 1", &result);

    /* base_copy = base mod m */
    bignum_assign(&base_copy, (struct bn*)&m);
    print_bn_first("base_copy = base", &base_copy);

    if (bignum_cmp(&base_copy, (struct bn*)&key.n) >= 0) {
        printf("base >= modulus, reducing...\n");
        bignum_mod(&base_copy, (struct bn*)&key.n, &base_copy);
        print_bn_first("base_copy after mod", &base_copy);
    } else {
        printf("base < modulus, no reduction needed\n");
    }

    /* Find highest non-zero word in exponent */
    int max_word = -1;
    for (int i = BN_ARRAY_SIZE - 1; i >= 0; i--) {
        if (key.e.array[i] != 0) {
            max_word = i;
            break;
        }
    }
    printf("max_word in exponent: %d\n", max_word);

    if (max_word < 0) {
        printf("ERROR: Exponent is zero!\n");
        return 1;
    }

    printf("\nStarting binary exponentiation...\n");
    printf("Processing exponent word 0: 0x%08x\n", key.e.array[0]);

    /* Process first few bits manually */
    DTYPE exp_word = key.e.array[0];
    int bits_processed = 0;

    for (int j = 0; j < 32 && bits_processed < 5; j++, bits_processed++) {
        printf("\nBit %d: %d\n", j, exp_word & 1);

        if (exp_word & 1) {
            printf("  Bit is 1: result = (result * base_copy) mod n\n");
            bignum_mul(&result, &base_copy, &temp);
            print_bn_first("  temp after mul", &temp);
            bignum_mod(&temp, (struct bn*)&key.n, &result);
            print_bn_first("  result after mod", &result);
        } else {
            printf("  Bit is 0: skip multiply\n");
        }

        exp_word >>= 1;

        if (exp_word == 0 && 0 == max_word) {
            printf("  All bits processed, stopping\n");
            break;
        }

        printf("  Squaring base: base_copy = (base_copy * base_copy) mod n\n");
        bignum_mul(&base_copy, &base_copy, &temp);
        print_bn_first("  temp after square", &temp);
        bignum_mod(&temp, (struct bn*)&key.n, &base_copy);
        print_bn_first("  base_copy after mod", &base_copy);
    }

    printf("\n\nNow call bn_modexp and check result:\n");
    bn_modexp(&c, &m, &key.e, &key.n);
    print_bn_first("Final result", &c);

    printf("\nExpected: 0xf91e0d65 (4179496293)\n");

    if (c.array[0] == 0xf91e0d65) {
        printf("✓ CORRECT!\n");
        return 0;
    } else {
        printf("✗ Wrong result\n");
        return 1;
    }
}
