/*
 * Debug (2*n) / n division
 */
#include <stdio.h>
#include <stdint.h>
#include "bn_tinybignum.h"
#include "bignum_adapter.h"
#include "rsa.h"

int main() {
    rsa_key_t key;
    bn_t two_n, quotient, remainder;
    
    rsa_init_key(&key);
    
    /* Compute 2*n */
    bignum_add(&key.n, &key.n, &two_n);
    
    printf("Computing (2*n) / n\n\n");
    printf("Dividend (2*n): [0]=0x%08x\n", two_n.array[0]);
    printf("Divisor (n):    [0]=0x%08x\n", key.n.array[0]);
    printf("\n");
    
    /* Perform division */
    bignum_divmod(&two_n, &key.n, &quotient, &remainder);
    
    printf("Quotient: [0]=0x%08x\n", quotient.array[0]);
    printf("Remainder: [0]=0x%08x\n", remainder.array[0]);
    printf("\n");
    
    printf("Expected:\n");
    printf("  Quotient = 2\n");
    printf("  Remainder = 0\n");
    printf("\n");
    
    if (quotient.array[0] == 2 && bignum_is_zero(&remainder)) {
        printf("✓ PASS\n");
        return 0;
    } else {
        printf("✗ FAIL\n");
        return 1;
    }
}
