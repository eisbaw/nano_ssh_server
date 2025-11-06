#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "bignum_tiny.h"
#include "diffie_hellman.h"

void print_bn(const char *label, const bn_t *bn) {
    printf("%s: ", label);
    int found_nonzero = 0;
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (bn->array[i] != 0) found_nonzero = 1;
        if (found_nonzero) {
            printf("%08x ", bn->array[i]);
        }
    }
    if (!found_nonzero) printf("0");
    printf("\n");
}

int main() {
    uint8_t priv_bytes[256];
    bn_t priv, pub, prime, generator;

    /* Test with a random-looking but manageable private key */
    memset(priv_bytes, 0, 256);
    priv_bytes[252] = 0x12;
    priv_bytes[253] = 0x34;
    priv_bytes[254] = 0x56;
    priv_bytes[255] = 0x78;  /* Private key = 0x12345678 */

    printf("Testing modexp with private key = 0x12345678\n\n");

    /* Convert to bignum */
    bn_from_bytes(&priv, priv_bytes, 256);
    bn_from_bytes(&prime, dh_group14_prime, 256);
    bn_zero(&generator);
    generator.array[0] = 2;

    print_bn("Private key", &priv);
    print_bn("Prime", &prime);
    print_bn("Generator", &generator);

    printf("\nCalling bn_modexp...\n");
    bn_modexp(&pub, &generator, &priv, &prime);

    print_bn("Result", &pub);

    /* Check if result is zero or invalid */
    if (bn_is_zero(&pub)) {
        printf("\n❌ Result is ZERO!\n");
        return 1;
    }

    bn_t one;
    bn_zero(&one);
    one.array[0] = 1;

    if (bn_cmp(&pub, &one) <= 0) {
        printf("\n❌ Result <= 1 (INVALID)\n");
        return 1;
    }

    printf("\n✅ Result looks valid (> 1)\n");

    /* Convert to bytes and print */
    uint8_t pub_bytes[256];
    bn_to_bytes(&pub, pub_bytes, 256);

    printf("Public key bytes (first 32):\n  ");
    for (int i = 0; i < 32; i++) {
        printf("%02x", pub_bytes[i]);
    }
    printf("\n");

    return 0;
}
