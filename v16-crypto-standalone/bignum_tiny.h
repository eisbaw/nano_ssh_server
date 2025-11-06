/*
 * Montgomery multiplication for fast modular arithmetic
 *
 * Key idea: Work in "Montgomery form" where reduction is cheap.
 * - Convert:  a' = a * R mod m (where R = 2^2048)
 * - Multiply: (a' * b') / R mod m  [reduction is just shift + subtract]
 * - Convert back: a = a' / R mod m
 *
 * This makes each modular multiplication O(n) instead of O(n²).
 */
#ifndef BIGNUM_MONTGOMERY_H
#define BIGNUM_MONTGOMERY_H

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

/* Montgomery context */
typedef struct {
    bn_t m;           /* Modulus */
    uint32_t m_inv;   /* -m^(-1) mod 2^32 */
    bn_t r2;          /* R^2 mod m, where R = 2^(BN_WORDS * 32) */
} mont_ctx_t;

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

static inline uint32_t mul_add_word(uint32_t *result, uint32_t a, uint32_t b, uint32_t carry) {
    uint64_t product = (uint64_t)a * b + *result + carry;
    *result = (uint32_t)product;
    return (uint32_t)(product >> 32);
}

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

/* Subtract: r = a - b */
static void bn_sub(bn_t *r, const bn_t *a, const bn_t *b) {
    uint32_t borrow = 0;
    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t next_borrow = (a->array[i] < b->array[i] + borrow) ? 1 : 0;
        r->array[i] = a->array[i] - b->array[i] - borrow;
        borrow = next_borrow;
    }
}

/* Add: r = a + b */
static uint32_t bn_add(bn_t *r, const bn_t *a, const bn_t *b) {
    uint32_t carry = 0;
    for (int i = 0; i < BN_WORDS; i++) {
        uint64_t sum = (uint64_t)a->array[i] + b->array[i] + carry;
        r->array[i] = (uint32_t)sum;
        carry = (uint32_t)(sum >> 32);
    }
    return carry;
}

/* Montgomery reduction: r = (a * R^-1) mod m
 * Input: a (double-width), m (modulus), m_inv = -m^-1 mod 2^32
 * Output: r = a / R mod m, where R = 2^(BN_WORDS * 32)
 *
 * CIOS algorithm (Coarsely Integrated Operand Scanning):
 * for i = 0 to BN_WORDS-1:
 *     t = a[i] + r * m[i]
 *     u = t * m_inv mod 2^32
 *     t = t + u * m
 *     r = t / 2^32
 */
static void mont_reduce(bn_t *r, const bn_2x_t *a, const mont_ctx_t *ctx) {
    bn_2x_t t = *a;

    for (int i = 0; i < BN_WORDS; i++) {
        /* u = t[i] * m_inv mod 2^32 */
        uint32_t u = t.array[i] * ctx->m_inv;

        /* t = t + u * m * 2^(i*32) */
        uint64_t carry = 0;
        for (int j = 0; j < BN_WORDS; j++) {
            uint64_t product = (uint64_t)u * ctx->m.array[j] + t.array[i + j] + carry;
            t.array[i + j] = (uint32_t)product;
            carry = product >> 32;
        }

        /* Propagate carry through rest of t */
        for (int j = BN_WORDS; j + i < BN_2X_WORDS && carry; j++) {
            uint64_t sum = (uint64_t)t.array[i + j] + carry;
            t.array[i + j] = (uint32_t)sum;
            carry = sum >> 32;
        }
    }

    /* Result is t >> (BN_WORDS * 32) */
    bn_zero(r);
    for (int i = 0; i < BN_WORDS; i++) {
        if (BN_WORDS + i < BN_2X_WORDS) {
            r->array[i] = t.array[BN_WORDS + i];
        }
    }

    /* Final conditional subtraction */
    if (bn_cmp(r, &ctx->m) >= 0) {
        bn_sub(r, r, &ctx->m);
    }
}

/* Montgomery multiplication: r = (a * b * R^-1) mod m */
static void mont_mul(bn_t *r, const bn_t *a, const bn_t *b, const mont_ctx_t *ctx) {
    bn_2x_t product;
    bn_mul_wide(&product, a, b);
    mont_reduce(r, &product, ctx);
}

/* Convert to Montgomery form: r = a * R mod m */
static void mont_to(bn_t *r, const bn_t *a, const mont_ctx_t *ctx) {
    mont_mul(r, a, &ctx->r2, ctx);
}

/* Convert from Montgomery form: r = a * R^-1 mod m */
static void mont_from(bn_t *r, const bn_t *a, const mont_ctx_t *ctx) {
    bn_2x_t a_wide;
    bn_2x_zero(&a_wide);
    for (int i = 0; i < BN_WORDS; i++) {
        a_wide.array[i] = a->array[i];
    }
    mont_reduce(r, &a_wide, ctx);
}

/* Simple modular reduction (only for initialization) */
static void bn_mod_simple(bn_t *r, const bn_2x_t *a, const bn_t *m) {
    bn_2x_t temp = *a;

    for (int iter = 0; iter < 10000; iter++) {
        /* Check if temp < m */
        int all_high_zero = 1;
        for (int i = BN_WORDS; i < BN_2X_WORDS; i++) {
            if (temp.array[i] != 0) {
                all_high_zero = 0;
                break;
            }
        }

        if (all_high_zero) {
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
            if (cmp < 0) break;
        }

        /* Subtract m with appropriate shift */
        int high = -1;
        for (int i = BN_2X_WORDS - 1; i >= 0; i--) {
            if (temp.array[i] != 0) {
                high = i;
                break;
            }
        }
        if (high < 0) break;

        int shift = (high >= BN_WORDS) ? (high - BN_WORDS + 1) : 0;

        uint32_t borrow = 0;
        if (shift > 0) {
            for (int i = shift; i < shift + BN_WORDS && i < BN_2X_WORDS; i++) {
                uint32_t next_borrow = (temp.array[i] < m->array[i - shift] + borrow) ? 1 : 0;
                temp.array[i] = temp.array[i] - m->array[i - shift] - borrow;
                borrow = next_borrow;
            }
            for (int i = shift + BN_WORDS; i < BN_2X_WORDS; i++) {
                if (borrow == 0) break;
                uint32_t next_borrow = (temp.array[i] < borrow) ? 1 : 0;
                temp.array[i] = temp.array[i] - borrow;
                borrow = next_borrow;
            }
        } else {
            for (int i = 0; i < BN_WORDS; i++) {
                uint32_t next_borrow = (temp.array[i] < m->array[i] + borrow) ? 1 : 0;
                temp.array[i] = temp.array[i] - m->array[i] - borrow;
                borrow = next_borrow;
            }
        }
    }

    bn_zero(r);
    for (int i = 0; i < BN_WORDS; i++) {
        r->array[i] = temp.array[i];
    }

    /* Final cleanup */
    if (bn_cmp(r, m) >= 0) {
        bn_sub(r, r, m);
    }
}

/* Compute m_inv = -m^-1 mod 2^32 using Newton's method
 * We want x such that m * x ≡ -1 (mod 2^32)
 */
static uint32_t compute_m_inv(uint32_t m0) {
    /* Newton iteration: x_{n+1} = x_n * (2 - m * x_n)
     * Start with x_0 = m */
    uint32_t x = m0;
    for (int i = 0; i < 5; i++) {  /* 5 iterations gives 2^32 bits of precision */
        x = x * (2 - m0 * x);
    }
    return -x;  /* Return -m^-1 mod 2^32 */
}

/* Initialize Montgomery context */
static void mont_init(mont_ctx_t *ctx, const bn_t *m) {
    ctx->m = *m;

    /* Compute m_inv = -m^-1 mod 2^32 */
    ctx->m_inv = compute_m_inv(m->array[0]);

    /* Compute r2 = R^2 mod m where R = 2^(BN_WORDS * 32)
     * R^2 = 2^(2 * BN_WORDS * 32) = 2^(128 * 32) = 2^4096
     *
     * Method: Start with 2^2048 mod m, then double it 2048 times */

    /* Start with r = 2^2048 mod m */
    /* 2^2048 = 1 << 2048 = word[64] = 1 (but that's out of bounds!)
     * So we need to compute 2^2048 mod m properly */

    /* Simple approach: r = 1, then double 4096 times, reducing each time
     * This computes R^2 mod m where R = 2^2048 */
    bn_t r, two_r;
    bn_zero(&r);
    r.array[0] = 1;

    for (int i = 0; i < 2048 * 2; i++) {  /* 2^4096 = (2^1)^4096 starting from 1 */
        /* two_r = (r + r) mod m = 2*r mod m */
        uint32_t carry = bn_add(&two_r, &r, &r);

        /* Reduce if needed */
        if (carry || bn_cmp(&two_r, m) >= 0) {
            bn_sub(&two_r, &two_r, m);  /* two_r = two_r - m */
        }

        /* Copy result back to r for next iteration */
        r = two_r;
    }

    ctx->r2 = r;
}

/* Modular exponentiation using Montgomery multiplication */
static void bn_modexp(bn_t *r, const bn_t *base, const bn_t *exp, const bn_t *mod) {
    mont_ctx_t ctx;
    mont_init(&ctx, mod);

    bn_t result_mont, base_mont, temp;

    /* Convert base to Montgomery form */
    mont_to(&base_mont, base, &ctx);

    /* result = 1 in Montgomery form = R mod m */
    bn_zero(&result_mont);
    result_mont.array[0] = 1;
    mont_to(&result_mont, &result_mont, &ctx);

    /* Binary exponentiation */
    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t exp_word = exp->array[i];
        for (int j = 0; j < 32; j++) {
            if (exp_word & 1) {
                mont_mul(&temp, &result_mont, &base_mont, &ctx);
                result_mont = temp;
            }

            mont_mul(&temp, &base_mont, &base_mont, &ctx);
            base_mont = temp;

            exp_word >>= 1;
        }
    }

    /* Convert result back from Montgomery form */
    mont_from(r, &result_mont, &ctx);
}

/* Compatibility wrapper */
static void bn_mulmod(bn_t *r, const bn_t *a, const bn_t *b, const bn_t *m) {
    mont_ctx_t ctx;
    mont_init(&ctx, m);

    bn_t a_mont, b_mont, r_mont;
    mont_to(&a_mont, a, &ctx);
    mont_to(&b_mont, b, &ctx);
    mont_mul(&r_mont, &a_mont, &b_mont, &ctx);
    mont_from(r, &r_mont, &ctx);
}

#endif /* BIGNUM_MONTGOMERY_H */
