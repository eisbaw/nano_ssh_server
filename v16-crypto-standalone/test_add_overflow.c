#include <stdio.h>
#include <stdint.h>
#include "bn_tinybignum.h"

int main() {
    struct bn a, b, result;
    
    /* Simple test: 0xFFFFFFFF + 0xFFFFFFFF = 0x1FFFFFFFE (needs carry) */
    bignum_init(&a);
    bignum_init(&b);
    bignum_from_int(&a, 0xFFFFFFFF);
    bignum_from_int(&b, 0xFFFFFFFF);
    
    bignum_add(&a, &b, &result);
    
    printf("Test: 0xFFFFFFFF + 0xFFFFFFFF\n");
    printf("result.array[0] = 0x%08x (expected 0xFFFFFFFE)\n", result.array[0]);
    printf("result.array[1] = 0x%08x (expected 0x00000001)\n", result.array[1]);
    
    if (result.array[0] == 0xFFFFFFFE && result.array[1] == 0x00000001) {
        printf("✓ Addition with carry works\n");
    } else {
        printf("✗ Addition loses carry!\n");
    }
    
    return 0;
}
