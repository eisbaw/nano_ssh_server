/*
 * Adapter for tiny-bignum-c library
 * Provides interface compatible with our RSA code while using proven tiny-bignum-c
 */
#ifndef BIGNUM_ADAPTER_H
#define BIGNUM_ADAPTER_H

#include "bn_tinybignum.h"
#include <string.h>

/* Alias tiny-bignum types to our naming */
typedef struct bn bn_t;

#define BN_WORDS BN_ARRAY_SIZE
#define BN_BYTES 256

/* Initialize to zero */
static inline void bn_zero(bn_t *n) {
    bignum_init(n);
}

/* Convert from big-endian bytes */
static inline void bn_from_bytes(bn_t *n, const uint8_t *bytes, size_t len) {
    bignum_init(n);
    if (len > BN_BYTES) len = BN_BYTES;

    /* Convert big-endian bytes to little-endian words
     * tiny-bignum-c uses little-endian for both word order and bytes within words
     *
     * Big-endian input: [01 02 03 04 05 06 07 08] represents 0x0102030405060708
     * Little-endian words:
     *   array[0] (LSW) = 0x08070605 (bytes [05 06 07 08] reversed)
     *   array[1] (MSW) = 0x04030201 (bytes [01 02 03 04] reversed)
     */

    for (size_t i = 0; i < len; i++) {
        size_t word_idx = i / WORD_SIZE;
        size_t byte_in_word = (WORD_SIZE - 1) - (i % WORD_SIZE);  /* Reverse byte order within word */
        size_t src_idx = len - 1 - i;  /* Read from end (convert big-endian to little-endian) */

        if (word_idx < BN_ARRAY_SIZE) {
            n->array[word_idx] |= ((DTYPE)bytes[src_idx]) << (byte_in_word * 8);
        }
    }
}

/* Convert to big-endian bytes */
static inline void bn_to_bytes(const bn_t *n, uint8_t *bytes, size_t len) {
    memset(bytes, 0, len);
    if (len > BN_BYTES) len = BN_BYTES;

    /* Convert little-endian words to big-endian bytes
     * Reverse of bn_from_bytes operation
     */

    for (size_t i = 0; i < len; i++) {
        size_t word_idx = i / WORD_SIZE;
        size_t byte_in_word = (WORD_SIZE - 1) - (i % WORD_SIZE);  /* Reverse byte order within word */
        size_t dst_idx = len - 1 - i;  /* Write to end (convert little-endian to big-endian) */

        if (word_idx < BN_ARRAY_SIZE) {
            bytes[dst_idx] = (n->array[word_idx] >> (byte_in_word * 8)) & 0xFF;
        }
    }
}

/* Compare */
static inline int bn_cmp(const bn_t *a, const bn_t *b) {
    return bignum_cmp((struct bn*)a, (struct bn*)b);
}

/* Check if zero */
static inline int bn_is_zero(const bn_t *n) {
    return bignum_is_zero((struct bn*)n);
}

/* Modular exponentiation: r = (base^exp) mod m */
static void bn_modexp(bn_t *r, const bn_t *base, const bn_t *exp, const bn_t *mod) {
    bn_t result, base_copy, temp;
    
    /* result = 1 */
    bignum_init(&result);
    bignum_from_int(&result, 1);
    
    /* base_copy = base mod m */
    bignum_assign(&base_copy, (struct bn*)base);
    if (bignum_cmp(&base_copy, (struct bn*)mod) >= 0) {
        bignum_mod(&base_copy, (struct bn*)mod, &base_copy);
    }
    
    /* Find highest non-zero word in exponent */
    int max_word = -1;
    for (int i = BN_ARRAY_SIZE - 1; i >= 0; i--) {
        if (exp->array[i] != 0) {
            max_word = i;
            break;
        }
    }
    
    /* If exponent is zero, return 1 */
    if (max_word < 0) {
        bignum_assign(r, &result);
        return;
    }
    
    /* Binary exponentiation: process bits from LSB to MSB */
    for (int i = 0; i <= max_word; i++) {
        DTYPE exp_word = exp->array[i];
        
        for (int j = 0; j < 32; j++) {
            /* If this bit of exponent is 1, multiply result by base */
            if (exp_word & 1) {
                bignum_mul(&result, &base_copy, &temp);
                bignum_mod(&temp, (struct bn*)mod, &result);
            }
            
            exp_word >>= 1;
            
            /* Early exit if no more bits */
            if (exp_word == 0 && i == max_word) {
                break;
            }
            
            /* Square the base for next bit */
            bignum_mul(&base_copy, &base_copy, &temp);
            bignum_mod(&temp, (struct bn*)mod, &base_copy);
        }
    }
    
    bignum_assign(r, &result);
}

#endif /* BIGNUM_ADAPTER_H */
