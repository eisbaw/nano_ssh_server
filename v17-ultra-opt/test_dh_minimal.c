#include <stdio.h>
#include <stdint.h>
#include "bignum_tiny.h"
#include "diffie_hellman.h"

void print_bn_full(const char *label, const bn_t *a) {
    printf("%s:\n  ", label);
    int non_zero_found = 0;
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (a->array[i] != 0 || non_zero_found || i == 0) {
            printf("%08x ", a->array[i]);
            non_zero_found = 1;
            if ((BN_WORDS - 1 - i) % 8 == 7) printf("\n  ");
        }
    }
    printf("\n");
}

int main() {
    bn_t base, exp, prime, result;
    extern const uint8_t dh_group14_prime[256];

    printf("=== Minimal DH Test: 2^5 mod large_prime ===\n\n");

    /* Load DH prime */
    bn_from_bytes(&prime, dh_group14_prime, 256);
    printf("Prime loaded\n");
    print_bn_full("Prime (first few words)", &prime);

    /* Set base = 2 */
    bn_zero(&base);
    base.array[0] = 2;
    print_bn_full("Base (should be 2)", &base);

    /* Set exp = 5 */
    bn_zero(&exp);
    exp.array[0] = 5;
    print_bn_full("Exp (should be 5)", &exp);

    /* Compute 2^5 mod prime (should be 32) */
    printf("\nCalling bn_modexp...\n");
    bn_modexp(&result, &base, &exp, &prime);
    print_bn_full("Result (should be 32)", &result);

    if (result.array[0] == 32 && result.array[1] == 0) {
        printf("\n✓ SUCCESS: 2^5 mod p = 32\n");
        return 0;
    } else {
        printf("\n✗ FAILED: Expected 32, got %u\n", result.array[0]);
        return 1;
    }
}
