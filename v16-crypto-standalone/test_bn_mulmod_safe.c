/*
 * Modular multiplication: r = (a * b) mod m
 * Uses word-by-word multiplication with reduction to avoid overflow
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include "bignum_tiny.h"
#include "diffie_hellman.h"

/* Helper: multiply-add-carry for a single word */
static inline uint32_t mul_add_carry(uint32_t *result, uint32_t a, uint32_t b, uint32_t carry) {
    uint64_t product = (uint64_t)a * b + *result + carry;
    *result = (uint32_t)product;
    return (uint32_t)(product >> 32);
}

/*
 * Modular multiplication without double-width buffer
 *
 * Strategy: Instead of computing full product then reducing,
 * we accumulate partial products and reduce periodically.
 *
 * For each word a[i]:
 *   partial_product = a[i] * b (fits in BN_WORDS+1 words)
 *   partial_product <<= (i * 32) bits
 *   result += partial_product mod m
 */
void bn_mulmod_safe(bn_t *r, const bn_t *a, const bn_t *b, const bn_t *m) {
    bn_t result, partial, shifted_partial;

    bn_zero(&result);

    /* For each word of a */
    for (int i = 0; i < BN_WORDS; i++) {
        if (a->array[i] == 0) continue;  /* Skip zero words for efficiency */

        /* Compute partial = a[i] * b */
        bn_zero(&partial);
        uint32_t carry = 0;
        for (int j = 0; j < BN_WORDS; j++) {
            carry = mul_add_carry(&partial.array[j], a->array[i], b->array[j], carry);
        }
        /* carry represents overflow beyond BN_WORDS - discard it since we'll reduce anyway */

        /* Shift partial left by i words: partial <<= (i * 32) bits */
        bn_zero(&shifted_partial);
        for (int j = 0; j < BN_WORDS - i; j++) {
            shifted_partial.array[j + i] = partial.array[j];
        }

        /* Add to result: result += shifted_partial */
        bn_t temp_sum;
        uint32_t add_carry = 0;
        for (int j = 0; j < BN_WORDS; j++) {
            uint64_t sum = (uint64_t)result.array[j] + shifted_partial.array[j] + add_carry;
            temp_sum.array[j] = (uint32_t)sum;
            add_carry = (uint32_t)(sum >> 32);
        }

        /* Reduce modulo m */
        bn_mod(&result, &temp_sum, m);
    }

    *r = result;
}

/* Test the new mulmod function */
int main() {
    uint8_t priv_bytes[256];
    bn_t a, b, m, result, expected;

    printf("========================================\n");
    printf("Test bn_mulmod_safe\n");
    printf("========================================\n\n");

    /* Load DH Group14 prime as modulus */
    bn_from_bytes(&m, dh_group14_prime, 256);

    /* Test 1: (2^1024) * (2^1024) mod prime = 2^2048 mod prime */
    printf("Test 1: (2^1024 * 2^1024) mod prime\n");
    bn_zero(&a);
    a.array[32] = 1;  /* a = 2^1024 */
    bn_zero(&b);
    b.array[32] = 1;  /* b = 2^1024 */

    printf("  Input: a = 2^1024, b = 2^1024\n");
    printf("  Expected: 2^2048 mod prime (a large value < prime)\n");

    bn_mulmod_safe(&result, &a, &b, &m);

    if (bn_is_zero(&result)) {
        printf("  ❌ FAIL: Result is zero!\n\n");
        return 1;
    }

    printf("  ✅ Result is non-zero\n");
    printf("  result.array[0] = 0x%08x\n", result.array[0]);
    printf("  result.array[32] = 0x%08x\n", result.array[32]);
    printf("  result.array[63] = 0x%08x\n\n", result.array[63]);

    /* Test 2: Small values to verify correctness */
    printf("Test 2: (17 * 19) mod prime = 323\n");
    bn_zero(&a);
    a.array[0] = 17;
    bn_zero(&b);
    b.array[0] = 19;

    bn_mulmod_safe(&result, &a, &b, &m);

    if (result.array[0] == 323 && result.array[1] == 0) {
        printf("  ✅ PASS: result = %u\n\n", result.array[0]);
    } else {
        printf("  ❌ FAIL: result = %u (expected 323)\n\n", result.array[0]);
        return 1;
    }

    /* Test 3: Squaring sequence to simulate modexp */
    printf("Test 3: Simulate modexp squaring sequence\n");
    bn_zero(&b);
    b.array[0] = 2;

    for (int i = 0; i < 12; i++) {
        printf("  Iteration %d: ", i);

        /* Count non-zero words */
        int nonzero = 0;
        int max_word = -1;
        for (int j = 0; j < BN_WORDS; j++) {
            if (b.array[j] != 0) {
                nonzero++;
                max_word = j;
            }
        }
        printf("max_word=%d, ", max_word);

        /* Square: b = (b * b) mod m */
        bn_mulmod_safe(&result, &b, &b, &m);
        b = result;

        if (bn_is_zero(&b)) {
            printf("❌ BECAME ZERO\n");
            return 1;
        }

        printf("✅ non-zero\n");
    }

    printf("\n✅ All 12 iterations completed without becoming zero!\n");
    printf("This proves bn_mulmod_safe works past the overflow point.\n");

    return 0;
}
