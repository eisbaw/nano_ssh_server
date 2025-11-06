/*
 * Fixed bignum implementation with double-width multiplication
 * Version 2 - with simpler, working modular reduction
 */
#ifndef BIGNUM_FIXED_V2_H
#define BIGNUM_FIXED_V2_H

#include <stdint.h>
#include <string.h>

#define BN_WORDS 64    /* 2048 bits */
#define BN_BYTES 256
#define BN_2X_WORDS 128  /* 4096 bits for intermediate multiplication */

typedef struct {
    uint32_t array[BN_WORDS];
} bn_t;

typedef struct {
    uint32_t array[BN_2X_WORDS];  /* Double width for multiplication results */
} bn_2x_t;

/* Zero a bignum */
static inline void bn_zero(bn_t *a) {
    memset(a, 0, sizeof(bn_t));
}

static inline void bn_2x_zero(bn_2x_t *a) {
    memset(a, 0, sizeof(bn_2x_t));
}

/* Check if zero */
static inline int bn_is_zero(const bn_t *a) {
    for (int i = 0; i < BN_WORDS; i++) {
        if (a->array[i] != 0) return 0;
    }
    return 1;
}

/* Convert from big-endian bytes */
static inline void bn_from_bytes(bn_t *a, const uint8_t *bytes, size_t len) {
    bn_zero(a);
    if (len > BN_BYTES) len = BN_BYTES;

    for (size_t i = 0; i < len; i++) {
        int word_idx = (len - 1 - i) / 4;
        int byte_idx = (len - 1 - i) % 4;
        a->array[word_idx] |= ((uint32_t)bytes[i]) << (byte_idx * 8);
    }
}

/* Convert to big-endian bytes */
static inline void bn_to_bytes(const bn_t *a, uint8_t *bytes, size_t len) {
    memset(bytes, 0, len);
    if (len > BN_BYTES) len = BN_BYTES;

    for (size_t i = 0; i < len; i++) {
        int word_idx = (len - 1 - i) / 4;
        int byte_idx = (len - 1 - i) % 4;
        bytes[i] = (a->array[word_idx] >> (byte_idx * 8)) & 0xFF;
    }
}

/* Compare single-width bignums */
static int bn_cmp(const bn_t *a, const bn_t *b) {
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (a->array[i] < b->array[i]) return -1;
        if (a->array[i] > b->array[i]) return 1;
    }
    return 0;
}

/* Compare double-width with modulus (single-width) */
/* Returns: -1 if a < m, 0 if equal, 1 if a >= m */
static int bn_2x_cmp_mod(const bn_2x_t *a, const bn_t *m) {
    /* Check if any high words are non-zero */
    for (int i = BN_2X_WORDS - 1; i >= BN_WORDS; i--) {
        if (a->array[i] != 0) return 1;  /* a >= m */
    }
    /* Compare low BN_WORDS */
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (a->array[i] < m->array[i]) return -1;
        if (a->array[i] > m->array[i]) return 1;
    }
    return 0;
}

/* Subtraction: r = a - m (double-width minus single-width modulus) */
static void bn_2x_sub_mod(bn_2x_t *r, const bn_2x_t *a, const bn_t *m) {
    uint32_t borrow = 0;
    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t next_borrow = (a->array[i] < m->array[i] + borrow) ? 1 : 0;
        r->array[i] = a->array[i] - m->array[i] - borrow;
        borrow = next_borrow;
    }
    /* Propagate borrow through high words */
    for (int i = BN_WORDS; i < BN_2X_WORDS; i++) {
        uint32_t next_borrow = (a->array[i] < borrow) ? 1 : 0;
        r->array[i] = a->array[i] - borrow;
        borrow = next_borrow;
    }
}

/* Helper for multiplication */
static inline uint32_t mul_add_word(uint32_t *result, uint32_t a, uint32_t b, uint32_t carry) {
    uint64_t product = (uint64_t)a * b + *result + carry;
    *result = (uint32_t)product;
    return (uint32_t)(product >> 32);
}

/* Wide multiplication: r = a * b (produces 4096-bit result) */
static void bn_mul_wide(bn_2x_t *r, const bn_t *a, const bn_t *b) {
    bn_2x_zero(r);

    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t carry = 0;
        for (int j = 0; j < BN_WORDS; j++) {
            carry = mul_add_word(&r->array[i + j], a->array[i], b->array[j], carry);
        }
        if (i + BN_WORDS < BN_2X_WORDS) {
            r->array[i + BN_WORDS] = carry;
        }
    }
}

/* Modular reduction: r = a % m (double-width input, single-width output) */
/* Simple but correct: repeated subtraction */
static void bn_mod_wide(bn_t *r, const bn_2x_t *a, const bn_t *m) {
    bn_2x_t temp = *a;

    /* Repeated subtraction while temp >= m */
    /* For DH, a is at most (prime-1)^2, so we need at most (prime-1) subtractions */
    /* Prime is ~2^2047, so worst case is ~2^2047 iterations */
    /* This is too slow! Need better algorithm */

    /* Use a hybrid approach: */
    /* 1. Quickly reduce by subtracting m << k for large k */
    /* 2. Then use simple subtraction for final reduction */

    /* Find highest non-zero word in temp */
    int temp_high = -1;
    for (int i = BN_2X_WORDS - 1; i >= 0; i--) {
        if (temp.array[i] != 0) {
            temp_high = i;
            break;
        }
    }

    /* If temp is already < m (all high words zero and low words < m), done */
    if (temp_high < BN_WORDS) {
        /* Check if temp < m */
        int cmp = 0;
        for (int i = BN_WORDS - 1; i >= 0; i--) {
            if (temp.array[i] < m->array[i]) {
                cmp = -1;
                break;
            }
            if (temp.array[i] > m->array[i]) {
                cmp = 1;
                break;
            }
        }

        if (cmp < 0) {
            /* temp < m, just copy to result */
            bn_zero(r);
            for (int i = 0; i < BN_WORDS; i++) {
                r->array[i] = temp.array[i];
            }
            return;
        }
    }

    /* Reduce by subtracting (m << shift) where shift is chosen to make progress */
    /* Continue until temp < m */
    int iterations = 0;
    int max_iterations = 4096;  /* Safety limit to prevent infinite loops */

    while (iterations < max_iterations) {
        /* Check if temp < m */
        int cmp = bn_2x_cmp_mod(&temp, m);
        if (cmp < 0) break;  /* temp < m, done */

        /* Find shift amount: how many bits to shift m left */
        /* We want m << shift <= temp, so shift = (msb(temp) - msb(m)) */

        /* Find MSB of temp */
        int temp_msb_word = -1;
        for (int i = BN_2X_WORDS - 1; i >= 0; i--) {
            if (temp.array[i] != 0) {
                temp_msb_word = i;
                break;
            }
        }

        /* Find MSB of m */
        int m_msb_word = -1;
        for (int i = BN_WORDS - 1; i >= 0; i--) {
            if (m->array[i] != 0) {
                m_msb_word = i;
                break;
            }
        }

        if (temp_msb_word < 0 || m_msb_word < 0) break;

        int word_shift = temp_msb_word - m_msb_word;
        if (word_shift < 0) word_shift = 0;

        /* Create shifted version of m */
        bn_2x_t m_shifted;
        bn_2x_zero(&m_shifted);
        for (int i = 0; i < BN_WORDS && i + word_shift < BN_2X_WORDS; i++) {
            m_shifted.array[i + word_shift] = m->array[i];
        }

        /* Check if m_shifted <= temp */
        int can_subtract = 1;
        for (int i = BN_2X_WORDS - 1; i >= 0; i--) {
            if (temp.array[i] < m_shifted.array[i]) {
                can_subtract = 0;
                break;
            }
            if (temp.array[i] > m_shifted.array[i]) {
                break;
            }
        }

        if (can_subtract) {
            /* temp -= m_shifted */
            uint32_t borrow = 0;
            for (int i = 0; i < BN_2X_WORDS; i++) {
                uint32_t next_borrow = (temp.array[i] < m_shifted.array[i] + borrow) ? 1 : 0;
                temp.array[i] = temp.array[i] - m_shifted.array[i] - borrow;
                borrow = next_borrow;
            }
        } else if (word_shift > 0) {
            /* Try with word_shift - 1 */
            word_shift--;
            bn_2x_zero(&m_shifted);
            for (int i = 0; i < BN_WORDS && i + word_shift < BN_2X_WORDS; i++) {
                m_shifted.array[i + word_shift] = m->array[i];
            }

            /* temp -= m_shifted (should always succeed) */
            uint32_t borrow = 0;
            for (int i = 0; i < BN_2X_WORDS; i++) {
                uint32_t next_borrow = (temp.array[i] < m_shifted.array[i] + borrow) ? 1 : 0;
                temp.array[i] = temp.array[i] - m_shifted.array[i] - borrow;
                borrow = next_borrow;
            }
        } else {
            /* word_shift == 0 and can't subtract, shouldn't happen */
            break;
        }

        iterations++;
    }

    /* Copy low words to result */
    bn_zero(r);
    for (int i = 0; i < BN_WORDS; i++) {
        r->array[i] = temp.array[i];
    }

    /* Final cleanup: if result >= m, subtract m once more */
    if (bn_cmp(r, m) >= 0) {
        bn_t temp_result;
        uint32_t borrow = 0;
        for (int i = 0; i < BN_WORDS; i++) {
            uint32_t next_borrow = (r->array[i] < m->array[i] + borrow) ? 1 : 0;
            temp_result.array[i] = r->array[i] - m->array[i] - borrow;
            borrow = next_borrow;
        }
        *r = temp_result;
    }
}

/* Modular multiplication: r = (a * b) mod m */
static void bn_mulmod(bn_t *r, const bn_t *a, const bn_t *b, const bn_t *m) {
    bn_2x_t product;
    bn_mul_wide(&product, a, b);
    bn_mod_wide(r, &product, m);
}

/* Modular exponentiation: r = base^exp mod m */
static void bn_modexp(bn_t *r, const bn_t *base, const bn_t *exp, const bn_t *mod) {
    bn_t result, temp_base;

    bn_zero(&result);
    result.array[0] = 1;

    /* temp_base = base % mod */
    bn_2x_t base_wide;
    bn_2x_zero(&base_wide);
    for (int i = 0; i < BN_WORDS; i++) {
        base_wide.array[i] = base->array[i];
    }
    bn_mod_wide(&temp_base, &base_wide, mod);

    /* Binary exponentiation */
    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t exp_word = exp->array[i];
        for (int j = 0; j < 32; j++) {
            if (exp_word & 1) {
                bn_mulmod(&result, &result, &temp_base, mod);
            }

            bn_mulmod(&temp_base, &temp_base, &temp_base, mod);
            exp_word >>= 1;
        }
    }

    *r = result;
}

#endif /* BIGNUM_FIXED_V2_H */
