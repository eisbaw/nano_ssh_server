#include "bignum_tiny.h"

/* Force all functions to be compiled */
void test_all_functions() {
    bn_t a, b, r;
    uint8_t bytes[BN_BYTES];
    
    bn_zero(&a);
    bn_is_zero(&a);
    bn_from_bytes(&a, bytes, BN_BYTES);
    bn_to_bytes(&a, bytes, BN_BYTES);
    bn_cmp(&a, &b);
    bn_add(&r, &a, &b);
    bn_sub(&r, &a, &b);
    bn_mul(&r, &a, &b);
    bn_mod(&r, &a, &b);
    bn_modexp(&r, &a, &b, &b);
    bn_shl1(&a);
    bn_shr1(&a);
}

int main() { return 0; }
