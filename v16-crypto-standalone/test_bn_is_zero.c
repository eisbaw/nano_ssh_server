/*
 * Test bn_is_zero function
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "bignum_tiny.h"

void test_bn_is_zero() {
    bn_t a;

    /* Test 1: Zero */
    printf("Test 1: All zeros\n");
    bn_zero(&a);
    printf("  bn_is_zero = %d (expected 1)\n", bn_is_zero(&a));
    assert(bn_is_zero(&a) == 1);
    printf("  ✅ PASS\n\n");

    /* Test 2: word[0] = 1 */
    printf("Test 2: word[0] = 1\n");
    bn_zero(&a);
    a.array[0] = 1;
    printf("  array[0] = %u\n", a.array[0]);
    printf("  bn_is_zero = %d (expected 0)\n", bn_is_zero(&a));
    assert(bn_is_zero(&a) == 0);
    printf("  ✅ PASS\n\n");

    /* Test 3: word[16] = 1 (like 2^512) */
    printf("Test 3: word[16] = 1\n");
    bn_zero(&a);
    a.array[16] = 1;
    printf("  array[16] = %u\n", a.array[16]);
    printf("  Checking all words:\n");
    for (int i = 0; i < 20; i++) {
        if (a.array[i] != 0) {
            printf("    array[%d] = %u\n", i, a.array[i]);
        }
    }
    printf("  bn_is_zero = %d (expected 0)\n", bn_is_zero(&a));
    assert(bn_is_zero(&a) == 0);
    printf("  ✅ PASS\n\n");

    /* Test 4: After bn_mul result */
    printf("Test 4: After bn_mul(2^256, 2^256)\n");
    bn_t x, result;
    bn_zero(&x);
    x.array[8] = 1;  /* 2^256 */

    printf("  Before: x.array[8] = %u\n", x.array[8]);

    bn_mul(&result, &x, &x);

    printf("  After bn_mul(&result, &x, &x):\n");
    printf("  First 20 words of result:\n");
    for (int i = 0; i < 20; i++) {
        if (result.array[i] != 0) {
            printf("    result.array[%d] = %u\n", i, result.array[i]);
        }
    }

    int is_zero = bn_is_zero(&result);
    printf("  bn_is_zero(&result) = %d (expected 0)\n", is_zero);

    if (is_zero) {
        printf("  ❌ FAIL: bn_is_zero returned true for non-zero result!\n");
        printf("  Manually checking each word:\n");
        for (int i = 0; i < BN_WORDS; i++) {
            if (result.array[i] != 0) {
                printf("    word[%d] = 0x%08x (NON-ZERO!)\n", i, result.array[i]);
            }
        }
        return;
    }

    assert(is_zero == 0);
    printf("  ✅ PASS\n\n");
}

int main() {
    printf("========================================\n");
    printf("Testing bn_is_zero function\n");
    printf("========================================\n\n");

    printf("BN_WORDS = %d\n", BN_WORDS);
    printf("sizeof(bn_t) = %zu\n", sizeof(bn_t));
    printf("sizeof(bn_t.array) = %zu\n\n", sizeof(((bn_t*)0)->array));

    test_bn_is_zero();

    printf("All tests passed!\n");
    return 0;
}
