#include <stdio.h>
#include <stdint.h>
#include "bignum_adapter.h"
#include "rsa.h"

int main() {
    rsa_key_t key;
    bn_t m, result, base_copy, temp;
    
    rsa_init_key(&key);
    
    bignum_init(&m);
    bignum_from_int(&m, 42);
    
    bignum_init(&result);
    bignum_from_int(&result, 1);
    
    bignum_assign(&base_copy, &m);
    
    printf("Tracing squaring operations to find where base_copy becomes 0\n\n");
    
    DTYPE exp_word = 0x00010001;
    
    for (int j = 0; j < 32; j++) {
        int bit = exp_word & 1;
        
        printf("Bit %2d: %d | base_copy=[0]=0x%08x [1]=0x%08x", 
               j, bit, base_copy.array[0], base_copy.array[1]);
        
        if (bit) {
            bignum_mul(&result, &base_copy, &temp);
            bignum_mod(&temp, &key.n, &result);
        }
        
        exp_word >>= 1;
        
        if (exp_word == 0) {
            printf(" | STOP (last bit)\n");
            break;
        }
        
        /* Square base */
        bignum_mul(&base_copy, &base_copy, &temp);
        printf(" → temp=[0]=0x%08x [1]=0x%08x", temp.array[0], temp.array[1]);
        
        bignum_mod(&temp, &key.n, &base_copy);
        printf(" → after_mod=[0]=0x%08x [1]=0x%08x", base_copy.array[0], base_copy.array[1]);
        
        /* Check if became zero */
        if (bignum_is_zero(&base_copy)) {
            printf(" ← BUG: became ZERO!\n");
            printf("\nFound the bug at bit %d!\n", j);
            return 1;
        }
        
        printf("\n");
    }
    
    return 0;
}
