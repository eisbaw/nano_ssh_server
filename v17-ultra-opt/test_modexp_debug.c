#include <stdio.h>
#include <stdint.h>
#include "bignum_tiny.h"
#include "diffie_hellman.h"

void print_bn(const char *label, const bn_t *a) {
    printf("%s: ", label);
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (a->array[i] != 0 || i == 0) {
            printf("%08x", a->array[i]);
            if (i > 0 && a->array[i] != 0) {
                /* Print rest of words */
                for (int j = i - 1; j >= 0; j--) {
                    printf("%08x", a->array[j]);
                }
                break;
            }
        }
    }
    printf("\n");
}

int main() {
    bn_t base, exp, result, prime;
    extern const uint8_t dh_group14_prime[256];

    printf("=== Testing bn_modexp with DH prime ===\n\n");

    /* Load prime */
    bn_from_bytes(&prime, dh_group14_prime, 256);
    printf("✓ Loaded DH prime\n\n");

    /* Test 1: 2^0 = 1 */
    printf("Test 1: 2^0 mod p\n");
    bn_zero(&base);
    base.array[0] = 2;
    bn_zero(&exp);

    bn_modexp(&result, &base, &exp, &prime);
    print_bn("Result", &result);
    printf("Expected: 1, Got: %u - %s\n\n", result.array[0],
           result.array[0] == 1 ? "✓ PASS" : "✗ FAIL");

    /* Test 2: 2^1 = 2 */
    printf("Test 2: 2^1 mod p\n");
    exp.array[0] = 1;

    bn_modexp(&result, &base, &exp, &prime);
    print_bn("Result", &result);
    printf("Expected: 2, Got: %u - %s\n\n", result.array[0],
           result.array[0] == 2 ? "✓ PASS" : "✗ FAIL");

    /* Test 3: 2^2 = 4 */
    printf("Test 3: 2^2 mod p\n");
    exp.array[0] = 2;

    bn_modexp(&result, &base, &exp, &prime);
    print_bn("Result", &result);
    printf("Expected: 4, Got: %u - %s\n\n", result.array[0],
           result.array[0] == 4 ? "✓ PASS" : "✗ FAIL");

    /* Test 4: 2^3 = 8 */
    printf("Test 4: 2^3 mod p\n");
    exp.array[0] = 3;

    bn_modexp(&result, &base, &exp, &prime);
    print_bn("Result", &result);
    printf("Expected: 8, Got: %u - %s\n\n", result.array[0],
           result.array[0] == 8 ? "✓ PASS" : "✗ FAIL");

    /* Test 5: 2^10 = 1024 */
    printf("Test 5: 2^10 mod p\n");
    exp.array[0] = 10;

    bn_modexp(&result, &base, &exp, &prime);
    print_bn("Result", &result);
    printf("Expected: 1024, Got: %u - %s\n\n", result.array[0],
           result.array[0] == 1024 ? "✓ PASS" : "✗ FAIL");

    /* Test 6: 2^32 (crosses word boundary) */
    printf("Test 6: 2^32 mod p\n");
    exp.array[0] = 32;

    bn_modexp(&result, &base, &exp, &prime);
    print_bn("Result", &result);
    printf("Expected: 2^32 = 0x100000000\n");
    printf("Got: high=%08x low=%08x - %s\n\n", result.array[1], result.array[0],
           (result.array[1] == 1 && result.array[0] == 0) ? "✓ PASS" : "✗ FAIL");

    /* Test 7: Large exponent (256 bits set) */
    printf("Test 7: 2^256 mod p\n");
    bn_zero(&exp);
    exp.array[0] = 256;

    bn_modexp(&result, &base, &exp, &prime);
    print_bn("Result", &result);
    int is_zero = bn_is_zero(&result);
    printf("Is zero: %s - %s\n\n", is_zero ? "YES" : "NO",
           is_zero ? "✗ FAIL" : "✓ PASS");

    return 0;
}
