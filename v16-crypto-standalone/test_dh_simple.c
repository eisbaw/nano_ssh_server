#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "bignum_tiny.h"
#include "diffie_hellman.h"
#include "csprng.h"

void print_bn(const char *label, const bn_t *bn) {
    printf("%s: ", label);
    for (int i = BN_WORDS - 1; i >= 0 && i >= BN_WORDS - 4; i--) {
        printf("%08x ", bn->array[i]);
    }
    printf("... ");
    for (int i = 3; i >= 0; i--) {
        printf("%08x ", bn->array[i]);
    }
    printf("\n");
}

int main() {
    uint8_t priv_bytes[256], pub_bytes[256];
    bn_t priv, pub, prime, generator;

    /* Test with a known private key */
    memset(priv_bytes, 0, 256);
    priv_bytes[0] = 0x4b;  // First byte from the failing test
    priv_bytes[1] = 0x0e;
    priv_bytes[2] = 0x3d;
    priv_bytes[3] = 0xd5;

    printf("Testing DH with partial key...\n");
    
    /* Convert to bignum */
    bn_from_bytes(&priv, priv_bytes, 256);
    bn_from_bytes(&prime, dh_group14_prime, 256);

    /* generator = 2 */
    bn_zero(&generator);
    generator.array[0] = 2;

    printf("Before modexp:\n");
    print_bn("  priv", &priv);
    print_bn("  generator", &generator);
    print_bn("  prime", &prime);

    /* Initialize pub to zero */
    bn_zero(&pub);

    /* Compute public = g^private mod p */
    bn_modexp(&pub, &generator, &priv, &prime);

    printf("After modexp:\n");
    print_bn("  pub", &pub);

    /* Check if zero */
    if (bn_is_zero(&pub)) {
        printf("❌ Result is ZERO!\n");
    } else {
        printf("✅ Result is non-zero\n");
    }

    /* Export to bytes */
    bn_to_bytes(&pub, pub_bytes, 256);

    printf("Public key bytes (first 32):\n  ");
    for (int i = 0; i < 32; i++) {
        printf("%02x", pub_bytes[i]);
    }
    printf("\n");

    return 0;
}
