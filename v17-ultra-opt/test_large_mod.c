#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "bignum_tiny.h"
#include "diffie_hellman.h"

void print_bn_short(const char *label, const bn_t *a) {
    printf("%s: ", label);
    for (int i = BN_WORDS - 1; i >= BN_WORDS - 4 && i >= 0; i--) {
        printf("%08x", a->array[i]);
    }
    printf("... (top 128 bits)\n");
}

int main() {
    bn_t a, b, result, prime;
    extern const uint8_t dh_group14_prime[256];

    printf("=== Testing Large Modular Operations ===\n\n");

    /* Load the actual DH prime */
    bn_from_bytes(&prime, dh_group14_prime, 256);
    print_bn_short("DH Prime", &prime);

    /* Test 1: 2 mod prime (should be 2) */
    printf("\nTest 1: 2 mod prime\n");
    bn_zero(&a);
    a.array[0] = 2;

    bn_mod(&result, &a, &prime);
    printf("Result: %u (expected 2)\n", result.array[0]);
    printf("Match: %s\n", result.array[0] == 2 ? "YES" : "NO");

    /* Test 2: (prime - 1) mod prime (should be prime - 1) */
    printf("\nTest 2: (prime - 1) mod prime\n");
    a = prime;
    bn_sub(&a, &a, &result);  /* a = prime - 2, but we want prime - 1 */
    a.array[0]++;  /* Now a = prime - 1 */

    print_bn_short("prime - 1", &a);

    bn_mod(&result, &a, &prime);
    print_bn_short("Result", &result);

    int match = (bn_cmp(&result, &a) == 0);
    printf("Match: %s\n", match ? "YES" : "NO");

    /* Test 3: (prime + 1) mod prime (should be 1) */
    printf("\nTest 3: (prime + 1) mod prime\n");
    a = prime;
    bn_zero(&b);
    b.array[0] = 1;
    bn_add(&a, &a, &b);  /* a = prime + 1 */

    print_bn_short("prime + 1", &a);

    bn_mod(&result, &a, &prime);
    printf("Result: %u (expected 1)\n", result.array[0]);
    printf("Match: %s\n", result.array[0] == 1 ? "YES" : "NO");

    /* Test 4: 2 * prime mod prime (should be 0) */
    printf("\nTest 4: 2 * prime mod prime\n");
    a = prime;
    bn_add(&a, &a, &prime);  /* a = 2 * prime */

    print_bn_short("2 * prime", &a);

    bn_mod(&result, &a, &prime);
    printf("Result is zero: %s (expected YES)\n", bn_is_zero(&result) ? "YES" : "NO");

    /* Test 5: 2 * 2 mod prime (should be 4) */
    printf("\nTest 5: 2 * 2 mod prime\n");
    bn_zero(&a);
    a.array[0] = 2;
    bn_zero(&b);
    b.array[0] = 2;

    bn_mul(&result, &a, &b);
    printf("2 * 2 = %u\n", result.array[0]);

    bn_mod(&result, &result, &prime);
    printf("Result: %u (expected 4)\n", result.array[0]);
    printf("Match: %s\n", result.array[0] == 4 ? "YES" : "NO");

    /* Test 6: Multiply two large numbers and check if result is truncated */
    printf("\nTest 6: Check for multiplication overflow\n");
    bn_zero(&a);
    a.array[63] = 0xFFFFFFFF;  /* Set high word */
    bn_zero(&b);
    b.array[63] = 2;

    print_bn_short("a", &a);
    print_bn_short("b", &b);

    bn_mul(&result, &a, &b);
    print_bn_short("a * b", &result);

    /* This should overflow beyond 2048 bits and wrap around */
    printf("Note: If result is truncated, we've found the bug!\n");

    return 0;
}
