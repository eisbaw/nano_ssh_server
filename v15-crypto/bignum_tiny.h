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
    uint32_t w[BN_WORDS];
} bn_t;

/* Initialize to zero */
static inline void bn_zero(bn_t *a) {
    memset(a->w, 0, sizeof(a->w));
}

/* Check if zero */
static inline int bn_is_zero(const bn_t *a) {
    for (int i = 0; i < BN_WORDS; i++) {
        if (a->w[i] != 0) return 0;
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
        a->w[word_idx] |= ((uint32_t)bytes[i]) << (byte_idx * 8);
    }
}

/* Convert to big-endian bytes */
static inline void bn_to_bytes(const bn_t *a, uint8_t *bytes, size_t len) {
    memset(bytes, 0, len);
    if (len > BN_BYTES) len = BN_BYTES;
    
    for (size_t i = 0; i < len; i++) {
        size_t word_idx = (BN_BYTES - 1 - i) / 4;
        size_t byte_idx = (BN_BYTES - 1 - i) % 4;
        bytes[i] = (a->w[word_idx] >> (byte_idx * 8)) & 0xFF;
    }
}

/* Compare: returns -1 if a < b, 0 if a == b, 1 if a > b */
static inline int bn_cmp(const bn_t *a, const bn_t *b) {
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (a->w[i] > b->w[i]) return 1;
        if (a->w[i] < b->w[i]) return -1;
    }
    return 0;
}

/* Addition: r = a + b (ignores overflow) */
static inline void bn_add(bn_t *r, const bn_t *a, const bn_t *b) {
    uint64_t carry = 0;
    for (int i = 0; i < BN_WORDS; i++) {
        uint64_t sum = (uint64_t)a->w[i] + (uint64_t)b->w[i] + carry;
        r->w[i] = (uint32_t)sum;
        carry = sum >> 32;
    }
}

/* Subtraction: r = a - b (assumes a >= b) */
static inline void bn_sub(bn_t *r, const bn_t *a, const bn_t *b) {
    uint64_t borrow = 0;
    for (int i = 0; i < BN_WORDS; i++) {
        uint64_t diff = (uint64_t)a->w[i] - (uint64_t)b->w[i] - borrow;
        r->w[i] = (uint32_t)diff;
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
        for (int j = 0; j < BN_WORDS - i; j++) {
            carry = mul_add_word(&temp.w[i + j], a->w[i], b->w[j], carry);
        }
    }
    
    *r = temp;
}

/* Left shift by 1 bit */
static inline void bn_shl1(bn_t *a) {
    uint32_t carry = 0;
    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t next_carry = a->w[i] >> 31;
        a->w[i] = (a->w[i] << 1) | carry;
        carry = next_carry;
    }
}

/* Right shift by 1 bit */
static inline void bn_shr1(bn_t *a) {
    uint32_t carry = 0;
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        uint32_t next_carry = a->w[i] & 1;
        a->w[i] = (a->w[i] >> 1) | (carry << 31);
        carry = next_carry;
    }
}

/* Modular reduction: r = a % m (simple repeated subtraction) */
static void bn_mod(bn_t *r, const bn_t *a, const bn_t *m) {
    *r = *a;
    
    /* Repeated subtraction until r < m */
    while (bn_cmp(r, m) >= 0) {
        bn_sub(r, r, m);
    }
}

/* Modular exponentiation: r = base^exp mod m (binary method) */
static void bn_modexp(bn_t *r, const bn_t *base, const bn_t *exp, const bn_t *mod) {
    bn_t result, temp_base, temp;
    
    /* result = 1 */
    bn_zero(&result);
    result.w[0] = 1;
    
    /* temp_base = base % mod */
    bn_mod(&temp_base, base, mod);
    
    /* Binary exponentiation (right-to-left) */
    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t exp_word = exp->w[i];
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
        }
    }
    
    *r = result;
}

#endif /* BIGNUM_TINY_H */
