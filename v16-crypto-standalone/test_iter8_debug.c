#include <stdio.h>
#include <stdint.h>
#include "bignum_debug.h"
#include "rsa.h"

int main() {
    rsa_key_t key;
    bn_t base_copy, temp;

    rsa_init_key(&key);

    bn_zero(&base_copy);
    base_copy.array[0] = 42;

    printf("Testing iterations 0-8 with debug output\n\n");

    /* Iterate to iteration 7 */
    for (int i = 0; i <= 7; i++) {
        printf("Iteration %d...\n", i);
        bn_mulmod(&temp, &base_copy, &base_copy, &key.n);
        base_copy = temp;
    }

    printf("\n");

    return 0;
}
