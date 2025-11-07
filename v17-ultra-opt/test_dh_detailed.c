#include <stdio.h>
#include <stdint.h>
#include "csprng.h"
#include "bignum_tiny.h"
#include "diffie_hellman.h"

void print_bn_short(const char *label, const bn_t *a) {
    printf("%s: ", label);
    /* Print last 4 words (128 bits) */
    for (int i = BN_WORDS - 1; i >= BN_WORDS - 4 && i >= 0; i--) {
        printf("%08x", a->array[i]);
    }
    printf("...\n");
}

void print_bytes_short(const char *label, const uint8_t *data, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < 16 && i < len; i++) {
        printf("%02x", data[i]);
    }
    printf("...\n");
}

int main() {
    bn_t prime, generator, priv_bn, pub_bn;
    uint8_t priv[256];
    uint8_t test_pub[256];

    printf("Testing DH Group14 key generation step-by-step\n\n");

    /* Load the DH prime */
    extern const uint8_t dh_group14_prime[256];
    bn_from_bytes(&prime, dh_group14_prime, 256);
    print_bn_short("Prime (top 128 bits)", &prime);
    print_bytes_short("Prime bytes", dh_group14_prime, 256);

    /* Set generator = 2 */
    bn_zero(&generator);
    generator.array[0] = 2;
    print_bn_short("Generator", &generator);

    /* Generate a simple private key for testing */
    printf("\nTest 1: Simple private key = 5\n");
    bn_zero(&priv_bn);
    priv_bn.array[0] = 5;

    /* Compute public = 2^5 mod prime */
    bn_modexp(&pub_bn, &generator, &priv_bn, &prime);
    print_bn_short("Public key (2^5 mod p)", &pub_bn);

    /* Convert to bytes */
    bn_to_bytes(&pub_bn, test_pub, 256);
    print_bytes_short("Public bytes", test_pub, 256);

    /* Test 2: Random private key */
    printf("\nTest 2: Random private key\n");
    if (random_bytes(priv, 256) < 0) {
        printf("ERROR: Failed to generate random bytes\n");
        return 1;
    }
    priv[0] &= 0x7F;  /* Clear top bit */

    print_bytes_short("Private bytes", priv, 256);

    /* Convert to bignum */
    bn_from_bytes(&priv_bn, priv, 256);
    print_bn_short("Private bignum", &priv_bn);

    /* Compute public = 2^priv mod prime */
    printf("Computing 2^priv mod prime...\n");
    bn_modexp(&pub_bn, &generator, &priv_bn, &prime);
    print_bn_short("Public bignum", &pub_bn);

    /* Convert to bytes */
    bn_to_bytes(&pub_bn, test_pub, 256);
    print_bytes_short("Public bytes", test_pub, 256);

    /* Check if zero */
    int all_zero = 1;
    for (int i = 0; i < 256; i++) {
        if (test_pub[i] != 0) {
            all_zero = 0;
            break;
        }
    }
    printf("Public key is %s\n", all_zero ? "ZERO (BUG!)" : "non-zero (OK)");

    return 0;
}
