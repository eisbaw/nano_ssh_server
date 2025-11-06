#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "bignum_tiny.h"
#include "diffie_hellman.h"

int main() {
    uint8_t priv_bytes[256], pub_bytes[256];
    bn_t priv, pub, prime, generator;

    /* Test 1: Simple exponent that we know works */
    printf("=== Test 1: Exponent = 256 ===\n");
    memset(priv_bytes, 0, 256);
    priv_bytes[254] = 1;  /* 256 */

    bn_from_bytes(&priv, priv_bytes, 256);
    bn_from_bytes(&prime, dh_group14_prime, 256);
    bn_zero(&generator);
    generator.array[0] = 2;

    bn_modexp(&pub, &generator, &priv, &prime);

    bn_to_bytes(&pub, pub_bytes, 256);

    if (bn_is_zero(&pub)) {
        printf("❌ Result is ZERO\n\n");
    } else {
        printf("✅ Result is non-zero\n");
        printf("Non-zero words:\n");
        for (int i = 0; i < BN_WORDS; i++) {
            if (pub.array[i] != 0) {
                printf("  word[%d] = 0x%08x\n", i, pub.array[i]);
            }
        }
        printf("\n");
    }

    /* Test 2: Slightly larger exponent */
    printf("=== Test 2: Exponent = 65536 ===\n");
    memset(priv_bytes, 0, 256);
    priv_bytes[253] = 1;  /* 65536 */

    bn_from_bytes(&priv, priv_bytes, 256);
    bn_modexp(&pub, &generator, &priv, &prime);
    bn_to_bytes(&pub, pub_bytes, 256);

    if (bn_is_zero(&pub)) {
        printf("❌ Result is ZERO\n\n");
    } else {
        printf("✅ Result is non-zero\n");
        printf("Non-zero words:\n");
        for (int i = 0; i < BN_WORDS; i++) {
            if (pub.array[i] != 0) {
                printf("  word[%d] = 0x%08x\n", i, pub.array[i]);
            }
        }
        printf("\n");
    }

    /* Test 3: Even larger */
    printf("=== Test 3: Exponent = 16777216 ===\n");
    memset(priv_bytes, 0, 256);
    priv_bytes[252] = 1;  /* 2^24 */

    bn_from_bytes(&priv, priv_bytes, 256);
    bn_modexp(&pub, &generator, &priv, &prime);
    bn_to_bytes(&pub, pub_bytes, 256);

    if (bn_is_zero(&pub)) {
        printf("❌ Result is ZERO\n\n");
    } else {
        printf("✅ Result is non-zero\n");
        printf("Non-zero words:\n");
        for (int i = 0; i < BN_WORDS; i++) {
            if (pub.array[i] != 0) {
                printf("  word[%d] = 0x%08x\n", i, pub.array[i]);
            }
        }
        printf("\n");
    }

    return 0;
}
