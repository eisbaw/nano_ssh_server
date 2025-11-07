/*
 * Manually test 2*n mod n using different approaches
 */
#include <stdio.h>
#include <stdint.h>
#include "bignum_adapter.h"
#include "rsa.h"

static void print_words(const char *name, const bn_t *n, int count) {
    printf("%s:\n", name);
    for (int i = 0; i < count; i++) {
        printf("  [%2d] = 0x%08x\n", i, n->array[i]);
    }
}

int main() {
    rsa_key_t key;
    bn_t two_n, result;
    
    rsa_init_key(&key);
    
    printf("Test: Compute 2*n using addition instead of shift\n\n");
    
    /* Method 1: Use lshift */
    bignum_assign(&two_n, &key.n);
    bignum_lshift(&two_n, &two_n, 1);
    
    printf("Method 1: two_n = n << 1\n");
    print_words("two_n", &two_n, 4);
    printf("\n");
    
    /* Method 2: Use addition */
    bn_t two_n_add;
    bignum_add(&key.n, &key.n, &two_n_add);
    
    printf("Method 2: two_n = n + n\n");
    print_words("two_n_add", &two_n_add, 4);
    printf("\n");
    
    /* Check if they match */
    if (bignum_cmp(&two_n, &two_n_add) == EQUAL) {
        printf("✓ Both methods produce same result\n\n");
    } else {
        printf("✗ Methods differ! Lshift might be buggy\n\n");
    }
    
    /* Now try mod with the addition-based 2*n */
    printf("Computing (n+n) mod n:\n");
    bignum_mod(&two_n_add, &key.n, &result);
    print_words("result", &result, 4);
    
    if (bignum_is_zero(&result)) {
        printf("✓ Result is zero (correct!)\n");
    } else {
        printf("✗ Result is non-zero (bug in mod!)\n");
    }
    
    return 0;
}
