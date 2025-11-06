/*
 * Debug version of mulmod to understand what's happening
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "bignum_tiny.h"
#include "diffie_hellman.h"

void print_nonzero_words(const char *label, const bn_t *bn) {
    printf("%s: ", label);
    int found = 0;
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (bn->array[i] != 0) {
            if (found) printf(", ");
            printf("word[%d]=0x%x", i, bn->array[i]);
            found = 1;
        }
    }
    if (!found) printf("ZERO");
    printf("\n");
}

/*
 * Simplest possible modular multiplication:
 * Use the fact that (a * b) mod m = ((a mod m) * (b mod m)) mod m
 *
 * But we still need to handle a*b overflow...
 *
 * Let's try: a * b = sum over i of (a[i] * b * 2^(32i))
 * We can compute each term modulo m
 */
void bn_mulmod_v2(bn_t *r, const bn_t *a, const bn_t *b, const bn_t *m) {
    bn_t result, term, b_shifted;

    bn_zero(&result);
    b_shifted = *b;  /* Start with b * 2^0 */

    printf("\nStarting bn_mulmod_v2:\n");
    print_nonzero_words("  a", a);
    print_nonzero_words("  b", b);
    print_nonzero_words("  m (first 3 words)", m);

    /* For each word of a, compute: result += a[i] * (b * 2^(32*i)) mod m */
    for (int i = 0; i < BN_WORDS; i++) {
        if (a->array[i] == 0) {
            /* Shift b_shifted left by 32 bits for next iteration */
            if (i < BN_WORDS - 1) {
                bn_t temp;
                bn_shl1(&temp, &b_shifted);
                for (int k = 0; k < 31; k++) {
                    bn_shl1(&b_shifted, &temp);
                    temp = b_shifted;
                }
                bn_mod(&b_shifted, &temp, m);
            }
            continue;
        }

        if (i < 5) {
            printf("  Iteration i=%d: a[%d]=%u\n", i, i, a->array[i]);
            print_nonzero_words("    b_shifted", &b_shifted);
        }

        /* Compute term = a[i] * b_shifted */
        /* This is still problematic if b_shifted is large! */
        /* Let me use repeated addition instead */
        bn_zero(&term);
        uint32_t multiplier = a->array[i];

        /* term = b_shifted * multiplier, done by repeated addition */
        /* But this is too slow for large multipliers... */
        /* Let me use binary multiplication */
        bn_t temp_term = b_shifted;
        for (int bit = 0; bit < 32; bit++) {
            if (multiplier & (1U << bit)) {
                /* term += temp_term */
                bn_t sum;
                uint32_t carry = 0;
                for (int j = 0; j < BN_WORDS; j++) {
                    uint64_t s = (uint64_t)term.array[j] + temp_term.array[j] + carry;
                    sum.array[j] = (uint32_t)s;
                    carry = (uint32_t)(s >> 32);
                }
                /* Reduce sum */
                bn_mod(&term, &sum, m);
            }

            /* temp_term *= 2 */
            if (bit < 31) {
                bn_t doubled;
                bn_shl1(&doubled, &temp_term);
                bn_mod(&temp_term, &doubled, m);
            }
        }

        if (i < 5) {
            print_nonzero_words("    term", &term);
        }

        /* result += term */
        bn_t sum;
        uint32_t carry = 0;
        for (int j = 0; j < BN_WORDS; j++) {
            uint64_t s = (uint64_t)result.array[j] + term.array[j] + carry;
            sum.array[j] = (uint32_t)s;
            carry = (uint32_t)(s >> 32);
        }
        bn_mod(&result, &sum, m);

        if (i < 5) {
            print_nonzero_words("    result", &result);
        }

        /* Shift b_shifted left by 32 bits for next iteration: b_shifted *= 2^32 */
        if (i < BN_WORDS - 1) {
            bn_t temp;
            /* Shift left 32 times */
            temp = b_shifted;
            for (int k = 0; k < 32; k++) {
                bn_shl1(&b_shifted, &temp);
                temp = b_shifted;
            }
            bn_mod(&b_shifted, &temp, m);
        }
    }

    *r = result;
    print_nonzero_words("  Final result", r);
}

int main() {
    bn_t a, b, m, result;

    printf("========================================\n");
    printf("Test bn_mulmod_v2 with debugging\n");
    printf("========================================\n");

    /* Load DH Group14 prime */
    bn_from_bytes(&m, dh_group14_prime, 256);

    /* Test: 17 * 19 mod prime = 323 */
    printf("\n\nTest 1: (17 * 19) mod prime\n");
    printf("Expected: 323\n");
    bn_zero(&a);
    a.array[0] = 17;
    bn_zero(&b);
    b.array[0] = 19;

    bn_mulmod_v2(&result, &a, &b, &m);

    if (result.array[0] == 323) {
        printf("✅ PASS\n");
    } else {
        printf("❌ FAIL: got %u\n", result.array[0]);
        return 1;
    }

    /* Test: 2^32 * 2 mod prime */
    printf("\n\nTest 2: (2^32 * 2) mod prime\n");
    printf("Expected: 2^33\n");
    bn_zero(&a);
    a.array[1] = 1;  /* 2^32 */
    bn_zero(&b);
    b.array[0] = 2;

    bn_mulmod_v2(&result, &a, &b, &m);

    if (result.array[1] == 2 && result.array[0] == 0) {
        printf("✅ PASS\n");
    } else {
        printf("❌ FAIL\n");
        print_nonzero_words("result", &result);
        return 1;
    }

    return 0;
}
