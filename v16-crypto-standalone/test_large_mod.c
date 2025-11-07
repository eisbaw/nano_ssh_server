/*
 * Test if bignum_mod works correctly with large 2048-bit modulus
 */
#include <stdio.h>
#include <stdint.h>
#include "bignum_adapter.h"
#include "rsa.h"

static void print_first_4_words(const char *name, const bn_t *n) {
    printf("%s: [0]=0x%08x [1]=0x%08x [2]=0x%08x [3]=0x%08x\n", 
           name, n->array[0], n->array[1], n->array[2], n->array[3]);
}

int main() {
    rsa_key_t key;
    bn_t a, result;
    
    rsa_init_key(&key);
    
    printf("Test: Does modulo work correctly with 2048-bit modulus?\n\n");
    
    /* Test 1: a = n - 1, then a mod n should equal n - 1 */
    printf("Test 1: (n-1) mod n = n-1\n");
    bignum_assign(&a, &key.n);
    bignum_dec(&a);  /* a = n - 1 */
    
    print_first_4_words("a (n-1)", &a);
    print_first_4_words("n", &key.n);
    
    bignum_mod(&a, &key.n, &result);
    print_first_4_words("result", &result);
    
    /* Should be n-1 */
    if (result.array[0] == (key.n.array[0] - 1)) {
        printf("✓ PASS\n\n");
    } else {
        printf("✗ FAIL\n\n");
    }
    
    /* Test 2: a = n + 1, then a mod n should equal 1 */
    printf("Test 2: (n+1) mod n = 1\n");
    bignum_assign(&a, &key.n);
    bignum_inc(&a);  /* a = n + 1 */
    
    print_first_4_words("a (n+1)", &a);
    
    bignum_mod(&a, &key.n, &result);
    print_first_4_words("result", &result);
    
    if (result.array[0] == 1 && result.array[1] == 0 && result.array[2] == 0) {
        printf("✓ PASS\n\n");
    } else {
        printf("✗ FAIL\n\n");
    }
    
    /* Test 3: a = 2*n, then a mod n should equal 0 */
    printf("Test 3: (2*n) mod n = 0\n");
    bignum_assign(&a, &key.n);
    bignum_lshift(&a, &a, 1);  /* a = 2*n */
    
    print_first_4_words("a (2*n)", &a);
    
    bignum_mod(&a, &key.n, &result);
    print_first_4_words("result", &result);
    
    if (bignum_is_zero(&result)) {
        printf("✓ PASS\n\n");
    } else {
        printf("✗ FAIL\n\n");
    }
    
    return 0;
}
