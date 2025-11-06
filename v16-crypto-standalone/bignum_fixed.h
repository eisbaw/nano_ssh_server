/*
 * Fixed bignum implementation with double-width multiplication
 * This is the CORRECT way to do modular arithmetic with large numbers
 */
#ifndef BIGNUM_FIXED_H
#define BIGNUM_FIXED_H

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

static inline int bn_2x_is_zero(const bn_2x_t *a) {
    for (int i = 0; i < BN_2X_WORDS; i++) {
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

/* Compare: returns -1 if a < b, 0 if a == b, 1 if a > b */
static int bn_cmp(const bn_t *a, const bn_t *b) {
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (a->array[i] < b->array[i]) return -1;
        if (a->array[i] > b->array[i]) return 1;
    }
    return 0;
}

/* Compare double-width with single-width (for reduction) */
static int bn_2x_cmp_1x(const bn_2x_t *a, const bn_t *b) {
    /* Check if a has any bits set above BN_WORDS */
    for (int i = BN_2X_WORDS - 1; i >= BN_WORDS; i--) {
        if (a->array[i] != 0) return 1;  /* a > b */
    }
    /* Compare low BN_WORDS words */
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (a->array[i] < b->array[i]) return -1;
        if (a->array[i] > b->array[i]) return 1;
    }
    return 0;
}

/* Shift left by 1 bit */
static void bn_shl1(bn_t *r, const bn_t *a) {
    uint32_t carry = 0;
    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t next_carry = a->array[i] >> 31;
        r->array[i] = (a->array[i] << 1) | carry;
        carry = next_carry;
    }
}

/* Subtraction: r = a - b (assumes a >= b) */
static void bn_sub(bn_t *r, const bn_t *a, const bn_t *b) {
    uint32_t borrow = 0;
    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t next_borrow = (a->array[i] < b->array[i] + borrow) ? 1 : 0;
        r->array[i] = a->array[i] - b->array[i] - borrow;
        borrow = next_borrow;
    }
}

/* Subtraction double-width from single-width: r = a - b */
static void bn_2x_sub_1x(bn_2x_t *r, const bn_2x_t *a, const bn_t *b) {
    uint32_t borrow = 0;
    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t next_borrow = (a->array[i] < b->array[i] + borrow) ? 1 : 0;
        r->array[i] = a->array[i] - b->array[i] - borrow;
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

/* Multiplication: r = a * b (DOUBLE-WIDTH OUTPUT) */
static void bn_mul_wide(bn_2x_t *r, const bn_t *a, const bn_t *b) {
    bn_2x_zero(r);

    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t carry = 0;
        for (int j = 0; j < BN_WORDS; j++) {
            /* i + j is always < BN_2X_WORDS since i,j < BN_WORDS */
            carry = mul_add_word(&r->array[i + j], a->array[i], b->array[j], carry);
        }
        /* Store final carry */
        if (i + BN_WORDS < BN_2X_WORDS) {
            r->array[i + BN_WORDS] = carry;
        }
    }
}

/* Find MSB position */
static int bn_msb(const bn_t *a) {
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (a->array[i] != 0) {
            uint32_t word = a->array[i];
            for (int j = 31; j >= 0; j--) {
                if (word & (1U << j)) {
                    return i * 32 + j;
                }
            }
        }
    }
    return -1;
}

static int bn_2x_msb(const bn_2x_t *a) {
    for (int i = BN_2X_WORDS - 1; i >= 0; i--) {
        if (a->array[i] != 0) {
            uint32_t word = a->array[i];
            for (int j = 31; j >= 0; j--) {
                if (word & (1U << j)) {
                    return i * 32 + j;
                }
            }
        }
    }
    return -1;
}

/* Shift left double-width by multiple bits */
static void bn_2x_shl(bn_2x_t *r, const bn_2x_t *a, int bits) {
    if (bits == 0) {
        *r = *a;
        return;
    }
    if (bits >= BN_2X_WORDS * 32) {
        bn_2x_zero(r);
        return;
    }

    int word_shift = bits / 32;
    int bit_shift = bits % 32;

    bn_2x_zero(r);

    if (bit_shift == 0) {
        for (int i = word_shift; i < BN_2X_WORDS; i++) {
            r->array[i] = a->array[i - word_shift];
        }
    } else {
        uint32_t carry = 0;
        for (int i = 0; i < BN_2X_WORDS - word_shift; i++) {
            uint32_t val = a->array[i];
            r->array[i + word_shift] = (val << bit_shift) | carry;
            carry = val >> (32 - bit_shift);
        }
        int final_word = BN_2X_WORDS - word_shift;
        if (final_word < BN_2X_WORDS) {
            r->array[final_word] = carry;
        }
    }
}

/* Modular reduction: r = a % m (double-width input, single-width output) */
static void bn_mod_wide(bn_t *r, const bn_2x_t *a, const bn_t *m) {
    bn_2x_t temp = *a;

    /* Binary long division */
    while (1) {
        int msb_temp = bn_2x_msb(&temp);
        int msb_m = bn_msb(m);

        if (msb_temp < 0 || msb_m < 0) break;
        if (msb_temp < msb_m) break;

        int shift = msb_temp - msb_m;

        /* shifted_m = m << shift */
        bn_2x_t shifted_m;
        bn_2x_zero(&shifted_m);
        /* Copy m into low words of shifted_m */
        for (int i = 0; i < BN_WORDS; i++) {
            shifted_m.array[i] = m->array[i];
        }
        bn_2x_t shifted_temp;
        bn_2x_shl(&shifted_temp, &shifted_m, shift);
        shifted_m = shifted_temp;

        /* Subtract if temp >= shifted_m */
        if (bn_2x_cmp_1x(&shifted_m, m) >= 0 || shift == 0) {
            /* Can't use bn_2x_cmp_1x here, need proper 2x comparison */
            /* Simplified: just try to subtract */
            bn_2x_t temp_result;
            uint32_t borrow = 0;
            int underflow = 0;
            for (int i = 0; i < BN_2X_WORDS; i++) {
                if (temp.array[i] < shifted_m.array[i] + borrow) {
                    if (i == BN_2X_WORDS - 1 && temp.array[i] - shifted_m.array[i] - borrow > temp.array[i]) {
                        underflow = 1;
                        break;
                    }
                }
                uint32_t next_borrow = (temp.array[i] < shifted_m.array[i] + borrow) ? 1 : 0;
                temp_result.array[i] = temp.array[i] - shifted_m.array[i] - borrow;
                borrow = next_borrow;
            }

            if (!underflow && borrow == 0) {
                temp = temp_result;
            } else if (shift > 0) {
                /* Try with shift-1 */
                bn_2x_shl(&shifted_m, &shifted_temp, shift - 1);
                borrow = 0;
                underflow = 0;
                for (int i = 0; i < BN_2X_WORDS; i++) {
                    uint32_t next_borrow = (temp.array[i] < shifted_m.array[i] + borrow) ? 1 : 0;
                    temp_result.array[i] = temp.array[i] - shifted_m.array[i] - borrow;
                    borrow = next_borrow;
                }
                if (!underflow && borrow == 0) {
                    temp = temp_result;
                }
            } else {
                break;
            }
        }
    }

    /* Copy low words to result */
    bn_zero(r);
    for (int i = 0; i < BN_WORDS; i++) {
        r->array[i] = temp.array[i];
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

#endif /* BIGNUM_FIXED_H */
