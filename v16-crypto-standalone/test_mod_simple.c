#include <stdio.h>
#include <stdint.h>
#include "bignum_simple.h"
#include "rsa.h"

int main() {
    rsa_key_t key;
    bn_t a, result;
    bn_2x_t product;

    rsa_init_key(&key);

    /* Test case from iteration 2: 3111696^2 mod n */
    bn_zero(&a);
    a.array[0] = 0x002f7b10;  /* 3111696 */

    printf("Test: (3111696)^2 mod n\n\n");

    /* Step 1: Multiply */
    bn_mul_wide(&product, &a, &a);
    printf("After bn_mul_wide:\n");
    printf("  product.array[0] = 0x%08x\n", product.array[0]);
    printf("  product.array[1] = 0x%08x\n", product.array[1]);

    /* Step 2: Reduce mod n */
    bn_mod_simple(&result, &product, &key.n);
    printf("\nAfter bn_mod_simple:\n");
    printf("  result.array[0] = 0x%08x\n", result.array[0]);
    printf("  result.array[1] = 0x%08x\n", result.array[1]);

    /* Compare with Python */
    printf("\nExpected from Python:\n");
    printf("  3111696^2 = 9682651996416 = 0x8ce6b086100\n");
    printf("  Since 9682651996416 << n, result should equal 0x8ce6b086100\n");
    printf("  result.array[0] should be 0x6b086100\n");
    printf("  result.array[1] should be 0x000008ce\n");

    if (result.array[0] == 0x6b086100 && result.array[1] == 0x000008ce) {
        printf("\n✓ CORRECT\n");
    } else {
        printf("\n✗ WRONG\n");
    }

    /* Now test iteration 3: square the result */
    printf("\n\n==================\nTest: (9682651996416)^2 mod n\n\n");
    
    a = result;  /* Use previous result */
    printf("Input:\n");
    printf("  a.array[0] = 0x%08x\n", a.array[0]);
    printf("  a.array[1] = 0x%08x\n", a.array[1]);

    bn_mul_wide(&product, &a, &a);
    printf("\nAfter bn_mul_wide:\n");
    printf("  product (low 4 words):\n");
    for (int i = 0; i < 4; i++) {
        printf("    [%d] = 0x%08x\n", i, product.array[i]);
    }

    bn_mod_simple(&result, &product, &key.n);
    printf("\nAfter bn_mod_simple:\n");
    printf("  result (low 4 words):\n");
    for (int i = 0; i < 4; i++) {
        printf("    [%d] = 0x%08x\n", i, result.array[i]);
    }

    printf("\nExpected from Python: [0]=0x34c10000, [1-2]=non-zero\n");

    return 0;
}
