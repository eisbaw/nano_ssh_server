#include <stdio.h>
#include <stdint.h>
#include "bignum_simple.h"
#include "rsa.h"

int main() {
    rsa_key_t key;
    bn_t result, base_copy, temp, m;

    rsa_init_key(&key);

    /* Simulate iterations 0-16 manually */
    bn_zero(&result);
    result.array[0] = 1;

    bn_zero(&base_copy);
    base_copy.array[0] = 42;

    /* Exponent is 65537 = 0x10001 = binary 10000000000000001 */
    /* So bit 0 is 1, bits 1-15 are 0, bit 16 is 1 */

    printf("Simulating 42^65537 mod n\n\n");

    /* Iteration 0: bit 0 = 1 */
    printf("Iteration 0: bit 0 = 1\n");
    bn_mulmod(&temp, &result, &base_copy, &key.n);  /* result *= base */
    result = temp;
    printf("  After multiply: result.array[0] = 0x%08x\n", result.array[0]);
    
    bn_mulmod(&temp, &base_copy, &base_copy, &key.n);  /* base^2 */
    base_copy = temp;
    printf("  After square: base_copy.array[0] = 0x%08x\n\n", base_copy.array[0]);

    /* Iterations 1-15: bits are 0, just square base */
    for (int i = 1; i <= 15; i++) {
        bn_mulmod(&temp, &base_copy, &base_copy, &key.n);
        base_copy = temp;
        if (i <= 3 || i == 15) {
            printf("Iteration %d: bit = 0, base_copy.array[0] = 0x%08x\n", i, base_copy.array[0]);
        } else if (i == 4) {
            printf("  ...\n");
        }
    }

    /* Iteration 16: bit 16 = 1 */
    printf("\nIteration 16: bit 16 = 1\n");
    printf("  Before multiply: result.array[0] = 0x%08x, base_copy.array[0-2]:\n", result.array[0]);
    for (int i = 0; i < 3; i++) {
        printf("    base_copy.array[%d] = 0x%08x\n", i, base_copy.array[i]);
    }
    
    bn_mulmod(&temp, &result, &base_copy, &key.n);
    result = temp;
    
    printf("  After multiply: result.array[0-2]:\n");
    for (int i = 0; i < 3; i++) {
        printf("    result.array[%d] = 0x%08x\n", i, result.array[i]);
    }

    /* Check if result is zero */
    int all_zero = 1;
    for (int i = 0; i < BN_WORDS; i++) {
        if (result.array[i] != 0) {
            all_zero = 0;
            break;
        }
    }

    if (all_zero) {
        printf("\n✗ Result became ALL ZEROS after iteration 16!\n");
    } else {
        printf("\n✓ Result is non-zero\n");
    }

    printf("\nExpected from Python: result.array[0] = 0xf91e0d65\n");

    return 0;
}
