/*
 * Comprehensive bn_modexp test with full tracing
 * Known reference: 2^17 mod prime (can verify with calculator)
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "bignum_tiny.h"
#include "diffie_hellman.h"

void print_bn(const char *label, const bn_t *bn) {
    printf("%s: ", label);
    int found = 0;
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (bn->array[i] != 0 || found) {
            printf("%08x ", bn->array[i]);
            found = 1;
        }
    }
    if (!found) printf("00000000");
    printf("\n");
}

/* Instrumented modexp with full tracing */
void bn_modexp_traced(bn_t *r, const bn_t *base, const bn_t *exp, const bn_t *mod) {
    bn_t result, temp_base, temp;

    printf("\n=== Starting bn_modexp ===\n");
    print_bn("base", base);
    print_bn("exp", exp);
    print_bn("mod (first 3 words)", mod);
    printf("mod.array[0-2]: %08x %08x %08x\n", mod->array[0], mod->array[1], mod->array[2]);

    /* Initialize */
    bn_zero(&result);
    bn_zero(&temp_base);
    bn_zero(&temp);

    result.array[0] = 1;
    printf("\nInitialized result = 1\n");
    print_bn("result", &result);

    /* temp_base = base % mod */
    bn_mod(&temp_base, base, mod);
    printf("\nAfter temp_base = base %% mod:\n");
    print_bn("temp_base", &temp_base);

    assert(!bn_is_zero(&temp_base) && "temp_base should not be zero after initial mod");

    int total_iterations = 0;
    int bits_processed = 0;

    /* Binary exponentiation */
    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t exp_word = exp->array[i];

        if (exp_word != 0) {
            printf("\n--- Processing exp word[%d] = 0x%08x ---\n", i, exp_word);
        }

        for (int j = 0; j < 32; j++) {
            bits_processed++;

            if (exp_word & 1) {
                printf("\n  ** Bit %d (word[%d] bit %d) is SET **\n", bits_processed-1, i, j);
                printf("  Before multiply:\n");
                print_bn("    result", &result);
                print_bn("    temp_base", &temp_base);

                /* result = (result * temp_base) % mod */
                bn_mul(&temp, &result, &temp_base);

                printf("  After bn_mul(&temp, &result, &temp_base):\n");
                print_bn("    temp", &temp);

                assert(!bn_is_zero(&temp) && "temp should not be zero after multiply");

                bn_mod(&result, &temp, mod);

                printf("  After bn_mod(&result, &temp, mod):\n");
                print_bn("    result", &result);

                assert(!bn_is_zero(&result) && "result should not be zero after mod");

                total_iterations++;
            }

            /* Square temp_base: temp_base = (temp_base * temp_base) % mod */
            if (j < 31 || i < BN_WORDS - 1) {  /* Don't square on last iteration */
                if (total_iterations <= 20) {  /* Only trace first 20 */
                    printf("  Squaring temp_base (iteration %d, bit %d):\n", total_iterations, bits_processed-1);
                    printf("    Before square:\n");
                    print_bn("      temp_base", &temp_base);
                }

                bn_mul(&temp, &temp_base, &temp_base);

                if (total_iterations <= 20) {
                    printf("    After bn_mul(&temp, &temp_base, &temp_base):\n");
                    print_bn("      temp", &temp);

                    if (bn_is_zero(&temp)) {
                        printf("    ❌ ERROR: temp is ZERO after squaring!\n");
                        printf("    This means bn_mul(%u^2) returned 0\n", temp_base.array[0]);
                        assert(0 && "CRITICAL: bn_mul returned zero when squaring");
                    }
                }

                bn_mod(&temp_base, &temp, mod);

                if (total_iterations <= 20) {
                    printf("    After bn_mod(&temp_base, &temp, mod):\n");
                    print_bn("      temp_base", &temp_base);

                    if (bn_is_zero(&temp_base)) {
                        printf("    ❌ ERROR: temp_base is ZERO after mod!\n");
                        printf("    This means bn_mod(large_number, prime) returned 0\n");
                        assert(0 && "CRITICAL: temp_base became zero after mod");
                    }
                }
            }

            exp_word >>= 1;
        }
    }

    printf("\n=== Finished bn_modexp ===\n");
    printf("Total iterations with bit set: %d\n", total_iterations);
    print_bn("Final result", &result);

    *r = result;
}

int main() {
    uint8_t priv_bytes[256];
    bn_t priv, pub, prime, generator;

    printf("========================================\n");
    printf("Test: 2^17 mod DH_GROUP14_PRIME\n");
    printf("========================================\n");

    /* Exponent = 17 */
    memset(priv_bytes, 0, 256);
    priv_bytes[255] = 17;

    bn_from_bytes(&priv, priv_bytes, 256);
    bn_from_bytes(&prime, dh_group14_prime, 256);
    bn_zero(&generator);
    generator.array[0] = 2;

    printf("\nInput verification:\n");
    print_bn("exponent", &priv);
    printf("Expected: 17 = 0x11 = 0b10001\n");
    printf("This means we should process bits 0 and 4\n\n");

    /* Call instrumented modexp */
    bn_modexp_traced(&pub, &generator, &priv, &prime);

    printf("\n\n========================================\n");
    printf("Result verification:\n");
    printf("========================================\n");

    if (bn_is_zero(&pub)) {
        printf("❌ FAIL: Result is zero!\n");
        return 1;
    }

    print_bn("result", &pub);
    printf("result.array[0] = %u\n", pub.array[0]);
    printf("result.array[1] = %u\n", pub.array[1]);

    /* 2^17 = 131072 */
    /* Since prime is huge, 2^17 mod prime = 2^17 = 131072 */
    uint32_t expected = 131072;

    if (pub.array[0] == expected && pub.array[1] == 0) {
        printf("✅ PASS: 2^17 = %u\n", expected);
        return 0;
    } else {
        printf("❌ FAIL: Expected %u, got %u (word[0])\n", expected, pub.array[0]);
        return 1;
    }
}
