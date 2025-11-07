#include <stdio.h>
#include <stdint.h>
#include "bn_tinybignum.h"
#include "bignum_adapter.h"
#include "rsa.h"

int main() {
    rsa_key_t key;
    bn_t two_n;
    
    rsa_init_key(&key);
    bignum_add(&key.n, &key.n, &two_n);
    
    printf("High words:\n");
    printf("n.array[63]    = 0x%08x\n", key.n.array[63]);
    printf("(2*n).array[63] = 0x%08x\n", two_n.array[63]);
    printf("\n");
    
    printf("Comparison: 2*n vs n\n");
    int cmp = bignum_cmp(&two_n, &key.n);
    printf("bignum_cmp(2*n, n) = %d (LARGER=%d, EQUAL=%d, SMALLER=%d)\n", 
           cmp, LARGER, EQUAL, SMALLER);
    
    if (cmp == LARGER) {
        printf("✓ Correctly identifies 2*n > n\n");
    } else {
        printf("✗ BUG: 2*n should be > n!\n");
    }
    
    return 0;
}
