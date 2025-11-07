#include <stdio.h>
#include <stdint.h>
#include "bignum_tiny.h"
#include "diffie_hellman.h"

void print_bn(const char *label, const bn_t *a) {
    printf("%s: ", label);
    int started = 0;
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (a->array[i] != 0 || started || i == 0) {
            printf("%08x", a->array[i]);
            started = 1;
            if ((BN_WORDS - 1 - i) % 8 == 7 && i > 0) printf(" ");
        }
    }
    printf("\n");
}

int main() {
    bn_t a, b, result, prime;
    extern const uint8_t dh_group14_prime[256];

    printf("=== Testing bn_mulmod ===\n\n");

    /* Load prime */
    bn_from_bytes(&prime, dh_group14_prime, 256);
    printf("✓ Loaded DH prime\n");

    /* Test 1: Simple case: 2 * 16 mod prime = 32 */
    printf("\nTest 1: 2 * 16 mod prime\n");
    bn_zero(&a);
    a.array[0] = 2;
    bn_zero(&b);
    b.array[0] = 16;

    bn_mulmod(&result, &a, &b, &prime);
    print_bn("Result", &result);
    printf("Expected: 32, Got: %u\n", result.array[0]);
    printf("Status: %s\n", result.array[0] == 32 ? "✓ PASS" : "✗ FAIL");

    /* Test 2: Larger numbers: 1000 * 2000 mod prime = 2000000 */
    printf("\nTest 2: 1000 * 2000 mod prime\n");
    bn_zero(&a);
    a.array[0] = 1000;
    bn_zero(&b);
    b.array[0] = 2000;

    bn_mulmod(&result, &a, &b, &prime);
    print_bn("Result", &result);
    printf("Expected: 2000000, Got: %u\n", result.array[0]);
    printf("Status: %s\n", result.array[0] == 2000000 ? "✓ PASS" : "✗ FAIL");

    /* Test 3: result * result mod prime (squaring) */
    printf("\nTest 3: 32 * 32 mod prime\n");
    bn_zero(&a);
    a.array[0] = 32;
    bn_zero(&b);
    b.array[0] = 32;

    bn_mulmod(&result, &a, &b, &prime);
    print_bn("Result", &result);
    printf("Expected: 1024, Got: %u\n", result.array[0]);
    printf("Status: %s\n", result.array[0] == 1024 ? "✓ PASS" : "✗ FAIL");

    /* Test 4: Try with one large number */
    printf("\nTest 4: Large * Small\n");
    bn_zero(&a);
    a.array[1] = 1;  /* 2^32 */
    bn_zero(&b);
    b.array[0] = 2;

    bn_mulmod(&result, &a, &b, &prime);
    print_bn("Result", &result);
    uint64_t expected = (1ULL << 33);  /* 2^33 */
    printf("Expected 2^33, high word: %u, low word: %u\n", result.array[1], result.array[0]);
    printf("Status: %s\n", (result.array[1] == 2 && result.array[0] == 0) ? "✓ PASS" : "✗ FAIL");

    return 0;
}
