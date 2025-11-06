/*
 * Simple, correct bignum implementation without Montgomery multiplication
 * Based on standard algorithms - prioritizes correctness over performance
 */
#ifndef BIGNUM_SIMPLE_H
#define BIGNUM_SIMPLE_H

#include <stdint.h>
#include <string.h>

#define BN_WORDS 64
#define BN_BYTES 256
#define BN_2X_WORDS 128

typedef struct {
    uint32_t array[BN_WORDS];
} bn_t;

typedef struct {
    uint32_t array[BN_2X_WORDS];
} bn_2x_t;

static inline void bn_zero(bn_t *a) {
    memset(a, 0, sizeof(bn_t));
}

static inline void bn_2x_zero(bn_2x_t *a) {
    memset(a, 0, sizeof(bn_2x_t));
}

static inline int bn_is_zero(const bn_t *a) {
    for (int i = 0; i < BN_WORDS; i++) {
        if (a->array[i] != 0) return 0;
    }
    return 1;
}

static inline void bn_from_bytes(bn_t *a, const uint8_t *bytes, size_t len) {
    bn_zero(a);
    if (len > BN_BYTES) len = BN_BYTES;
    for (size_t i = 0; i < len; i++) {
        int word_idx = (len - 1 - i) / 4;
        int byte_idx = (len - 1 - i) % 4;
        a->array[word_idx] |= ((uint32_t)bytes[i]) << (byte_idx * 8);
    }
}

static inline void bn_to_bytes(const bn_t *a, uint8_t *bytes, size_t len) {
    memset(bytes, 0, len);
    if (len > BN_BYTES) len = BN_BYTES;
    for (size_t i = 0; i < len; i++) {
        int word_idx = (len - 1 - i) / 4;
        int byte_idx = (len - 1 - i) % 4;
        bytes[i] = (a->array[word_idx] >> (byte_idx * 8)) & 0xFF;
    }
}

static int bn_cmp(const bn_t *a, const bn_t *b) {
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (a->array[i] < b->array[i]) return -1;
        if (a->array[i] > b->array[i]) return 1;
    }
    return 0;
}

static uint32_t bn_add(bn_t *r, const bn_t *a, const bn_t *b) {
    uint32_t carry = 0;
    for (int i = 0; i < BN_WORDS; i++) {
        uint64_t sum = (uint64_t)a->array[i] + b->array[i] + carry;
        r->array[i] = (uint32_t)sum;
        carry = (uint32_t)(sum >> 32);
    }
    return carry;
}

static void bn_sub(bn_t *r, const bn_t *a, const bn_t *b) {
    uint32_t borrow = 0;
    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t next_borrow = (a->array[i] < b->array[i] + borrow) ? 1 : 0;
        r->array[i] = a->array[i] - b->array[i] - borrow;
        borrow = next_borrow;
    }
}

/* Wide multiplication: r = a * b (full 4096-bit result) */
static void bn_mul_wide(bn_2x_t *r, const bn_t *a, const bn_t *b) {
    bn_2x_zero(r);

    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t carry = 0;
        for (int j = 0; j < BN_WORDS; j++) {
            uint64_t product = (uint64_t)a->array[i] * b->array[j] + r->array[i + j] + carry;
            r->array[i + j] = (uint32_t)product;
            carry = (uint32_t)(product >> 32);
        }
        r->array[i + BN_WORDS] = carry;
    }
}

/* Simple modular reduction using repeated subtraction */
static void bn_mod_simple(bn_t *r, const bn_2x_t *a, const bn_t *m) {
    bn_2x_t temp = *a;

    /* Repeatedly subtract m until result < m */
    for (int iter = 0; iter < 10000; iter++) {
        /* Check if temp < m (compare high words first) */
        int all_high_zero = 1;
        for (int i = BN_WORDS; i < BN_2X_WORDS; i++) {
            if (temp.array[i] != 0) {
                all_high_zero = 0;
                break;
            }
        }

        if (all_high_zero) {
            /* Copy low words to result */
            for (int i = 0; i < BN_WORDS; i++) {
                r->array[i] = temp.array[i];
            }

            /* Keep subtracting m until r < m */
            while (bn_cmp(r, m) >= 0) {
                bn_sub(r, r, m);
            }
            return;
        }

        /* temp >= 2^2048, so subtract m * 2^2048 */
        /* This is equivalent to subtracting m from the high words */
        uint32_t borrow = 0;
        for (int i = 0; i < BN_WORDS; i++) {
            uint64_t diff = (uint64_t)temp.array[BN_WORDS + i] - (uint64_t)m->array[i] - (uint64_t)borrow;
            temp.array[BN_WORDS + i] = (uint32_t)(diff & 0xFFFFFFFF);
            borrow = (diff >> 32) & 1;  /* Check if subtraction underflowed */
        }
    }

    /* Should never reach here */
    bn_zero(r);
}

/* Modular multiplication: r = (a * b) mod m */
static void bn_mulmod(bn_t *r, const bn_t *a, const bn_t *b, const bn_t *m) {
    bn_2x_t product;
    bn_mul_wide(&product, a, b);
    bn_mod_simple(r, &product, m);
}

/* Modular exponentiation: r = (base^exp) mod m */
static void bn_modexp(bn_t *r, const bn_t *base, const bn_t *exp, const bn_t *mod) {
    bn_t result, base_copy, temp;

    /* result = 1 */
    bn_zero(&result);
    result.array[0] = 1;

    /* base_copy = base mod m */
    base_copy = *base;
    if (bn_cmp(&base_copy, mod) >= 0) {
        bn_2x_t wide;
        bn_2x_zero(&wide);
        for (int i = 0; i < BN_WORDS; i++) {
            wide.array[i] = base_copy.array[i];
        }
        bn_mod_simple(&base_copy, &wide, mod);
    }

    /* Find the highest non-zero word in exponent */
    int max_word = -1;
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (exp->array[i] != 0) {
            max_word = i;
            break;
        }
    }

    /* If exponent is zero, return 1 */
    if (max_word < 0) {
        *r = result;
        return;
    }

    /* Binary exponentiation: process bits from LSB to MSB */
    for (int i = 0; i <= max_word; i++) {
        uint32_t exp_word = exp->array[i];

        for (int j = 0; j < 32; j++) {
            /* If this bit of exponent is 1, multiply result by base */
            if (exp_word & 1) {
                bn_mulmod(&temp, &result, &base_copy, mod);
                result = temp;
            }

            exp_word >>= 1;

            /* Early exit if no more bits in this word and we're at the last word */
            if (exp_word == 0 && i == max_word) {
                break;
            }

            /* Square the base for next bit */
            bn_mulmod(&temp, &base_copy, &base_copy, mod);
            base_copy = temp;
        }
    }

    *r = result;
}

#endif /* BIGNUM_SIMPLE_H */
