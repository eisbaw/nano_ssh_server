#include <stdio.h>
#include <string.h>
#include "bignum_tiny.h"

int main() {
    bn_t a, b, result;

    /* Test 1: 100 - 30 = 70 */
    printf("Test 1: 100 - 30 = 70\n");
    bn_zero(&a);
    bn_zero(&b);
    a.array[0] = 100;
    b.array[0] = 30;
    bn_sub(&result, &a, &b);
    printf("Expected: 70, Got: %u\n", result.array[0]);
    if (result.array[0] == 70) {
        printf("✅ PASS\n\n");
    } else {
        printf("❌ FAIL\n\n");
    }

    /* Test 2: (2^32 + 100) - (2^32 + 30) = 70 */
    printf("Test 2: (2^32 + 100) - (2^32 + 30) = 70\n");
    bn_zero(&a);
    bn_zero(&b);
    a.array[1] = 1;
    a.array[0] = 100;
    b.array[1] = 1;
    b.array[0] = 30;
    bn_sub(&result, &a, &b);
    printf("Expected: word[1]=0, word[0]=70\n");
    printf("Got: word[1]=%u, word[0]=%u\n", result.array[1], result.array[0]);
    if (result.array[0] == 70 && result.array[1] == 0) {
        printf("✅ PASS\n\n");
    } else {
        printf("❌ FAIL\n\n");
    }

    /* Test 3: (2^32) - 1 = 0xFFFFFFFF */
    printf("Test 3: 2^32 - 1 = 0xFFFFFFFF\n");
    bn_zero(&a);
    bn_zero(&b);
    a.array[1] = 1;  /* 2^32 */
    b.array[0] = 1;
    bn_sub(&result, &a, &b);
    printf("Expected: 0xFFFFFFFF\n");
    printf("Got: 0x%08X\n", result.array[0]);
    if (result.array[0] == 0xFFFFFFFF && result.array[1] == 0) {
        printf("✅ PASS\n\n");
    } else {
        printf("❌ FAIL\n\n");
    }

    return 0;
}
