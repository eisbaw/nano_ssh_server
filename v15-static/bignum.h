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

/* Static temporary variables to reduce stack usage
 * Using static vars prevents stack overflow in deep recursion
 * Trade-off: Not thread-safe, but we're single-threaded
 */
static bn_t bn_temp1, bn_temp2, bn_temp3, bn_temp4;

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

/* Addition: c = a + b, returns 1 if overflow (carry out) */
static inline int bn_add(bn_t *c, const bn_t *a, const bn_t *b) {
    uint64_t carry = 0;

    for (int i = 0; i < BN_ARRAY_SIZE; i++) {
        uint64_t sum = (uint64_t)a->array[i] + (uint64_t)b->array[i] + carry;
        c->array[i] = (uint32_t)sum;
        carry = sum >> 32;
    }

    return (int)carry;
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

/* Helper: Get bit length of bignum (position of highest set bit + 1) */
static inline int bn_bitlen(const bn_t *n) {
    int i;

    /* Find highest non-zero word */
    for (i = BN_ARRAY_SIZE - 1; i >= 0; i--) {
        if (n->array[i] != 0) break;
    }

    if (i < 0) return 0;  /* All zeros */

    /* Find highest set bit in that word */
    uint32_t word = n->array[i];
    int bits = 0;
    while (word) {
        word >>= 1;
        bits++;
    }

    return i * 32 + bits;
}

/* Helper: Left shift by N bits */
static inline void bn_lshift_n(bn_t *r, const bn_t *a, int n) {
    if (n == 0) {
        memcpy(r, a, sizeof(bn_t));
        return;
    }

    int word_shift = n / 32;
    int bit_shift = n % 32;

    bn_zero(r);

    if (bit_shift == 0) {
        /* Just word shift */
        for (int i = word_shift; i < BN_ARRAY_SIZE; i++) {
            r->array[i] = a->array[i - word_shift];
        }
    } else {
        /* Word shift + bit shift */
        for (int i = word_shift; i < BN_ARRAY_SIZE; i++) {
            r->array[i] = a->array[i - word_shift] << bit_shift;
            if (i - word_shift > 0) {
                r->array[i] |= a->array[i - word_shift - 1] >> (32 - bit_shift);
            }
        }
    }
}

/* Modulo: r = a mod m (using binary long division)
 * Much faster than repeated subtraction for large numbers
 * Uses static temps to reduce stack usage
 */
static inline void bn_mod(bn_t *r, const bn_t *a, const bn_t *m) {
    /* Handle trivial cases */
    if (bn_cmp(a, m) < 0) {
        if (r != a) {  /* Avoid self-copy */
            memcpy(r, a, sizeof(bn_t));
        }
        return;
    }

    int a_bits = bn_bitlen(a);
    int m_bits = bn_bitlen(m);

    if (m_bits == 0) {
        /* Division by zero - just return 0 */
        bn_zero(r);
        return;
    }

    /* Use static temps instead of stack allocation */
    if (a != &bn_temp1) {  /* Avoid self-copy UB */
        memcpy(&bn_temp1, a, sizeof(bn_t));  /* remainder = a */
    }

    /* Binary long division */
    int shift = a_bits - m_bits;

    while (shift >= 0) {
        /* bn_temp2 = m << shift */
        bn_lshift_n(&bn_temp2, m, shift);

        /* If remainder >= divisor, subtract */
        if (bn_cmp(&bn_temp1, &bn_temp2) >= 0) {
            bn_sub(&bn_temp1, &bn_temp1, &bn_temp2);
        }

        shift--;
    }

    if (r != &bn_temp1) {  /* Avoid self-copy UB */
        memcpy(r, &bn_temp1, sizeof(bn_t));
    }
}

/* Modular multiplication: r = (a * b) mod m
 * Avoids overflow by using repeated doubling and addition
 * Handles 2048-bit overflow correctly when temp_a * 2 >= 2^2048
 * Uses static temps to reduce stack usage
 * Note: bn_temp3 for result, bn_temp4 for temp_a (bn_mod uses bn_temp1/2)
 */
static inline void bn_mulmod(bn_t *r, const bn_t *a, const bn_t *b, const bn_t *m) {
    bn_t temp_b, m_complement;  /* temp_b for loop counter, m_complement for overflow handling */
    int overflow;

    /* Compute m_complement = 2^2048 - m = ~m + 1 (for handling overflow) */
    for (int i = 0; i < BN_ARRAY_SIZE; i++) {
        m_complement.array[i] = ~m->array[i];
    }
    /* Add 1 using manual carry propagation */
    uint64_t carry = 1;
    for (int i = 0; i < BN_ARRAY_SIZE; i++) {
        uint64_t sum = (uint64_t)m_complement.array[i] + carry;
        m_complement.array[i] = (uint32_t)sum;
        carry = sum >> 32;
    }

    bn_zero(&bn_temp3);  /* result = 0 */
    bn_mod(&bn_temp4, a, m);  /* temp_a = a mod m */
    memcpy(&temp_b, b, sizeof(bn_t));

    /* Binary multiplication with modular reduction
     * result = sum of (a * 2^i) for each bit i set in b
     */
    while (!bn_is_zero(&temp_b)) {
        /* If bit 0 of b is set, add a to result */
        if (temp_b.array[0] & 1) {
            overflow = bn_add(&bn_temp3, &bn_temp3, &bn_temp4);
            if (overflow) {
                /* result + temp_a overflowed, add m_complement to get correct mod */
                bn_add(&bn_temp3, &bn_temp3, &m_complement);
            }
            /* Reduce if needed */
            if (bn_cmp(&bn_temp3, m) >= 0) {
                bn_sub(&bn_temp3, &bn_temp3, m);
            }
        }

        /* temp_a = (temp_a * 2) mod m */
        overflow = bn_add(&bn_temp1, &bn_temp4, &bn_temp4);  /* bn_temp1 = temp_a * 2 */
        if (overflow) {
            /* temp_a * 2 overflowed 2048 bits, add m_complement to get correct mod */
            bn_add(&bn_temp1, &bn_temp1, &m_complement);
        }
        /* Reduce if needed */
        if (bn_cmp(&bn_temp1, m) >= 0) {
            bn_sub(&bn_temp1, &bn_temp1, m);
        }
        memcpy(&bn_temp4, &bn_temp1, sizeof(bn_t));

        /* b = b / 2 */
        bn_rshift1(&temp_b);
    }

    memcpy(r, &bn_temp3, sizeof(bn_t));
}

/* Modular exponentiation: result = base^exp mod modulus
 * Using binary exponentiation (right-to-left)
 * This is the most important function for DH and RSA
 */
static inline void bn_modexp(bn_t *result, const bn_t *base, const bn_t *exp, const bn_t *modulus) {
    bn_t base_copy, exp_copy;

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
            bn_mulmod(result, result, &base_copy, modulus);
        }

        /* exp = exp / 2 */
        bn_rshift1(&exp_copy);

        /* base = base^2 mod modulus */
        bn_mulmod(&base_copy, &base_copy, &base_copy, modulus);
    }
}

#endif /* BIGNUM_H */
