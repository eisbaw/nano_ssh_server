#include <stdio.h>
#include <stdint.h>
#include "bignum_simple.h"

int main() {
    bn_t a, b, m, result;
    bn_2x_t product;

    /* Test: 100 * 200 mod 13 = 20000 mod 13 = 6 */
    bn_zero(&a);
    a.array[0] = 100;

    bn_zero(&b);
    b.array[0] = 200;

    bn_zero(&m);
    m.array[0] = 13;

    printf("=== Test bn_mulmod: 100 * 200 mod 13 ===\n");
    printf("Expected: 6\n");

    /* Test bn_mul_wide */
    bn_mul_wide(&product, &a, &b);
    printf("100 * 200 = %llu (should be 20000)\n", (unsigned long long)product.array[0]);

    /* Test bn_mod_simple */
    bn_mod_simple(&result, &product, &m);
    printf("20000 mod 13 = %u (should be 6)\n", result.array[0]);

    /* Test bn_mulmod */
    bn_mulmod(&result, &a, &b, &m);
    printf("bn_mulmod result: %u (should be 6)\n", result.array[0]);

    if (result.array[0] == 6) {
        printf("✓ PASS\n\n");
    } else {
        printf("✗ FAIL\n\n");
    }

    /* Test with RSA modulus */
    printf("=== Test with RSA values ===\n");
    bn_t n;
    uint8_t n_bytes[256] = {
        0xa7, 0x3e, 0x9d, 0x97, 0x8a, 0xeb, 0xa1, 0x12,
        0x40, 0x5d, 0x96, 0x3c, 0xc7, 0x66, 0x5c, 0xa7,
        /* ... rest zeros for simplicity ... */
    };
    bn_from_bytes(&n, n_bytes, 8);  // Just use first 8 bytes

    bn_zero(&a);
    a.array[0] = 42;

    bn_zero(&b);
    b.array[0] = 42;

    bn_mulmod(&result, &a, &b, &n);
    printf("42 * 42 mod n[0:8] = result.array[0] = %u\n", result.array[0]);
    printf("n.array[0] = %08x\n", n.array[0]);

    return 0;
}
