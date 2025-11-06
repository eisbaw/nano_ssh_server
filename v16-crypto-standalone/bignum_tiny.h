/* bignum_tiny.h - Minimal 2048-bit bignum library
 * Goal: ~500-800 bytes code size (vs 2-3KB for external library)
 * Features: Fixed 2048-bit arithmetic, simple algorithms, size-optimized
 */

#ifndef BIGNUM_TINY_H
#define BIGNUM_TINY_H

#include <stdint.h>
#include <string.h>

#define BN_WORDS 64  /* 64 × 32 = 2048 bits */
#define BN_BYTES 256 /* 2048 / 8 */

typedef struct {
    uint32_t array[BN_WORDS];
} bn_t;

/* Initialize to zero */
static inline void bn_zero(bn_t *a) {
    memset(a->array, 0, sizeof(a->array));
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
        size_t word_idx = (BN_BYTES - 1 - i) / 4;
        size_t byte_idx = (BN_BYTES - 1 - i) % 4;
        a->array[word_idx] |= ((uint32_t)bytes[i]) << (byte_idx * 8);
    }
}

/* Convert to big-endian bytes */
static inline void bn_to_bytes(const bn_t *a, uint8_t *bytes, size_t len) {
    memset(bytes, 0, len);
    if (len > BN_BYTES) len = BN_BYTES;
    
    for (size_t i = 0; i < len; i++) {
        size_t word_idx = (BN_BYTES - 1 - i) / 4;
        size_t byte_idx = (BN_BYTES - 1 - i) % 4;
        bytes[i] = (a->array[word_idx] >> (byte_idx * 8)) & 0xFF;
    }
}

/* Compare: returns -1 if a < b, 0 if a == b, 1 if a > b */
static inline int bn_cmp(const bn_t *a, const bn_t *b) {
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (a->array[i] > b->array[i]) return 1;
        if (a->array[i] < b->array[i]) return -1;
    }
    return 0;
}

/* Addition: r = a + b (ignores overflow) */
static inline void bn_add(bn_t *r, const bn_t *a, const bn_t *b) {
    uint64_t carry = 0;
    for (int i = 0; i < BN_WORDS; i++) {
        uint64_t sum = (uint64_t)a->array[i] + (uint64_t)b->array[i] + carry;
        r->array[i] = (uint32_t)sum;
        carry = sum >> 32;
    }
}

/* Subtraction: r = a - b (assumes a >= b) */
static inline void bn_sub(bn_t *r, const bn_t *a, const bn_t *b) {
    uint64_t borrow = 0;
    for (int i = 0; i < BN_WORDS; i++) {
        uint64_t diff = (uint64_t)a->array[i] - (uint64_t)b->array[i] - borrow;
        r->array[i] = (uint32_t)diff;
        borrow = (diff >> 63) & 1;
    }
}

/* Helper: multiply-add word - result += a * b + carry, return new carry */
static inline uint32_t mul_add_word(uint32_t *result, uint32_t a, uint32_t b, uint32_t carry) {
    uint64_t prod = (uint64_t)a * (uint64_t)b + (uint64_t)carry + (uint64_t)(*result);
    *result = (uint32_t)prod;
    return (uint32_t)(prod >> 32);
}

/* Multiplication: r = a * b (schoolbook algorithm) */
static void bn_mul(bn_t *r, const bn_t *a, const bn_t *b) {
    bn_t temp;
    bn_zero(&temp);

    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t carry = 0;
        for (int j = 0; j < BN_WORDS && i + j < BN_WORDS; j++) {
            carry = mul_add_word(&temp.array[i + j], a->array[i], b->array[j], carry);
        }
    }

    *r = temp;
}

/* Left shift by 1 bit */
static inline void bn_shl1(bn_t *a) {
    uint32_t carry = 0;
    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t next_carry = a->array[i] >> 31;
        a->array[i] = (a->array[i] << 1) | carry;
        carry = next_carry;
    }
}

/* Right shift by 1 bit */
static inline void bn_shr1(bn_t *a) {
    uint32_t carry = 0;
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        uint32_t next_carry = a->array[i] & 1;
        a->array[i] = (a->array[i] >> 1) | (carry << 31);
        carry = next_carry;
    }
}

/* Find most significant bit position (returns -1 if zero) */
static inline int bn_msb(const bn_t *a) {
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (a->array[i] != 0) {
            uint32_t word = a->array[i];
            int bit = i * 32;
            /* Find highest bit in word */
            for (int j = 31; j >= 0; j--) {
                if (word & (1U << j)) {
                    return bit + j;
                }
            }
        }
    }
    return -1;
}

/* Shift left by multiple bits efficiently (word-aligned) */
static void bn_shl(bn_t *r, const bn_t *a, int bits) {
    if (bits == 0) {
        *r = *a;
        return;
    }
    if (bits >= BN_WORDS * 32) {
        bn_zero(r);
        return;
    }

    int word_shift = bits / 32;
    int bit_shift = bits % 32;

    bn_zero(r);

    if (bit_shift == 0) {
        /* Pure word shift */
        for (int i = word_shift; i < BN_WORDS; i++) {
            r->array[i] = a->array[i - word_shift];
        }
    } else {
        /* Word shift + bit shift */
        uint32_t carry = 0;
        int i;
        for (i = 0; i < BN_WORDS - word_shift; i++) {
            uint32_t val = a->array[i];
            r->array[i + word_shift] = (val << bit_shift) | carry;
            carry = val >> (32 - bit_shift);
        }
        /* Store final carry in next position if it fits */
        if (i + word_shift < BN_WORDS) {
            r->array[i + word_shift] = carry;
        }
    }
}

/* Modular reduction: r = a % m (simple repeated subtraction with iteration limit) */
static void bn_mod(bn_t *r, const bn_t *a, const bn_t *m) {
    *r = *a;

    /* Fast path: if a < m, done */
    if (bn_cmp(r, m) < 0) return;

    /* For DH with 2048-bit numbers, result should be < prime after at most
     * 2^2048 / prime iterations. But we limit to reasonable amount. */
    int max_iterations = 4096;  /* Safety limit */
    int iterations = 0;

    /* Repeated subtraction until r < m */
    while (bn_cmp(r, m) >= 0 && iterations < max_iterations) {
        bn_sub(r, r, m);
        iterations++;
    }

    /* If we hit the limit, something is wrong - try binary method */
    if (iterations >= max_iterations) {
        /* Binary long division: shift m left, subtract when possible */
        while (1) {
            int msb_r = bn_msb(r);
            int msb_m = bn_msb(m);

            if (msb_r < 0 || msb_m < 0) return;
            if (msb_r < msb_m) return;

            int shift = msb_r - msb_m;
            bn_t shifted_m;
            bn_shl(&shifted_m, m, shift);

            if (bn_cmp(r, &shifted_m) >= 0) {
                bn_sub(r, r, &shifted_m);
            } else if (shift > 0) {
                shift--;
                bn_shl(&shifted_m, m, shift);
                if (bn_cmp(r, &shifted_m) >= 0) {
                    bn_sub(r, r, &shifted_m);
                }
            } else {
                break;
            }
        }
    }
}

/* Modular exponentiation: r = base^exp mod m (binary method) */
static void bn_modexp(bn_t *r, const bn_t *base, const bn_t *exp, const bn_t *mod) {
    bn_t result, temp_base, temp;

    /* Initialize all variables */
    bn_zero(&result);
    bn_zero(&temp_base);
    bn_zero(&temp);

    /* result = 1 */
    result.array[0] = 1;

    /* temp_base = base % mod */
    bn_mod(&temp_base, base, mod);

    /* Binary exponentiation (right-to-left) */
    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t exp_word = exp->array[i];
        for (int j = 0; j < 32; j++) {
            if (exp_word & 1) {
                /* result = (result * temp_base) % mod */
                bn_mul(&temp, &result, &temp_base);
                bn_mod(&result, &temp, mod);
            }

            /* temp_base = (temp_base * temp_base) % mod */
            bn_mul(&temp, &temp_base, &temp_base);
            bn_mod(&temp_base, &temp, mod);

            exp_word >>= 1;

            /* Early exit if result becomes 0 (shouldn't happen but check) */
            if (bn_is_zero(&result)) {
                return; /* Result is 0, no point continuing */
            }
        }
    }

    *r = result;
}

#endif /* BIGNUM_TINY_H */
