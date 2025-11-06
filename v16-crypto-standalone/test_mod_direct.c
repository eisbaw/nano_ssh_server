#include <stdio.h>
#include <stdint.h>
#include "bignum_simple.h"
#include "rsa.h"

int main() {
    rsa_key_t key;
    bn_t a, result;
    bn_2x_t product;

    rsa_init_key(&key);

    /* Test: 3111696^2 mod n */
    /* Expected: 9682651996416 = 0x8ce6b086100 */
    /* Low 32 bits: 0x6b086100 */

    bn_zero(&a);
    a.array[0] = 0x002f7b10;  /* 3111696 */

    printf("Test: (3111696)^2 mod n\n");
    printf("Expected: array[0]=0x6b086100, array[1]=0x000008ce\n\n");

    /* Multiply */
    bn_mul_wide(&product, &a, &a);
    printf("After bn_mul_wide:\n");
    printf("  product.array[0] = 0x%08x\n", product.array[0]);
    printf("  product.array[1] = 0x%08x\n", product.array[1]);

    /* Reduce */
    bn_mod_simple(&result, &product, &key.n);
    printf("\nAfter bn_mod_simple:\n");
    printf("  result.array[0] = 0x%08x\n", result.array[0]);
    printf("  result.array[1] = 0x%08x\n", result.array[1]);

    if (result.array[0] == 0x6b086100 && result.array[1] == 0x000008ce) {
        printf("\n✓ CORRECT\n");
    } else {
        printf("\n✗ WRONG\n");
    }

    return 0;
}
