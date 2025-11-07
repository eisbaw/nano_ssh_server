#include <stdio.h>
#include <stdint.h>
#include "csprng.h"
#include "bignum_tiny.h"
#include "diffie_hellman.h"

void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < (len < 32 ? len : 32); i++) {
        printf("%02x", data[i]);
    }
    if (len > 32) printf("...");
    printf("\n");
}

int main() {
    uint8_t priv[256], pub[256];

    printf("Testing DH key generation...\n\n");

    for (int test = 0; test < 5; test++) {
        printf("Test %d:\n", test + 1);

        if (dh_generate_keypair(priv, pub) != 0) {
            printf("  ERROR: dh_generate_keypair failed\n");
            continue;
        }

        print_hex("  Private key", priv, 256);
        print_hex("  Public key ", pub, 256);

        /* Check if public key is valid (> 1) */
        int all_zero = 1;
        for (int i = 0; i < 256; i++) {
            if (pub[i] != 0) {
                all_zero = 0;
                break;
            }
        }

        if (all_zero) {
            printf("  INVALID: Public key is zero!\n");
        } else if (pub[255] == 1 && pub[254] == 0) {
            /* Check if it's exactly 1 */
            int is_one = 1;
            for (int i = 0; i < 255; i++) {
                if (pub[i] != 0) {
                    is_one = 0;
                    break;
                }
            }
            if (is_one) {
                printf("  INVALID: Public key is one!\n");
            } else {
                printf("  OK: Public key looks valid\n");
            }
        } else {
            printf("  OK: Public key looks valid\n");
        }

        printf("\n");
    }

    return 0;
}
