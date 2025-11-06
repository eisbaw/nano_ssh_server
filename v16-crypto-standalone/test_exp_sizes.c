#include <stdio.h>
#include <stdint.h>
#include "rsa.h"

void test_exp(uint32_t exp_val, const char *name) {
    rsa_key_t key;
    bn_t base, exp, result;

    rsa_init_key(&key);

    bn_zero(&base);
    base.array[0] = 2;  /* Use base=2 for easier verification */

    bn_zero(&exp);
    exp.array[0] = exp_val;

    printf("Test: 2^%u mod n... ", exp_val);

    bn_modexp(&result, &base, &exp, &key.n);

    int is_zero = 1;
    for (int i = 0; i < BN_WORDS; i++) {
        if (result.array[i] != 0) {
            is_zero = 0;
            break;
        }
    }

    if (is_zero) {
        printf("✗ FAIL (result is zero)\n");
    } else {
        printf("✓ PASS (non-zero result, array[0]=%u)\n", result.array[0]);
    }
}

int main() {
    test_exp(3, "2^3");
    test_exp(7, "2^7");
    test_exp(15, "2^15");
    test_exp(17, "2^17");
    test_exp(31, "2^31");
    test_exp(63, "2^63");
    test_exp(65535, "2^65535");
    test_exp(65537, "2^65537");

    return 0;
}
