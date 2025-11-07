/* 
 * Test if using tiny-bignum-c's bignum_pow instead of custom bn_modexp works
 */
#include <stdio.h>
#include <stdint.h>
#include "bn_tinybignum.h"

int main() {
    struct bn base, exp, result, mod;
    
    printf("Test: using bignum functions differently\n\n");
    
    /* Simple test: 42^7 mod 100 = 88 */
    bignum_init(&base);
    bignum_init(&exp);
    bignum_init(&mod);
    
    bignum_from_int(&base, 42);
    bignum_from_int(&exp, 7);
    bignum_from_int(&mod, 100);
    
    /* Method 1: Compute 42^7 first, then mod */
    printf("Method 1: (base^exp) then mod\n");
    bignum_pow(&base, &exp, &result);
    bignum_mod(&result, &mod, &result);
    
    int r1 = bignum_to_int(&result);
    printf("Result: %d (expected 88)\n\n", r1);
    
    /* Method 2: Manual modular exponentiation like our bn_modexp */
    printf("Method 2: Modular exponentiation (multiply and mod at each step)\n");
    struct bn result2, base_copy, temp;
    bignum_init(&result2);
    bignum_from_int(&result2, 1);
    bignum_assign(&base_copy, &base);
    
    /* Process exponent 7 = 0b111 */
    for (int i = 0; i < 3; i++) {  /* 3 bits in exponent 7 */
        /* Multiply */
        bignum_mul(&result2, &base_copy, &temp);
        bignum_mod(&temp, &mod, &result2);
        
        /* Square (except on last iteration) */
        if (i < 2) {
            bignum_mul(&base_copy, &base_copy, &temp);
            bignum_mod(&temp, &mod, &base_copy);
        }
    }
    
    int r2 = bignum_to_int(&result2);
    printf("Result: %d (expected 88)\n\n", r2);
    
    if (r1 == 88 && r2 == 88) {
        printf("✓ Both methods work for small numbers\n");
        return 0;
    } else {
        printf("✗ At least one method failed\n");
        return 1;
    }
}
