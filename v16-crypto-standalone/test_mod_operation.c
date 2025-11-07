#include <stdio.h>
#include <stdint.h>
#include "bignum_adapter.h"
#include "rsa.h"

static void print_first_words(const char *name, const bn_t *n) {
    printf("%s: [0]=0x%08x [1]=0x%08x [2]=0x%08x\n", name, n->array[0], n->array[1], n->array[2]);
}

int main() {
    rsa_key_t key;
    bn_t a, result;
    
    rsa_init_key(&key);
    
    printf("Test: modular reduction of 0xADEF98100000000 mod n\n\n");
    
    /* Set a = 0xADEF98100000000 */
    bignum_init((struct bn*)&a);
    a.array[0] = 0x00000000;
    a.array[1] = 0x0ADEF981;
    
    print_first_words("a (before mod)", &a);
    print_first_words("n (modulus)", &key.n);
    
    /* Perform: result = a mod n */
    bignum_mod((struct bn*)&a, (struct bn*)&key.n, (struct bn*)&result);
    
    printf("\n");
    print_first_words("result (after mod)", &result);
    
    /* Check if result is zero */
    int is_zero = bignum_is_zero((struct bn*)&result);
    printf("\nIs result zero? %s\n", is_zero ? "YES - BUG!" : "NO - OK");
    
    if (is_zero) {
        printf("\nThis is the bug! Modular reduction is producing zero.\n");
        printf("This should not happen since 0xADEF98100000000 < n.\n");
        return 1;
    }
    
    return 0;
}
