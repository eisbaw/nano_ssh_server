#include <stdio.h>
#include <string.h>
#include "bignum_tiny.h"

void print_words(const char *label, const bn_t *bn, int count) {
    printf("%s: ", label);
    for (int i = count - 1; i >= 0; i--) {
        printf("%08x ", bn->array[i]);
    }
    printf("\n");
}

int main() {
    bn_t a, result;

    /* Test 1: Shift 1 by 8 bits = 256 */
    printf("Test 1: 1 << 8 = 256\n");
    bn_zero(&a);
    a.array[0] = 1;
    bn_shl(&result, &a, 8);
    printf("Expected: 256, Got: %u\n", result.array[0]);
    if (result.array[0] == 256) {
        printf("✅ PASS\n\n");
    } else {
        printf("❌ FAIL\n\n");
    }

    /* Test 2: Shift 1 by 32 bits (one full word) */
    printf("Test 2: 1 << 32\n");
    bn_zero(&a);
    a.array[0] = 1;
    bn_shl(&result, &a, 32);
    print_words("Result", &result, 3);
    if (result.array[0] == 0 && result.array[1] == 1) {
        printf("✅ PASS\n\n");
    } else {
        printf("❌ FAIL\n\n");
    }

    /* Test 3: Shift 3 by 33 bits */
    printf("Test 3: 3 << 33\n");
    bn_zero(&a);
    a.array[0] = 3;  /* binary: 11 */
    bn_shl(&result, &a, 33);
    /* 3 << 33 = 3 * 2^33 = 25769803776 */
    /* In 32-bit words: word[1] should be 6, word[0] should be 0 */
    print_words("Result", &result, 3);
    if (result.array[0] == 0 && result.array[1] == 6) {
        printf("✅ PASS\n\n");
    } else {
        printf("❌ FAIL (expected: word[1]=6, word[0]=0)\n\n");
    }

    /* Test 4: Large shift */
    printf("Test 4: 0xFF << 2000 bits\n");
    bn_zero(&a);
    a.array[0] = 0xFF;
    bn_shl(&result, &a, 2000);
    /* Should shift into word[62] (2000/32 = 62.5) */
    int word_idx = 2000 / 32;  /* 62 */
    int bit_shift = 2000 % 32;  /* 16 */
    printf("Should be in word[%d] with bit shift %d\n", word_idx, bit_shift);
    printf("word[62] = %08x, word[63] = %08x\n", result.array[62], result.array[63]);
    /* 0xFF << 16 = 0xFF0000 */
    if (result.array[62] == 0xFF0000 && result.array[63] == 0) {
        printf("✅ PASS\n\n");
    } else {
        printf("❌ FAIL\n\n");
    }

    return 0;
}
