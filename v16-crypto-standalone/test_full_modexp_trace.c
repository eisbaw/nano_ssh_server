#include <stdio.h>
#include <stdint.h>
#include "bignum_adapter.h"
#include "rsa.h"

static void print_first_words(const char *name, const bn_t *n) {
    printf("%s: [0]=0x%08x [1]=0x%08x\n", name, n->array[0], n->array[1]);
}

int main() {
    rsa_key_t key;
    bn_t m;
    
    rsa_init_key(&key);
    
    /* m = 42 */
    bn_zero(&m);
    bignum_from_int((struct bn*)&m, 42);
    
    printf("Computing 42^65537 mod n with detailed trace\n");
    printf("Exponent 65537 = 0x10001 = binary 10000000000000001\n\n");
    
    /* Manually implement bn_modexp with tracing */
    bn_t result, base_copy, temp;
    
    bignum_init(&result);
    bignum_from_int(&result, 1);
    print_first_words("Initial result", &result);
    
    bignum_assign(&base_copy, (struct bn*)&m);
    print_first_words("Initial base_copy (42)", &base_copy);
    
    /* max_word = 0 for exponent 65537 */
    int max_word = 0;
    
    printf("\nProcessing exponent word 0: 0x%08x\n\n", key.e.array[0]);
    
    DTYPE exp_word = key.e.array[0];  /* 0x00010001 */
    
    for (int j = 0; j < 32; j++) {
        int bit = exp_word & 1;
        
        if (j == 0 || j == 16 || j == 17) {  /* Trace important bits */
            printf("Bit %d: %d\n", j, bit);
            
            if (bit) {
                printf("  Multiplying: result = result * base_copy mod n\n");
                print_first_words("    result (before)", &result);
                print_first_words("    base_copy", &base_copy);
                
                bignum_mul(&result, &base_copy, &temp);
                print_first_words("    temp (after mul)", &temp);
                
                bignum_mod(&temp, (struct bn*)&key.n, &result);
                print_first_words("    result (after mod)", &result);
            }
        }
        
        exp_word >>= 1;
        
        if (exp_word == 0 && 0 == max_word) {
            printf("\nBit %d: exp_word became 0, stopping (correct!)\n", j);
            break;
        }
        
        if (j == 0 || j == 15 || j == 16) {  /* Trace important squares */
            if (j != 16 || exp_word != 0) {  /* Only if we're not at the last bit */
                printf("  Squaring: base_copy = base_copy^2 mod n\n");
                print_first_words("    base_copy (before)", &base_copy);
                
                bignum_mul(&base_copy, &base_copy, &temp);
                print_first_words("    temp (after square)", &temp);
                
                bignum_mod(&temp, (struct bn*)&key.n, &base_copy);
                print_first_words("    base_copy (after mod)", &base_copy);
            }
        } else {
            /* Still do the squaring, just don't print */
            bignum_mul(&base_copy, &base_copy, &temp);
            bignum_mod(&temp, (struct bn*)&key.n, &base_copy);
        }
        
        printf("\n");
    }
    
    printf("\nFinal result:\n");
    print_first_words("result", &result);
    printf("\nExpected: [0]=0xf91e0d65\n");
    
    if (result.array[0] == 0xf91e0d65) {
        printf("✓ CORRECT!\n");
        return 0;
    } else {
        printf("✗ WRONG\n");
        return 1;
    }
}
