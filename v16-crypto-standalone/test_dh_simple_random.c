#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "bignum_tiny.h"
#include "diffie_hellman.h"
#include "csprng.h"

int main() {
    uint8_t priv_bytes[256], pub_bytes[256];
    bn_t priv, pub, prime, generator;

    /* Generate a simple random-ish private key (not full random) */
    memset(priv_bytes, 0, 256);
    /* Set a few bytes to simulate partial randomness */
    priv_bytes[248] = 0x01;
    priv_bytes[249] = 0x23;
    priv_bytes[250] = 0x45;
    priv_bytes[251] = 0x67;
    priv_bytes[252] = 0x89;
    priv_bytes[253] = 0xAB;
    priv_bytes[254] = 0xCD;
    priv_bytes[255] = 0xEF;

    printf("Testing with partially random key...\n");
    printf("Private key (last 16 bytes): ");
    for (int i = 240; i < 256; i++) {
        printf("%02x", priv_bytes[i]);
    }
    printf("\n\n");

    /* Convert and compute */
    bn_from_bytes(&priv, priv_bytes, 256);
    bn_from_bytes(&prime, dh_group14_prime, 256);
    bn_zero(&generator);
    generator.array[0] = 2;

    printf("Computing 2^priv mod prime...\n");
    bn_modexp(&pub, &generator, &priv, &prime);

    /* Check result */
    printf("Result is_zero: %d\n", bn_is_zero(&pub));
    printf("Result words (first 8 non-zero from end):\n");
    int count = 0;
    for (int i = BN_WORDS - 1; i >= 0 && count < 8; i--) {
        if (pub.array[i] != 0) {
            printf("  word[%d] = 0x%08x\n", i, pub.array[i]);
            count++;
        }
    }

    /* Convert to bytes */
    bn_to_bytes(&pub, pub_bytes, 256);

    printf("\nPublic key (first 32 bytes):\n  ");
    for (int i = 0; i < 32; i++) {
        printf("%02x", pub_bytes[i]);
    }
    printf("\n");

    printf("Public key (last 32 bytes):\n  ");
    for (int i = 224; i < 256; i++) {
        printf("%02x", pub_bytes[i]);
    }
    printf("\n");

    /* Check if valid */
    bn_t one;
    bn_zero(&one);
    one.array[0] = 1;

    if (bn_is_zero(&pub)) {
        printf("\n❌ Result is ZERO!\n");
        return 1;
    }

    if (bn_cmp(&pub, &one) <= 0) {
        printf("\n❌ Result <= 1\n");
        return 1;
    }

    printf("\n✅ Result looks valid (> 1)\n");
    return 0;
}
