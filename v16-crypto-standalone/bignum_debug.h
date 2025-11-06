#ifndef BIGNUM_DEBUG_H
#define BIGNUM_DEBUG_H

#include <stdint.h>
#include <string.h>

#define BN_WORDS 64
#define BN_2X_WORDS (BN_WORDS * 2)

typedef struct {
    uint32_t array[BN_WORDS];
} bn_t;

typedef struct {
    uint32_t array[BN_2X_WORDS];
} bn_2x_t;

/* All the same functions from bignum_simple.h, but with debug output in bn_mod_simple */

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

static inline void bn_to_bytes(const bn_t *a, uint8_t *bytes, size_t len) {
    memset(bytes, 0, len);
    size_t max_len = (len < 256) ? len : 256;
    for (size_t i = 0; i < max_len; i++) {
        size_t word_idx = i / 4;
        size_t byte_in_word = i % 4;
        size_t byte_idx = max_len - 1 - i;
        bytes[byte_idx] = (a->array[word_idx] >> (byte_in_word * 8)) & 0xFF;
    }
}

static int bn_cmp(const bn_t *a, const bn_t *b) {
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (a->array[i] > b->array[i]) return 1;
        if (a->array[i] < b->array[i]) return -1;
    }
    return 0;
}

static uint32_t bn_add(bn_t *r, const bn_t *a, const bn_t *b) {
    uint32_t carry = 0;
    for (int i = 0; i < BN_WORDS; i++) {
        uint64_t sum = (uint64_t)a->array[i] + (uint64_t)b->array[i] + carry;
        r->array[i] = (uint32_t)(sum & 0xFFFFFFFF);
        carry = (uint32_t)(sum >> 32);
    }
    return carry;
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

/* DEBUG version with iteration counting */
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
            return;
        }

        uint32_t borrow = 0;
        for (int i = 0; i < BN_WORDS; i++) {
            uint32_t next_borrow = (temp.array[BN_WORDS + i] < m->array[i] + borrow) ? 1 : 0;
            temp.array[BN_WORDS + i] = temp.array[BN_WORDS + i] - m->array[i] - borrow;
            borrow = next_borrow;
        }
        
        if (iter == 9999) {
            printf("WARNING: bn_mod_simple hit 10000 iteration limit!\n");
        }
    }

    printf("ERROR: bn_mod_simple exceeded iteration limit, returning zero\n");
    bn_zero(r);
}

static void bn_mulmod(bn_t *r, const bn_t *a, const bn_t *b, const bn_t *m) {
    bn_2x_t product;
    bn_mul_wide(&product, a, b);
    bn_mod_simple(r, &product, m);
}

#endif
