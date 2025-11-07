#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "bignum_tiny.h"

void print_bn(const char *label, const bn_t *a) {
    printf("%s: ", label);
    int started = 0;
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (a->array[i] != 0 || started) {
            printf("%08x", a->array[i]);
            started = 1;
        }
    }
    if (!started) printf("0");
    printf("\n");
}

int main() {
    bn_t base, exp, mod, result;

    printf("Testing simple modexp: 2^10 mod 100\n\n");

    /* base = 2 */
    bn_zero(&base);
    base.array[0] = 2;
    print_bn("Base", &base);

    /* exp = 10 */
    bn_zero(&exp);
    exp.array[0] = 10;
    print_bn("Exp ", &exp);

    /* mod = 100 */
    bn_zero(&mod);
    mod.array[0] = 100;
    print_bn("Mod ", &mod);

    /* result = 2^10 mod 100 = 1024 mod 100 = 24 */
    bn_modexp(&result, &base, &exp, &mod);
    print_bn("Result (expected 24)", &result);
    printf("Result value: %u (expected 24)\n\n", result.array[0]);

    /* Test 2: DH-like - 2^256 mod (large prime) */
    printf("Testing DH-like: 2^256 mod prime\n");

    /* Use a smaller prime for testing: 257 (prime) */
    bn_zero(&mod);
    mod.array[0] = 257;

    bn_zero(&exp);
    exp.array[0] = 256;

    bn_zero(&base);
    base.array[0] = 2;

    bn_modexp(&result, &base, &exp, &mod);
    print_bn("Result", &result);
    printf("Result value: %u\n", result.array[0]);

    /* Fermat's little theorem: 2^256 mod 257 = 2^(p-1) mod p = 1 (for prime p) */
    printf("Expected: 1 (by Fermat's little theorem)\n");

    return 0;
}
