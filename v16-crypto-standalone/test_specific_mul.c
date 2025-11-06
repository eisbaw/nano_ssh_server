#include <stdio.h>
#include <stdint.h>
#include "bignum_simple.h"

int main() {
    /* Test: 3111696^2 mod n */
    /* Expected: 9682651996416 = 0x8cc6b086100 */
    /* Words: [0]=0x6b086100, [1]=0x000008cc */

    bn_t a, mod, result;
    bn_2x_t product;

    /* Load n (modulus) */
    uint8_t n_bytes[256] = {
        0xa7, 0x3e, 0x9d, 0x97, 0x8a, 0xeb, 0xa1, 0x12,
        0x40, 0x5d, 0x96, 0x3c, 0xc7, 0x66, 0x5c, 0xa7,
        0xde, 0xe3, 0xbf, 0x6b, 0xeb, 0xf4, 0x47, 0x20,
        0xc7, 0x23, 0xe0, 0x14, 0x35, 0xaf, 0xc5, 0x35,
        /* Rest filled with pattern from RSA key... let me just use first 32 bytes */
    };
    
    /* For simplicity, use a smaller modulus for this test */
    bn_zero(&mod);
    for (int i = 0; i < 64; i++) {
        mod.array[i] = 0xFFFFFFFF;  /* Very large modulus */
    }

    /* a = 3111696 = 0x002f7b10 */
    bn_zero(&a);
    a.array[0] = 0x002f7b10;

    printf("Testing: 3111696^2\n");
    printf("a.array[0] = 0x%08x (%u)\n\n", a.array[0], a.array[0]);

    /* Step 1: Multiply */
    bn_mul_wide(&product, &a, &a);
    
    printf("After bn_mul_wide (3111696 * 3111696):\n");
    printf("product.array[0] = 0x%08x\n", product.array[0]);
    printf("product.array[1] = 0x%08x\n", product.array[1]);
    printf("product.array[2] = 0x%08x\n", product.array[2]);
    printf("\nExpected:\n");
    printf("product.array[0] = 0x6b086100\n");
    printf("product.array[1] = 0x000008cc\n");
    printf("product.array[2] = 0x00000000\n");
    
    /* Manual calculation: 3111696^2 = 9,682,651,996,416 */
    uint64_t check = 3111696ULL * 3111696ULL;
    printf("\nManual uint64_t calculation: %llu\n", check);
    printf("Manual in hex: 0x%llx\n", check);
    printf("Low 32 bits: 0x%08x\n", (uint32_t)(check & 0xFFFFFFFF));
    printf("High 32 bits: 0x%08x\n", (uint32_t)(check >> 32));

    /* Step 2: Now test with actual RSA modulus */
    /* Load real modulus */
    uint8_t rsa_n[256];
    memset(rsa_n, 0, 256);
    rsa_n[0] = 0xa7; rsa_n[1] = 0x3e; rsa_n[2] = 0x9d; rsa_n[3] = 0x97;
    rsa_n[4] = 0x8a; rsa_n[5] = 0xeb; rsa_n[6] = 0xa1; rsa_n[7] = 0x12;
    /* ... abbreviated for test */
    
    return 0;
}
