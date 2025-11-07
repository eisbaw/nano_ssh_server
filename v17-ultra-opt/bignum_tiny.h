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

/* Double-width bignum for intermediate multiplication results */
typedef struct {
    uint32_t array[BN_WORDS * 2];  /* 4096 bits */
} bn_double_t;

/* Multiplication: r_double = a * b (full width result) */
static void bn_mul_full(bn_double_t *r, const bn_t *a, const bn_t *b) {
    /* Zero result */
    for (int i = 0; i < BN_WORDS * 2; i++) {
        r->array[i] = 0;
    }

    /* Schoolbook multiplication with full width */
    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t carry = 0;
        for (int j = 0; j < BN_WORDS; j++) {
            carry = mul_add_word(&r->array[i + j], a->array[i], b->array[j], carry);
        }
        /* Store final carry */
        r->array[i + BN_WORDS] = carry;
    }
}

/* Modular reduction of double-width number: r = a_double % m */
/* Uses decomposition: (H * 2^k + L) mod m where k = BN_WORDS * 32 */
static void bn_mod_double(bn_t *r, const bn_double_t *a, const bn_t *m) {
    bn_t low, high, temp1, temp2;
    bn_t power;  /* Will hold 2^k mod m where k = BN_WORDS * 32 */

    /* Extract low and high parts */
    for (int i = 0; i < BN_WORDS; i++) {
        low.array[i] = a->array[i];
        high.array[i] = a->array[BN_WORDS + i];
    }

    /* Check if high part is zero - common case */
    if (bn_is_zero(&high)) {
        bn_mod(r, &low, m);
        return;
    }

    /* Compute 2^(BN_WORDS * 32) mod m using repeated squaring */
    /* Start with 2^1 */
    bn_zero(&power);
    power.array[0] = 2;

    /* Square it (BN_WORDS * 32 - 1) times to get 2^(2^(BN_WORDS*32)) */
    /* Actually, we want 2^(BN_WORDS * 32) = 2^2048 */
    /* Start with 2, square to get 2^2, square to get 2^4, etc. */
    /* After k squarings we have 2^(2^k) */
    /* We want 2^2048, so we need to compute it differently */

    /* Instead: compute (2^32)^BN_WORDS mod m */
    /* 2^32 = 0x100000000, which is word[1] = 1 */
    bn_zero(&power);
    power.array[1] = 1;  /* 2^32 */
    bn_mod(&power, &power, m);  /* Reduce it */

    /* Now raise power to the BN_WORDS power using exponentiation */
    /* Actually this is getting too complex. Let me use a simpler approach: */
    /* Compute result = 0, then for each word in high, add (word * 2^(k*32)) mod m */

    bn_zero(r);

    for (int i = 0; i < BN_WORDS; i++) {
        if (high.array[i] == 0) continue;

        /* Compute 2^((BN_WORDS + i) * 32) mod m */
        /* This is 2 raised to the power (BN_WORDS + i) * 32 */
        bn_zero(&temp1);
        temp1.array[0] = 1;  /* Start with 1 */

        /* Multiply by 2, (BN_WORDS + i) * 32 times, reducing each time */
        for (int bit = 0; bit < (BN_WORDS + i) * 32; bit++) {
            bn_add(&temp2, &temp1, &temp1);  /* temp2 = temp1 * 2 */
            bn_mod(&temp1, &temp2, m);       /* temp1 = temp2 mod m */
        }

        /* Now temp1 = 2^((BN_WORDS + i) * 32) mod m */
        /* Multiply by high.array[i] */
        for (uint32_t j = 0; j < high.array[i]; j++) {
            bn_add(&temp2, r, &temp1);
            bn_mod(r, &temp2, m);
        }
    }

    /* Add low part */
    bn_add(&temp1, r, &low);
    bn_mod(r, &temp1, m);
}

/* Modular multiplication: r = (a * b) mod m using double-width */
static void bn_mulmod(bn_t *r, const bn_t *a, const bn_t *b, const bn_t *m) {
    bn_double_t temp;
    bn_mul_full(&temp, a, b);
    bn_mod_double(r, &temp, m);
}

/* Multiplication: r = a * b (truncated to BN_WORDS) */
static void bn_mul(bn_t *r, const bn_t *a, const bn_t *b) {
    bn_t temp;
    bn_zero(&temp);

    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t carry = 0;
        for (int j = 0; j < BN_WORDS; j++) {
            if (i + j < BN_WORDS) {
                carry = mul_add_word(&temp.array[i + j], a->array[i], b->array[j], carry);
            }
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
    bn_t result, temp_base;

    /* result = 1 */
    bn_zero(&result);
    result.array[0] = 1;

    /* temp_base = base % mod */
    bn_mod(&temp_base, base, mod);

    /* Check if exponent is zero */
    if (bn_is_zero(exp)) {
        *r = result;  /* base^0 = 1 */
        return;
    }

    /* Binary exponentiation (right-to-left) */
    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t exp_word = exp->array[i];

        for (int j = 0; j < 32; j++) {
            if (exp_word & 1) {
                /* result = (result * temp_base) % mod */
                bn_mulmod(&result, &result, &temp_base, mod);
            }

            exp_word >>= 1;

            /* Only square if not the last bit of the last word */
            if (i < BN_WORDS - 1 || j < 31) {
                /* temp_base = (temp_base * temp_base) % mod */
                bn_mulmod(&temp_base, &temp_base, &temp_base, mod);
            }
        }
    }

    *r = result;
}

#endif /* BIGNUM_TINY_H */
