#include <stdio.h>
#include <string.h>
#include "bignum_tiny.h"

int main() {
    bn_t a, b, result;
    
    /* Test 1: Small multiplication */
    bn_zero(&a);
    bn_zero(&b);
    a.array[0] = 1000;
    b.array[0] = 2000;
    
    bn_mul(&result, &a, &b);
    
    printf("Test 1: 1000 * 2000 = %u (expected 2000000)\n", result.array[0]);
    
    /* Test 2: Larger multiplication */
    bn_zero(&a);
    bn_zero(&b);
    a.array[1] = 1;  // = 2^32
    b.array[0] = 2;
    
    bn_mul(&result, &a, &b);
    
    printf("Test 2: 2^32 * 2 = [1]=%u [0]=%u (expected [1]=2 [0]=0)\n", 
           result.array[1], result.array[0]);
    
    /* Test 3: Full word multiplication */
    bn_zero(&a);
    bn_zero(&b);
    a.array[0] = 0xFFFFFFFF;
    b.array[0] = 0xFFFFFFFF;
    
    bn_mul(&result, &a, &b);
    
    printf("Test 3: 0xFFFFFFFF * 0xFFFFFFFF = [1]=%08x [0]=%08x\n", 
           result.array[1], result.array[0]);
    printf("        Expected: [1]=fffffffe [0]=00000001\n");
    
    return 0;
}
