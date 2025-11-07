#include <stdio.h>
#include <stdint.h>
#include "bignum_tiny.h"
#include "diffie_hellman.h"

void print_bn_short(const char *label, const bn_t *a) {
    printf("%s: ", label);
    int started = 0;
    for (int i = BN_WORDS - 1; i >= 0 && i >= BN_WORDS - 8; i--) {
        printf("%08x", a->array[i]);
        started = 1;
    }
    printf("... (top 8 words)\n");
}

int main() {
    bn_t base, exp, result, prime;
    extern const uint8_t dh_group14_prime[256];

    printf("=== Testing with large exponents ===\n\n");

    /* Load prime */
    bn_from_bytes(&prime, dh_group14_prime, 256);
    printf("✓ Loaded DH prime\n\n");

    /* Base = 2 */
    bn_zero(&base);
    base.array[0] = 2;

    /* Test 1: Exponent with bit in word[1] */
    printf("Test 1: 2^(2^32 + 1) mod p\n");
    bn_zero(&exp);
    exp.array[0] = 1;
    exp.array[1] = 1;  /* 2^32 + 1 */

    bn_modexp(&result, &base, &exp, &prime);
    print_bn_short("Result", &result);
    int is_zero = bn_is_zero(&result);
    printf("Is zero: %s - %s\n\n", is_zero ? "YES" : "NO",
           is_zero ? "✗ FAIL" : "✓ PASS");

    /* Test 2: Exponent in word[10] */
    printf("Test 2: Exponent with bit in word[10]\n");
    bn_zero(&exp);
    exp.array[10] = 1;

    bn_modexp(&result, &base, &exp, &prime);
    print_bn_short("Result", &result);
    is_zero = bn_is_zero(&result);
    printf("Is zero: %s - %s\n\n", is_zero ? "YES" : "NO",
           is_zero ? "✗ FAIL" : "✓ PASS");

    /* Test 3: Fill lower half of exponent with random-ish pattern */
    printf("Test 3: Exponent with multiple words set\n");
    bn_zero(&exp);
    for (int i = 0; i < 32; i++) {
        exp.array[i] = 0x12345678 + i;
    }

    bn_modexp(&result, &base, &exp, &prime);
    print_bn_short("Result", &result);
    is_zero = bn_is_zero(&result);
    printf("Is zero: %s - %s\n\n", is_zero ? "YES" : "NO",
           is_zero ? "✗ FAIL" : "✓ PASS");

    /* Test 4: All words set to 1 (huge exponent) */
    printf("Test 4: Exponent = 0x111...111 (all words = 1)\n");
    bn_zero(&exp);
    for (int i = 0; i < BN_WORDS; i++) {
        exp.array[i] = 1;
    }

    bn_modexp(&result, &base, &exp, &prime);
    print_bn_short("Result", &result);
    is_zero = bn_is_zero(&result);
    printf("Is zero: %s - %s\n\n", is_zero ? "YES" : "NO",
           is_zero ? "✗ FAIL" : "✓ PASS");

    return 0;
}
