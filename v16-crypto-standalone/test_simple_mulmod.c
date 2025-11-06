/*
 * Test just the wide multiplication without reduction first
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define BN_WORDS 64
#define BN_2X_WORDS 128

typedef struct {
    uint32_t array[BN_WORDS];
} bn_t;

typedef struct {
    uint32_t array[BN_2X_WORDS];
} bn_2x_t;

static inline void bn_zero(bn_t *a) {
    memset(a, 0, sizeof(bn_t));
}

static inline void bn_2x_zero(bn_2x_t *a) {
    memset(a, 0, sizeof(bn_2x_t));
}

static inline uint32_t mul_add_word(uint32_t *result, uint32_t a, uint32_t b, uint32_t carry) {
    uint64_t product = (uint64_t)a * b + *result + carry;
    *result = (uint32_t)product;
    return (uint32_t)(product >> 32);
}

/* Wide multiplication */
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

int main() {
    bn_t a, b;
    bn_2x_t result;

    printf("Test wide multiplication\n\n");

    /* Test 1: 17 * 19 = 323 */
    printf("Test 1: 17 * 19 = 323\n");
    bn_zero(&a);
    a.array[0] = 17;
    bn_zero(&b);
    b.array[0] = 19;

    bn_mul_wide(&result, &a, &b);

    printf("  result.array[0] = %u (expected 323)\n", result.array[0]);
    printf("  result.array[1] = %u (expected 0)\n", result.array[1]);

    if (result.array[0] == 323 && result.array[1] == 0) {
        printf("  ✅ PASS\n\n");
    } else {
        printf("  ❌ FAIL\n\n");
        return 1;
    }

    /* Test 2: 2^1024 * 2^1024 = 2^2048 */
    printf("Test 2: 2^1024 * 2^1024 = 2^2048\n");
    bn_zero(&a);
    a.array[32] = 1;  /* 2^1024 */

    bn_mul_wide(&result, &a, &a);

    printf("  result.array[64] = %u (expected 1)\n", result.array[64]);
    printf("  result.array[63] = %u (expected 0)\n", result.array[63]);
    printf("  result.array[65] = %u (expected 0)\n", result.array[65]);

    if (result.array[64] == 1 && result.array[63] == 0 && result.array[65] == 0) {
        printf("  ✅ PASS: 2^2048 correctly stored in word[64]!\n\n");
    } else {
        printf("  ❌ FAIL\n\n");
        return 1;
    }

    /* Test 3: Check that result doesn't overflow */
    printf("Test 3: 2^2047 * 2 = 2^2048\n");
    bn_zero(&a);
    a.array[63] = 0x80000000;  /* 2^2047 */
    bn_zero(&b);
    b.array[0] = 2;

    bn_mul_wide(&result, &a, &b);

    printf("  result.array[64] = %u (expected 1)\n", result.array[64]);

    if (result.array[64] == 1) {
        printf("  ✅ PASS\n\n");
    } else {
        printf("  ❌ FAIL\n\n");
        return 1;
    }

    printf("✅ Wide multiplication works correctly!\n");
    printf("Now need to implement proper modular reduction.\n");

    return 0;
}
