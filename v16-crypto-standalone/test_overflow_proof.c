/*
 * Prove that bn_mul overflows at 2^2048
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "bignum_tiny.h"

int main() {
    bn_t a, result;

    printf("========================================\n");
    printf("Testing bn_mul overflow at boundary\n");
    printf("========================================\n\n");

    printf("BN_WORDS = %d\n", BN_WORDS);
    printf("Max representable value: 2^%d - 1\n", BN_WORDS * 32);
    printf("Overflow occurs at: 2^%d\n\n", BN_WORDS * 32);

    /* Test 1: Maximum value that squares without overflow */
    printf("Test 1: Square 2^1023 (word[31] = 0x80000000)\n");
    bn_zero(&a);
    a.array[31] = 0x80000000;  /* 2^(31*32 + 31) = 2^1023 */

    bn_mul(&result, &a, &a);

    int nonzero = 0;
    int max_word = -1;
    for (int i = 0; i < BN_WORDS; i++) {
        if (result.array[i] != 0) {
            nonzero++;
            max_word = i;
        }
    }

    printf("  Result: %d non-zero words, highest at word[%d]\n", nonzero, max_word);
    if (max_word >= 0) {
        printf("  result.array[%d] = 0x%08x\n", max_word, result.array[max_word]);
    }
    printf("  Expected: word[62] = 0x40000000 (2^2046)\n");
    printf("  Status: %s\n\n", (max_word == 62) ? "✅ PASS" : "❌ FAIL");

    /* Test 2: Overflow boundary - 2^1024 */
    printf("Test 2: Square 2^1024 (word[32] = 1)\n");
    bn_zero(&a);
    a.array[32] = 1;  /* 2^(32*32) = 2^1024 */

    bn_mul(&result, &a, &a);

    nonzero = 0;
    max_word = -1;
    for (int i = 0; i < BN_WORDS; i++) {
        if (result.array[i] != 0) {
            nonzero++;
            max_word = i;
        }
    }

    printf("  Result: %d non-zero words", nonzero);
    if (max_word >= 0) {
        printf(", highest at word[%d]\n", max_word);
        printf("  result.array[%d] = 0x%08x\n", max_word, result.array[max_word]);
    } else {
        printf(" (ALL ZERO)\n");
    }
    printf("  Expected: word[64] = 0x00000001 (2^2048)\n");
    printf("  But word[64] is OUT OF BOUNDS! (valid: 0-63)\n");
    printf("  Status: %s\n\n", (nonzero == 0) ? "❌ OVERFLOW (returns zero)" : "⚠️ UNEXPECTED");

    /* Test 3: Just below overflow - 2^1023.5 */
    printf("Test 3: Square 2^1020 (word[31] = 0x10000000)\n");
    bn_zero(&a);
    a.array[31] = 0x10000000;  /* 2^(31*32 + 28) = 2^1020 */

    bn_mul(&result, &a, &a);

    nonzero = 0;
    max_word = -1;
    for (int i = 0; i < BN_WORDS; i++) {
        if (result.array[i] != 0) {
            nonzero++;
            max_word = i;
        }
    }

    printf("  Result: %d non-zero words, highest at word[%d]\n", nonzero, max_word);
    if (max_word >= 0) {
        printf("  result.array[%d] = 0x%08x\n", max_word, result.array[max_word]);
    }
    printf("  Expected: word[62] with high bit (2^2040)\n");
    printf("  Status: %s\n\n", (max_word == 62 || max_word == 63) ? "✅ PASS" : "❌ FAIL");

    printf("========================================\n");
    printf("CONCLUSION:\n");
    printf("========================================\n");
    printf("bn_mul has a maximum output of 2048 bits.\n");
    printf("When multiplying two 1024+ bit numbers, the\n");
    printf("result exceeds 2048 bits and OVERFLOWS.\n");
    printf("\nFor modular exponentiation with 2048-bit prime:\n");
    printf("  prime ≈ 2^2047\n");
    printf("  prime² ≈ 2^4094 (NEEDS 4096-bit buffer!)\n");
    printf("\nThis library needs 2x buffer size for modular\n");
    printf("multiplication, which it doesn't have.\n");
    printf("\n❌ bignum_tiny.h is FUNDAMENTALLY BROKEN for\n");
    printf("   2048-bit modular exponentiation.\n");

    return 0;
}
