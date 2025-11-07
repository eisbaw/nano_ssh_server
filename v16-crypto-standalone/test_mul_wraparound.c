/*
 * Test if multiplication wraps around for large values near n
 */
#include <stdio.h>
#include <stdint.h>
#include "bn_tinybignum.h"
#include "bignum_adapter.h"
#include "rsa.h"

int main() {
    rsa_key_t key;
    bn_t a, b, product;
    
    rsa_init_key(&key);
    
    /* Use values close to n for multiplication */
    /* a = n - 1, b = n - 1 */
    bignum_assign(&a, &key.n);
    bignum_dec(&a);  /* a = n - 1 */
    
    bignum_assign(&b, &key.n);
    bignum_dec(&b);  /* b = n - 1 */
    
    printf("Test: (n-1) * (n-1)\n\n");
    
    bignum_mul(&a, &b, &product);
    
    printf("Mathematically: (n-1)^2 = n^2 - 2n + 1\n");
    printf("For large n, this is approximately n^2\n");
    printf("n^2 requires ~4096 bits\n\n");
    
    printf("If multiplication wrapped, product < n\n");
    printf("If multiplication is correct, product > n\n\n");
    
    int cmp = bignum_cmp(&product, &key.n);
    
    if (cmp == LARGER) {
        printf("✓ product > n (multiplication seems OK)\n");
    } else if (cmp == SMALLER) {
        printf("✗ product < n - WRAPAROUND DETECTED!\n");
        printf("  This proves bignum_mul loses high bits\n");
        printf("  Cannot be used for RSA modular arithmetic!\n");
    } else {
        printf("? product == n (unexpected)\n");
    }
    
    return (cmp == LARGER) ? 0 : 1;
}
