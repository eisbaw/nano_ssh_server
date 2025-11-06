#include <stdio.h>
#include <stdint.h>
#include "bignum_simple.h"
#include "rsa.h"

void test_exp(uint32_t exp_val, const char *name) {
    rsa_key_t key;
    bn_t m, exp, c;

    rsa_init_key(&key);

    bn_zero(&m);
    m.array[0] = 2;  /* Use base=2 for easier testing */

    bn_zero(&exp);
    exp.array[0] = exp_val;

    bn_modexp(&c, &m, &exp, &key.n);

    int is_zero = 1;
    for (int i = 0; i < BN_WORDS; i++) {
        if (c.array[i] != 0) {
            is_zero = 0;
            break;
        }
    }

    if (is_zero) {
        printf("%s: ✗ ZERO\n", name);
    } else {
        printf("%s: ✓ non-zero (c.array[0]=0x%08x)\n", name, c.array[0]);
    }
}

int main() {
    printf("Testing various exponent sizes:\n\n");

    test_exp(3, "2^3");
    test_exp(7, "2^7");
    test_exp(15, "2^15");
    test_exp(31, "2^31");
    test_exp(63, "2^63");
    test_exp(127, "2^127");
    test_exp(255, "2^255");
    test_exp(511, "2^511");
    test_exp(1023, "2^1023");
    test_exp(2047, "2^2047");
    test_exp(4095, "2^4095");
    test_exp(8191, "2^8191");
    test_exp(16383, "2^16383");
    test_exp(32767, "2^32767");
    test_exp(65535, "2^65535");
    test_exp(65537, "2^65537");

    return 0;
}
