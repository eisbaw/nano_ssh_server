#include <stdio.h>
#include <stdint.h>
#include "csprng.h"
#include "bignum_tiny.h"
#include "diffie_hellman.h"

void print_bytes(const char *label, const uint8_t *data, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < len && i < 32; i++) {
        printf("%02x", data[i]);
    }
    if (len > 32) printf("...");
    printf("\n");
}

int main() {
    bn_t base, priv_bn, prime, pub_bn;
    uint8_t priv[256];
    uint8_t pub[256];
    extern const uint8_t dh_group14_prime[256];

    printf("=== Testing DH with random private key ===\n\n");

    /* Load DH prime */
    bn_from_bytes(&prime, dh_group14_prime, 256);
    printf("✓ Loaded DH Group14 prime\n");

    /* Set base = 2 */
    bn_zero(&base);
    base.array[0] = 2;
    printf("✓ Generator = 2\n");

    /* Generate random private key */
    if (random_bytes(priv, 256) < 0) {
        printf("✗ Failed to generate random bytes\n");
        return 1;
    }
    priv[0] &= 0x7F;  /* Clear top bit to ensure < prime */
    print_bytes("Private key", priv, 256);

    /* Convert to bignum */
    bn_from_bytes(&priv_bn, priv, 256);
    printf("✓ Converted to bignum\n");

    /* Compute public = 2^priv mod prime */
    printf("\nComputing public key: 2^priv mod p...\n");
    bn_modexp(&pub_bn, &base, &priv_bn, &prime);
    printf("✓ Computation complete\n");

    /* Convert to bytes */
    bn_to_bytes(&pub_bn, pub, 256);
    print_bytes("Public key", pub, 256);

    /* Check if all zero */
    int all_zero = 1;
    for (int i = 0; i < 256; i++) {
        if (pub[i] != 0) {
            all_zero = 0;
            break;
        }
    }

    /* Check if all ones (would indicate -1 or error) */
    int all_ones = 1;
    for (int i = 0; i < 256; i++) {
        if (pub[i] != 0xFF) {
            all_ones = 0;
            break;
        }
    }

    if (all_zero) {
        printf("\n✗ FAILED: Public key is zero\n");
        return 1;
    } else if (all_ones) {
        printf("\n✗ FAILED: Public key is all 0xFF\n");
        return 1;
    } else if (pub[0] == 0 && pub[1] == 0 && pub[2] == 0) {
        printf("\n✗ FAILED: Public key looks suspiciously small\n");
        return 1;
    } else {
        printf("\n✓ SUCCESS: Public key looks valid!\n");
        return 0;
    }
}
