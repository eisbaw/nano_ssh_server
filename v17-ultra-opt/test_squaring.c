#include <stdio.h>
#include <stdint.h>
#include "bignum_tiny.h"
#include "diffie_hellman.h"

void print_bn(const char *label, const bn_t *a) {
    printf("%s: ", label);
    int started = 0;
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (a->array[i] != 0 || started || i < 4) {
            printf("%08x", a->array[i]);
            started = 1;
        }
    }
    printf("\n");
}

int main() {
    bn_t value, prime;
    extern const uint8_t dh_group14_prime[256];

    printf("=== Testing repeated squaring ===\n\n");

    /* Load prime */
    bn_from_bytes(&prime, dh_group14_prime, 256);
    printf("✓ Loaded DH prime\n\n");

    /* Start with 2 */
    bn_zero(&value);
    value.array[0] = 2;

    /* Square it 40 times and print every 5 iterations */
    for (int i = 0; i < 40; i++) {
        if (i % 5 == 0 || i == 32 || i == 33) {
            printf("After %2d squarings: ", i);
            print_bn("", &value);

            if (bn_is_zero(&value)) {
                printf("✗ ERROR: value became zero!\n");
                return 1;
            }
        }

        /* value = value * value mod prime */
        bn_mulmod(&value, &value, &value, &prime);
    }

    printf("\nFinal value after 40 squarings: ");
    print_bn("", &value);

    if (bn_is_zero(&value)) {
        printf("✗ FAIL: Value is zero\n");
        return 1;
    } else {
        printf("✓ PASS: Value is non-zero\n");
        return 0;
    }
}
