/*
 * Minimal Big Integer Arithmetic for RSA and DH
 * Public domain implementation
 */

#ifndef BIGINT_H
#define BIGINT_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* Use 256-bit limbs (32 x 64-bit words = 2048 bits) */
#define BIGINT_WORDS 32
typedef uint64_t bigint_word_t;

typedef struct {
    bigint_word_t d[BIGINT_WORDS];
} bigint_t;

/* Zero a bigint */
static inline void bigint_zero(bigint_t *a) {
    memset(a->d, 0, sizeof(a->d));
}

/* Copy bigint */
static inline void bigint_copy(bigint_t *dest, const bigint_t *src) {
    memcpy(dest->d, src->d, sizeof(dest->d));
}

/* Set from uint64 */
static inline void bigint_set_u64(bigint_t *a, uint64_t val) {
    bigint_zero(a);
    a->d[0] = val;
}

/* Compare: returns <0 if a<b, 0 if a==b, >0 if a>b */
static int bigint_cmp(const bigint_t *a, const bigint_t *b) {
    for (int i = BIGINT_WORDS - 1; i >= 0; i--) {
        if (a->d[i] < b->d[i]) return -1;
        if (a->d[i] > b->d[i]) return 1;
    }
    return 0;
}

/* Add: c = a + b, returns carry */
static int bigint_add(bigint_t *c, const bigint_t *a, const bigint_t *b) {
    uint64_t carry = 0;
    for (int i = 0; i < BIGINT_WORDS; i++) {
        __uint128_t sum = (__uint128_t)a->d[i] + b->d[i] + carry;
        c->d[i] = (uint64_t)sum;
        carry = sum >> 64;
    }
    return carry;
}

/* Subtract: c = a - b, returns borrow */
static int bigint_sub(bigint_t *c, const bigint_t *a, const bigint_t *b) {
    uint64_t borrow = 0;
    for (int i = 0; i < BIGINT_WORDS; i++) {
        __uint128_t diff = (__uint128_t)a->d[i] - b->d[i] - borrow;
        c->d[i] = (uint64_t)diff;
        borrow = (diff >> 64) & 1;
    }
    return borrow;
}

/* Left shift by 1 bit */
static void bigint_shl1(bigint_t *a) {
    uint64_t carry = 0;
    for (int i = 0; i < BIGINT_WORDS; i++) {
        uint64_t new_carry = a->d[i] >> 63;
        a->d[i] = (a->d[i] << 1) | carry;
        carry = new_carry;
    }
}

/* Right shift by 1 bit */
static void bigint_shr1(bigint_t *a) {
    uint64_t carry = 0;
    for (int i = BIGINT_WORDS - 1; i >= 0; i--) {
        uint64_t new_carry = a->d[i] & 1;
        a->d[i] = (a->d[i] >> 1) | (carry << 63);
        carry = new_carry;
    }
}

/* Test if zero */
static int bigint_is_zero(const bigint_t *a) {
    for (int i = 0; i < BIGINT_WORDS; i++) {
        if (a->d[i] != 0) return 0;
    }
    return 1;
}

/* Test if odd */
static inline int bigint_is_odd(const bigint_t *a) {
    return a->d[0] & 1;
}

/* Multiply: c = a * b (simple schoolbook multiplication) */
static void bigint_mul(bigint_t *c, const bigint_t *a, const bigint_t *b) {
    bigint_t result;
    bigint_zero(&result);

    for (int i = 0; i < BIGINT_WORDS; i++) {
        uint64_t carry = 0;
        for (int j = 0; j < BIGINT_WORDS - i; j++) {
            __uint128_t prod = (__uint128_t)a->d[i] * b->d[j] + result.d[i + j] + carry;
            result.d[i + j] = (uint64_t)prod;
            carry = prod >> 64;
        }
    }

    bigint_copy(c, &result);
}

/* Modular reduction: r = a mod m (simple division) */
static void bigint_mod(bigint_t *r, const bigint_t *a, const bigint_t *m) {
    bigint_t remainder, temp;
    bigint_copy(&remainder, a);

    /* Simple repeated subtraction for small values */
    while (bigint_cmp(&remainder, m) >= 0) {
        bigint_sub(&temp, &remainder, m);
        bigint_copy(&remainder, &temp);
    }

    bigint_copy(r, &remainder);
}

/* Modular exponentiation: result = base^exp mod m (binary method) */
static void bigint_modexp(bigint_t *result, const bigint_t *base, const bigint_t *exp, const bigint_t *m) {
    bigint_t res, b, e;
    bigint_set_u64(&res, 1);
    bigint_copy(&b, base);
    bigint_copy(&e, exp);

    while (!bigint_is_zero(&e)) {
        if (bigint_is_odd(&e)) {
            bigint_t temp;
            bigint_mul(&temp, &res, &b);
            bigint_mod(&res, &temp, m);
        }

        bigint_t temp;
        bigint_mul(&temp, &b, &b);
        bigint_mod(&b, &temp, m);

        bigint_shr1(&e);
    }

    bigint_copy(result, &res);
}

/* Import from bytes (big-endian) */
static void bigint_from_bytes(bigint_t *a, const uint8_t *bytes, int len) {
    bigint_zero(a);
    int word_idx = 0;
    int byte_in_word = 0;

    for (int i = len - 1; i >= 0; i--) {
        a->d[word_idx] |= ((uint64_t)bytes[i]) << (byte_in_word * 8);
        byte_in_word++;
        if (byte_in_word == 8) {
            byte_in_word = 0;
            word_idx++;
            if (word_idx >= BIGINT_WORDS) break;
        }
    }
}

/* Export to bytes (big-endian) */
static void bigint_to_bytes(uint8_t *bytes, int len, const bigint_t *a) {
    memset(bytes, 0, len);
    int word_idx = 0;
    int byte_in_word = 0;

    for (int i = len - 1; i >= 0; i--) {
        if (word_idx < BIGINT_WORDS) {
            bytes[i] = (a->d[word_idx] >> (byte_in_word * 8)) & 0xFF;
        }
        byte_in_word++;
        if (byte_in_word == 8) {
            byte_in_word = 0;
            word_idx++;
        }
    }
}

#endif /* BIGINT_H */
