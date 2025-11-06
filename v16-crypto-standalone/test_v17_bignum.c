#include <stdio.h>
#include <stdint.h>
#include "bignum_v17.h"
#include "rsa.h"

int main() {
    /* Load RSA key */
    rsa_key_t key;
    bn_from_bytes(&key.n, rsa_modulus, 256);
    bn_from_bytes(&key.d, rsa_private_exponent, 256);
    bn_zero(&key.e);
    key.e.array[0] = RSA_PUBLIC_EXPONENT;

    /* Test: 42^65537 mod n */
    bn_t m, c, m2;
    
    bn_zero(&m);
    m.array[0] = 42;

    printf("Testing: 42^65537 mod n\n");
    printf("m = %u\n", m.array[0]);
    
    bn_modexp(&c, &m, &key.e, &key.n);
    printf("c.array[0] = %u\n", c.array[0]);
    
    bn_modexp(&m2, &c, &key.d, &key.n);
    printf("m2.array[0] = %u\n", m2.array[0]);

    if (m.array[0] == m2.array[0]) {
        printf("✓ RSA WORKS!\n");
        return 0;
    } else {
        printf("✗ RSA BROKEN\n");
        return 1;
    }
}
