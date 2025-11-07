#include <stdio.h>
#include <stdint.h>
#include "csprng.h"
#include "bignum_tiny.h"
#include "diffie_hellman.h"

void print_bytes_full(const char *label, const uint8_t *data, size_t len) {
    printf("%s:\n", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
        if ((i + 1) % 32 == 0) printf("\n");
    }
    if (len % 32 != 0) printf("\n");
}

int main() {
    bn_t priv_bn, prime, cmp_result;
    uint8_t priv[256];
    extern const uint8_t dh_group14_prime[256];

    printf("=== Checking private key vs prime ===\n\n");

    /* Load prime */
    bn_from_bytes(&prime, dh_group14_prime, 256);
    printf("✓ Loaded DH prime\n");
    print_bytes_full("DH Prime", dh_group14_prime, 256);

    /* Generate random private key */
    if (random_bytes(priv, 256) < 0) {
        printf("✗ Failed to generate random bytes\n");
        return 1;
    }
    priv[0] &= 0x7F;  /* Clear top bit */

    printf("\n");
    print_bytes_full("Private key (before mod)", priv, 256);

    /* Convert to bignum */
    bn_from_bytes(&priv_bn, priv, 256);

    /* Compare with prime */
    int cmp = bn_cmp(&priv_bn, &prime);
    printf("\nComparison: priv_bn %s prime\n",
           cmp < 0 ? "<" : (cmp > 0 ? ">" : "=="));

    /* If >= prime, reduce it */
    if (cmp >= 0) {
        printf("⚠ Private key >= prime, reducing...\n");
        bn_mod(&priv_bn, &priv_bn, &prime);
        bn_to_bytes(&priv_bn, priv, 256);
        print_bytes_full("Private key (after mod)", priv, 256);
    } else {
        printf("✓ Private key < prime, good to use\n");
    }

    /* Check if private key is zero */
    if (bn_is_zero(&priv_bn)) {
        printf("✗ ERROR: Private key is zero!\n");
        return 1;
    }

    /* Check if private key is 1 */
    if (priv_bn.array[0] == 1 && priv_bn.array[1] == 0 && priv_bn.array[2] == 0) {
        printf("⚠ WARNING: Private key is very small (likely 1)\n");
    }

    printf("\n✓ Private key looks valid\n");
    return 0;
}
