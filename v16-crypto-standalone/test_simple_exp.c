#include <stdio.h>
#include <stdint.h>
#include "rsa.h"

int main() {
    rsa_key_t key;
    bn_t base, exp, result;

    rsa_init_key(&key);

    /* Test: 42^3 mod n (should be 74088) */
    bn_zero(&base);
    base.array[0] = 42;

    bn_zero(&exp);
    exp.array[0] = 3;  /* Simple exponent */

    printf("Test: 42^3 mod n\n");
    printf("Expected: 42^3 = 74088\n\n");

    bn_modexp(&result, &base, &exp, &key.n);

    printf("Result: result.array[0] = %u\n", result.array[0]);

    if (result.array[0] == 74088) {
        printf("✓ PASS\n");
    } else {
        printf("✗ FAIL\n");
    }

    /* Manual calculation */
    bn_t temp1, temp2;
    bn_mulmod(&temp1, &base, &base, &key.n);  /* 42^2 */
    printf("\nManual: 42^2 = %u\n", temp1.array[0]);
    
    bn_mulmod(&temp2, &temp1, &base, &key.n);  /* 42^3 */
    printf("Manual: 42^3 = %u\n", temp2.array[0]);

    return 0;
}
