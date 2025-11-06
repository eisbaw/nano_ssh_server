#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "bignum_tiny.h"
#include "diffie_hellman.h"

/* Modified bn_modexp with debug tracing */
static void bn_modexp_debug(bn_t *r, const bn_t *base, const bn_t *exp, const bn_t *mod) {
    bn_t result, temp_base, temp;

    /* Initialize all variables */
    bn_zero(&result);
    bn_zero(&temp_base);
    bn_zero(&temp);

    /* result = 1 */
    result.array[0] = 1;

    /* temp_base = base % mod */
    bn_mod(&temp_base, base, mod);
    printf("After temp_base = base %% mod:\n");
    printf("  temp_base.array[0] = %u\n", temp_base.array[0]);
    printf("  temp_base is_zero = %d\n", bn_is_zero(&temp_base));

    int iterations = 0;
    int actual_iterations = 0;

    /* Binary exponentiation (right-to-left) */
    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t exp_word = exp->array[i];

        if (exp_word != 0 && iterations < 3) {
            printf("\nProcessing exp word[%d] = 0x%08x\n", i, exp_word);
        }

        for (int j = 0; j < 32; j++) {
            if (exp_word & 1) {
                actual_iterations++;

                if (iterations < 5) {
                    printf("  Bit set at i=%d, j=%d\n", i, j);
                    printf("    Before: result.array[0]=%u, temp_base.array[0]=%u\n",
                           result.array[0], temp_base.array[0]);
                }

                /* result = (result * temp_base) % mod */
                bn_mul(&temp, &result, &temp_base);

                if (iterations < 5) {
                    printf("    After mul: temp.array[0]=%u, temp is_zero=%d\n",
                           temp.array[0], bn_is_zero(&temp));
                }

                bn_mod(&result, &temp, mod);

                if (iterations < 5) {
                    printf("    After mod: result.array[0]=%u, result is_zero=%d\n",
                           result.array[0], bn_is_zero(&result));
                }

                iterations++;
            }

            /* temp_base = (temp_base * temp_base) % mod */
            bn_mul(&temp, &temp_base, &temp_base);
            bn_mod(&temp_base, &temp, mod);

            if (bn_is_zero(&temp_base)) {
                printf("  ❌ temp_base became ZERO at i=%d, j=%d!\n", i, j);
                *r = result;
                return;
            }

            exp_word >>= 1;

            /* Early exit if result becomes 0 */
            if (bn_is_zero(&result)) {
                printf("  ❌ result became ZERO at i=%d, j=%d!\n", i, j);
                *r = result;
                return;
            }
        }
    }

    printf("\nTotal iterations with exp bit set: %d\n", actual_iterations);
    printf("Final result.array[0] = %u\n", result.array[0]);
    printf("Final result is_zero = %d\n", bn_is_zero(&result));

    *r = result;
}

int main() {
    uint8_t priv_bytes[256];
    bn_t priv, pub, prime, generator;

    /* Simple test with known small exponent first */
    printf("=== Test 1: Exponent = 5 ===\n");
    memset(priv_bytes, 0, 256);
    priv_bytes[255] = 5;

    bn_from_bytes(&priv, priv_bytes, 256);
    bn_from_bytes(&prime, dh_group14_prime, 256);
    bn_zero(&generator);
    generator.array[0] = 2;

    bn_modexp_debug(&pub, &generator, &priv, &prime);

    printf("Result: %u (expected: 32)\n", pub.array[0]);

    printf("\n\n=== Test 2: Larger exponent = 0x12345678 ===\n");
    memset(priv_bytes, 0, 256);
    priv_bytes[252] = 0x12;
    priv_bytes[253] = 0x34;
    priv_bytes[254] = 0x56;
    priv_bytes[255] = 0x78;

    bn_from_bytes(&priv, priv_bytes, 256);

    bn_modexp_debug(&pub, &generator, &priv, &prime);

    printf("Result is_zero: %d\n", bn_is_zero(&pub));

    return 0;
}
