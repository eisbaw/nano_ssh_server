#ifndef BIGNUM_TRACE_H
#define BIGNUM_TRACE_H

#include "bignum_simple.h"
#include <stdio.h>

/* Traced version of bn_modexp for debugging */
static void bn_modexp_trace(bn_t *r, const bn_t *base, const bn_t *exp, const bn_t *mod) {
    bn_t result, base_copy, temp;

    /* result = 1 */
    bn_zero(&result);
    result.array[0] = 1;
    printf("Initial result = 1\n");

    /* base_copy = base mod m */
    base_copy = *base;
    printf("Initial base_copy.array[0] = %u\n", base_copy.array[0]);
    
    if (bn_cmp(&base_copy, mod) >= 0) {
        printf("base >= mod, reducing...\n");
        bn_2x_t wide;
        bn_2x_zero(&wide);
        for (int i = 0; i < BN_WORDS; i++) {
            wide.array[i] = base_copy.array[i];
        }
        bn_mod_simple(&base_copy, &wide, mod);
        printf("After reduction: base_copy.array[0] = %u\n", base_copy.array[0]);
    }

    /* Find the highest non-zero word in exponent */
    int max_word = -1;
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (exp->array[i] != 0) {
            max_word = i;
            break;
        }
    }
    printf("max_word = %d, exp->array[%d] = %08x\n", max_word, max_word, exp->array[max_word]);

    /* If exponent is zero, return 1 */
    if (max_word < 0) {
        *r = result;
        return;
    }

    /* Binary exponentiation: process bits from LSB to MSB */
    int bits_processed = 0;
    for (int i = 0; i <= max_word; i++) {
        uint32_t exp_word = exp->array[i];

        for (int j = 0; j < 32; j++) {
            bits_processed++;
            
            /* If this bit of exponent is 1, multiply result by base */
            if (exp_word & 1) {
                if (bits_processed <= 5) {
                    printf("Bit %d is 1, multiplying result by base_copy\n", bits_processed-1);
                    printf("  Before: result.array[0]=%u, base_copy.array[0]=%u\n", 
                           result.array[0], base_copy.array[0]);
                }
                
                bn_mulmod(&temp, &result, &base_copy, mod);
                result = temp;
                
                if (bits_processed <= 5) {
                    printf("  After: result.array[0]=%u\n", result.array[0]);
                }
            }

            /* Square the base for next bit (except after last bit) */
            if (i < max_word || j < 31) {
                if (bits_processed <= 5) {
                    printf("Squaring base_copy\n");
                    printf("  Before: base_copy.array[0]=%u\n", base_copy.array[0]);
                }
                
                bn_mulmod(&temp, &base_copy, &base_copy, mod);
                base_copy = temp;
                
                if (bits_processed <= 5) {
                    printf("  After: base_copy.array[0]=%u\n", base_copy.array[0]);
                }
            }

            exp_word >>= 1;
        }
    }

    printf("Final result.array[0] = %u\n", result.array[0]);
    *r = result;
}

#endif
