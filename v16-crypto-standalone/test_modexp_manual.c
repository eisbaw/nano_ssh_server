/*
 * Manual step-by-step modexp for 2^17 with explicit word checks
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "bignum_tiny.h"
#include "diffie_hellman.h"

int main() {
    uint8_t priv_bytes[256];
    bn_t priv, prime, generator;
    bn_t result, temp_base, temp;

    printf("Manual modexp: 2^17 mod prime\n\n");

    /* Setup */
    memset(priv_bytes, 0, 256);
    priv_bytes[255] = 17;  /* Exponent = 17 = 0b10001 */

    bn_from_bytes(&priv, priv_bytes, 256);
    bn_from_bytes(&prime, dh_group14_prime, 256);
    bn_zero(&generator);
    generator.array[0] = 2;

    /* Initialize */
    bn_zero(&result);
    bn_zero(&temp_base);
    bn_zero(&temp);
    result.array[0] = 1;
    temp_base.array[0] = 2;  /* temp_base = base % mod = 2 */

    printf("Initialized: result=1, temp_base=2\n");
    printf("Exponent bits: 10001 (bits 0 and 4 set)\n\n");

    /* Process bit 0 (set) */
    printf("=== Bit 0 (SET) ===\n");
    printf("  result=%u, temp_base=%u\n", result.array[0], temp_base.array[0]);
    bn_mul(&temp, &result, &temp_base);  /* result * temp_base */
    printf("  After mul: temp.array[0]=%u\n", temp.array[0]);
    bn_mod(&result, &temp, &prime);
    printf("  After mod: result=%u\n\n", result.array[0]);

    /* Square for bits 1-3 */
    for (int bit = 1; bit <= 3; bit++) {
        printf("=== Bit %d (not set, just square) ===\n", bit);
        printf("  temp_base=%u\n", temp_base.array[0]);
        bn_mul(&temp, &temp_base, &temp_base);
        printf("  After square: temp.array[0]=%u, temp.array[1]=%u\n",
               temp.array[0], temp.array[1]);
        bn_mod(&temp_base, &temp, &prime);
        printf("  After mod: temp_base=%u\n\n", temp_base.array[0]);
    }

    /* Process bit 4 (set) */
    printf("=== Bit 4 (SET) ===\n");
    printf("  result=%u, temp_base=%u\n", result.array[0], temp_base.array[0]);
    bn_mul(&temp, &result, &temp_base);
    printf("  After mul: temp.array[0]=%u\n", temp.array[0]);
    bn_mod(&result, &temp, &prime);
    printf("  After mod: result=%u\n\n", result.array[0]);

    /* Continue squaring until we've processed ~20 more bits */
    printf("=== Continuing to square temp_base ===\n");
    for (int bit = 5; bit <= 12; bit++) {
        printf("\nBit %d:\n", bit);

        /* Count non-zero words before */
        int nonzero_before = 0;
        int max_word_before = -1;
        for (int i = 0; i < BN_WORDS; i++) {
            if (temp_base.array[i] != 0) {
                nonzero_before++;
                max_word_before = i;
            }
        }
        printf("  Before: temp_base has %d non-zero words, highest at word[%d]\n",
               nonzero_before, max_word_before);
        if (max_word_before >= 0 && max_word_before < 20) {
            printf("    temp_base.array[%d] = 0x%08x\n", max_word_before, temp_base.array[max_word_before]);
        }

        /* Square */
        printf("  Calling bn_mul(&temp, &temp_base, &temp_base)...\n");
        bn_mul(&temp, &temp_base, &temp_base);

        /* Check temp manually word by word */
        int nonzero_after_mul = 0;
        int max_word_after_mul = -1;
        for (int i = 0; i < BN_WORDS; i++) {
            if (temp.array[i] != 0) {
                nonzero_after_mul++;
                max_word_after_mul = i;
            }
        }
        printf("  After mul: temp has %d non-zero words, highest at word[%d]\n",
               nonzero_after_mul, max_word_after_mul);
        if (max_word_after_mul >= 0 && max_word_after_mul < 20) {
            printf("    temp.array[%d] = 0x%08x\n", max_word_after_mul, temp.array[max_word_after_mul]);
        }

        /* Check bn_is_zero */
        int is_zero = bn_is_zero(&temp);
        printf("  bn_is_zero(&temp) = %d\n", is_zero);

        if (is_zero && nonzero_after_mul > 0) {
            printf("  ❌ CONTRADICTION: Found %d non-zero words but bn_is_zero returned 1!\n",
                   nonzero_after_mul);
            printf("  Manually checking bn_is_zero logic:\n");
            for (int i = 0; i < BN_WORDS; i++) {
                if (temp.array[i] != 0) {
                    printf("    word[%d] = 0x%08x (NON-ZERO)\n", i, temp.array[i]);
                }
            }
            return 1;
        }

        if (is_zero) {
            printf("  ❌ ERROR: temp is zero after squaring!\n");
            printf("  This means bn_mul(%d^2) returned all zeros\n", max_word_before);
            return 1;
        }

        /* Reduce mod prime */
        printf("  Calling bn_mod(&temp_base, &temp, &prime)...\n");
        bn_mod(&temp_base, &temp, &prime);

        /* Check result */
        int nonzero_after_mod = 0;
        for (int i = 0; i < BN_WORDS; i++) {
            if (temp_base.array[i] != 0) {
                nonzero_after_mod++;
            }
        }
        printf("  After mod: temp_base has %d non-zero words\n", nonzero_after_mod);

        if (bn_is_zero(&temp_base)) {
            printf("  ❌ ERROR: temp_base became zero after mod!\n");
            return 1;
        }
    }

    printf("\n✅ All steps completed successfully!\n");
    printf("Final result: %u\n", result.array[0]);
    printf("Expected: 131072 (2^17)\n");

    if (result.array[0] == 131072) {
        printf("✅ CORRECT\n");
        return 0;
    } else {
        printf("❌ WRONG\n");
        return 1;
    }
}
