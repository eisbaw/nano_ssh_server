#include <stdio.h>
#include <string.h>
#include "bignum_tiny.h"

void print_bn_simple(const char *label, const bn_t *bn) {
    printf("%s: ", label);
    for (int i = 4; i >= 0; i--) {
        printf("%08x ", bn->array[i]);
    }
    printf("...\n");
}

int main() {
    bn_t a, m, result;

    /* Simple test: 100 % 7 */
    printf("Test 1: 100 %% 7\n");
    bn_zero(&a);
    bn_zero(&m);
    a.array[0] = 100;
    m.array[0] = 7;

    print_bn_simple("Before: a", &a);
    print_bn_simple("Before: m", &m);

    bn_mod(&result, &a, &m);

    print_bn_simple("After:  result", &result);
    printf("Expected: 2, Got: %u\n", result.array[0]);

    if (result.array[0] == 2) {
        printf("✅ PASS\n\n");
    } else {
        printf("❌ FAIL\n\n");
    }

    /* Test 2: 1000 % 7 */
    printf("Test 2: 1000 %% 7\n");
    bn_zero(&a);
    a.array[0] = 1000;

    bn_mod(&result, &a, &m);
    printf("Expected: 6, Got: %u\n", result.array[0]);

    if (result.array[0] == 6) {
        printf("✅ PASS\n\n");
    } else {
        printf("❌ FAIL\n\n");
    }

    /* Test 3: Large number in array[1] */
    printf("Test 3: 2^32 + 100 %% 7\n");
    bn_zero(&a);
    a.array[1] = 1;  /* 2^32 */
    a.array[0] = 100;

    /* 2^32 = 4294967296, 4294967296 % 7 = 4 */
    /* (4294967296 + 100) % 7 = (4 + 100) % 7 = 104 % 7 = 6 */

    bn_mod(&result, &a, &m);
    printf("Expected: 6, Got: %u\n", result.array[0]);

    if (result.array[0] == 6) {
        printf("✅ PASS\n\n");
    } else {
        printf("❌ FAIL\n\n");
    }

    return 0;
}
