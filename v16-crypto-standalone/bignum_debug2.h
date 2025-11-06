#ifndef BIGNUM_DEBUG2_H
#define BIGNUM_DEBUG2_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define BN_WORDS 64
#define BN_2X_WORDS (BN_WORDS * 2)

typedef struct {
    uint32_t array[BN_WORDS];
} bn_t;

typedef struct {
    uint32_t array[BN_2X_WORDS];
} bn_2x_t;

static inline void bn_zero(bn_t *a) {
    memset(a->array, 0, sizeof(a->array));
}

static inline void bn_2x_zero(bn_2x_t *a) {
    memset(a->array, 0, sizeof(a->array));
}

static inline void bn_from_bytes(bn_t *a, const uint8_t *bytes, size_t len) {
    bn_zero(a);
    size_t max_len = (len < 256) ? len : 256;
    for (size_t i = 0; i < max_len; i++) {
        size_t byte_idx = max_len - 1 - i;
        size_t word_idx = i / 4;
        size_t byte_in_word = i % 4;
        a->array[word_idx] |= ((uint32_t)bytes[byte_idx]) << (byte_in_word * 8);
    }
}

static int bn_cmp(const bn_t *a, const bn_t *b) {
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (a->array[i] > b->array[i]) return 1;
        if (a->array[i] < b->array[i]) return -1;
    }
    return 0;
}

static void bn_sub(bn_t *r, const bn_t *a, const bn_t *b) {
    uint32_t borrow = 0;
    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t next_borrow = (a->array[i] < b->array[i] + borrow) ? 1 : 0;
        r->array[i] = a->array[i] - b->array[i] - borrow;
        borrow = next_borrow;
    }
}

static void bn_mul_wide(bn_2x_t *r, const bn_t *a, const bn_t *b) {
    bn_2x_zero(r);
    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t carry = 0;
        for (int j = 0; j < BN_WORDS; j++) {
            uint64_t product = (uint64_t)a->array[i] * (uint64_t)b->array[j] + 
                               (uint64_t)r->array[i + j] + (uint64_t)carry;
            r->array[i + j] = (uint32_t)(product & 0xFFFFFFFF);
            carry = (uint32_t)(product >> 32);
        }
        r->array[i + BN_WORDS] = carry;
    }
}

/* DEBUG version with iteration counting and FIXED borrow logic */
static void bn_mod_simple(bn_t *r, const bn_2x_t *a, const bn_t *m) {
    bn_2x_t temp = *a;

    for (int iter = 0; iter < 10000; iter++) {
        int all_high_zero = 1;
        for (int i = BN_WORDS; i < BN_2X_WORDS; i++) {
            if (temp.array[i] != 0) {
                all_high_zero = 0;
                break;
            }
        }

        if (all_high_zero) {
            for (int i = 0; i < BN_WORDS; i++) {
                r->array[i] = temp.array[i];
            }
            while (bn_cmp(r, m) >= 0) {
                bn_sub(r, r, m);
            }
            if (iter > 0) {
                printf("  bn_mod_simple converged after %d iterations\n", iter);
            }
            return;
        }

        /* FIXED: Use uint64_t to avoid overflow in borrow calculation */
        uint32_t borrow = 0;
        for (int i = 0; i < BN_WORDS; i++) {
            uint64_t diff = (uint64_t)temp.array[BN_WORDS + i] - (uint64_t)m->array[i] - (uint64_t)borrow;
            temp.array[BN_WORDS + i] = (uint32_t)(diff & 0xFFFFFFFF);
            borrow = (diff >> 32) & 1;
        }
    }

    printf("ERROR: bn_mod_simple exceeded 10000 iterations!\n");
    bn_zero(r);
}

static void bn_mulmod(bn_t *r, const bn_t *a, const bn_t *b, const bn_t *m) {
    bn_2x_t product;
    bn_mul_wide(&product, a, b);
    bn_mod_simple(r, &product, m);
}

#endif
