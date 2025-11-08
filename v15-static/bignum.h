/*
 * Minimal Big Integer Library
 * Based on tiny-bignum-c (public domain)
 * Configured for 2048-bit operations (SSH DH Group14 + RSA-2048)
 *
 * Optimized for size, not speed
 */

#ifndef BIGNUM_H
#define BIGNUM_H

#include <stdint.h>
#include <string.h>

/* 2048-bit big integer = 256 bytes = 64 words (32-bit) */
#define BN_ARRAY_SIZE  64   /* 64 × 32-bit = 2048 bits */
#define BN_BYTES       256  /* 2048 bits = 256 bytes */

typedef struct {
    uint32_t array[BN_ARRAY_SIZE];
} bn_t;

/* Initialize bignum to zero */
static inline void bn_zero(bn_t *n) {
    memset(n->array, 0, sizeof(n->array));
}

/* Initialize bignum from bytes (big-endian) */
static inline void bn_from_bytes(bn_t *n, const uint8_t *bytes, size_t len) {
    bn_zero(n);

    /* Copy bytes in big-endian format */
    size_t max_len = (len < BN_BYTES) ? len : BN_BYTES;
    for (size_t i = 0; i < max_len; i++) {
        size_t byte_idx = max_len - 1 - i;
        size_t word_idx = i / 4;
        size_t byte_in_word = i % 4;
        n->array[word_idx] |= ((uint32_t)bytes[byte_idx]) << (byte_in_word * 8);
    }
}

/* Export bignum to bytes (big-endian) */
static inline void bn_to_bytes(const bn_t *n, uint8_t *bytes, size_t len) {
    memset(bytes, 0, len);

    size_t max_len = (len < BN_BYTES) ? len : BN_BYTES;
    for (size_t i = 0; i < max_len; i++) {
        size_t word_idx = i / 4;
        size_t byte_in_word = i % 4;
        size_t byte_idx = max_len - 1 - i;
        bytes[byte_idx] = (n->array[word_idx] >> (byte_in_word * 8)) & 0xFF;
    }
}

/* Compare two bignums: returns -1 if a < b, 0 if a == b, 1 if a > b */
static inline int bn_cmp(const bn_t *a, const bn_t *b) {
    for (int i = BN_ARRAY_SIZE - 1; i >= 0; i--) {
        if (a->array[i] > b->array[i]) return 1;
        if (a->array[i] < b->array[i]) return -1;
    }
    return 0;
}

/* Check if bignum is zero */
static inline int bn_is_zero(const bn_t *n) {
    for (int i = 0; i < BN_ARRAY_SIZE; i++) {
        if (n->array[i] != 0) return 0;
    }
    return 1;
}

/* Addition: c = a + b */
static inline void bn_add(bn_t *c, const bn_t *a, const bn_t *b) {
    uint64_t carry = 0;

    for (int i = 0; i < BN_ARRAY_SIZE; i++) {
        uint64_t sum = (uint64_t)a->array[i] + (uint64_t)b->array[i] + carry;
        c->array[i] = (uint32_t)sum;
        carry = sum >> 32;
    }
}

/* Subtraction: c = a - b (assumes a >= b) */
static inline void bn_sub(bn_t *c, const bn_t *a, const bn_t *b) {
    uint64_t borrow = 0;

    for (int i = 0; i < BN_ARRAY_SIZE; i++) {
        uint64_t diff = (uint64_t)a->array[i] - (uint64_t)b->array[i] - borrow;
        c->array[i] = (uint32_t)diff;
        borrow = (diff >> 32) & 1;
    }
}

/* Left shift by one bit: n = n << 1 */
static inline void bn_lshift1(bn_t *n) {
    uint32_t carry = 0;

    for (int i = 0; i < BN_ARRAY_SIZE; i++) {
        uint32_t new_carry = n->array[i] >> 31;
        n->array[i] = (n->array[i] << 1) | carry;
        carry = new_carry;
    }
}

/* Right shift by one bit: n = n >> 1 */
static inline void bn_rshift1(bn_t *n) {
    for (int i = 0; i < BN_ARRAY_SIZE - 1; i++) {
        n->array[i] = (n->array[i] >> 1) | (n->array[i + 1] << 31);
    }
    n->array[BN_ARRAY_SIZE - 1] >>= 1;
}

/* Multiplication: c = a * b */
static inline void bn_mul(bn_t *c, const bn_t *a, const bn_t *b) {
    bn_t result;
    bn_zero(&result);

    /* Simple multiplication algorithm */
    for (int i = 0; i < BN_ARRAY_SIZE; i++) {
        if (b->array[i] == 0) continue;

        uint64_t carry = 0;
        for (int j = 0; j < BN_ARRAY_SIZE - i; j++) {
            uint64_t product = (uint64_t)a->array[j] * (uint64_t)b->array[i] +
                              (uint64_t)result.array[i + j] + carry;
            result.array[i + j] = (uint32_t)product;
            carry = product >> 32;
        }
    }

    memcpy(c, &result, sizeof(bn_t));
}

/* Modulo: r = a mod m (using division) */
static inline void bn_mod(bn_t *r, const bn_t *a, const bn_t *m) {
    bn_t tmp;
    memcpy(&tmp, a, sizeof(bn_t));

    /* Simple repeated subtraction (slow but small code) */
    while (bn_cmp(&tmp, m) >= 0) {
        bn_sub(&tmp, &tmp, m);
    }

    memcpy(r, &tmp, sizeof(bn_t));
}

/* Modular exponentiation: result = base^exp mod modulus
 * Using binary exponentiation (right-to-left)
 * This is the most important function for DH and RSA
 */
static inline void bn_modexp(bn_t *result, const bn_t *base, const bn_t *exp, const bn_t *modulus) {
    bn_t base_copy, exp_copy;
    bn_t temp;

    memcpy(&base_copy, base, sizeof(bn_t));
    memcpy(&exp_copy, exp, sizeof(bn_t));

    /* result = 1 */
    bn_zero(result);
    result->array[0] = 1;

    /* base = base mod modulus */
    bn_mod(&base_copy, &base_copy, modulus);

    /* Binary exponentiation */
    while (!bn_is_zero(&exp_copy)) {
        /* If exp is odd, multiply result by base */
        if (exp_copy.array[0] & 1) {
            bn_mul(&temp, result, &base_copy);
            bn_mod(result, &temp, modulus);
        }

        /* exp = exp / 2 */
        bn_rshift1(&exp_copy);

        /* base = base^2 mod modulus */
        bn_mul(&temp, &base_copy, &base_copy);
        bn_mod(&base_copy, &temp, modulus);
    }
}

#endif /* BIGNUM_H */
