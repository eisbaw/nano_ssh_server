#include <stdio.h>
#include <stdint.h>
#include "bignum_simple.h"
#include "rsa.h"

void print_bn_words(const char *name, const bn_t *x, int num_words) {
    printf("%s: ", name);
    int found_nonzero = 0;
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (x->array[i] != 0 || found_nonzero) {
            printf("[%d]=0x%08x ", i, x->array[i]);
            found_nonzero = 1;
            if (i > 0 && BN_WORDS - i >= num_words) break;
        }
    }
    if (!found_nonzero) printf("[0]=0x00000000");
    printf("\n");
}

int main() {
    rsa_key_t key;
    bn_t m, result, base_copy, temp;

    rsa_init_key(&key);

    bn_zero(&m);
    m.array[0] = 42;

    /* Initialize */
    bn_zero(&result);
    result.array[0] = 1;
    base_copy = m;

    printf("Testing iterations 0-5 of 42^65537 mod n\n\n");

    /* Iteration 0 */
    printf("=== Iteration 0 ===\n");
    print_bn_words("base_copy BEFORE square", &base_copy, 3);
    bn_mulmod(&temp, &result, &base_copy, &key.n);  /* result *= base */
    result = temp;
    bn_mulmod(&temp, &base_copy, &base_copy, &key.n);  /* base *= base */
    base_copy = temp;
    print_bn_words("base_copy AFTER square", &base_copy, 3);
    printf("Expected: [0]=0x000006e4 (1764)\n\n");

    /* Iteration 1 */
    printf("=== Iteration 1 ===\n");
    print_bn_words("base_copy BEFORE square", &base_copy, 3);
    bn_mulmod(&temp, &base_copy, &base_copy, &key.n);
    base_copy = temp;
    print_bn_words("base_copy AFTER square", &base_copy, 3);
    printf("Expected: [0]=0x002f7b10 (3111696)\n\n");

    /* Iteration 2 */
    printf("=== Iteration 2 ===\n");
    print_bn_words("base_copy BEFORE square", &base_copy, 3);
    bn_mulmod(&temp, &base_copy, &base_copy, &key.n);
    base_copy = temp;
    print_bn_words("base_copy AFTER square", &base_copy, 3);
    printf("Expected: [0]=0x6b086100, [1]=0x000008cc (from Python: 9682651996416)\n\n");

    /* Iteration 3 */
    printf("=== Iteration 3 ===\n");
    print_bn_words("base_copy BEFORE square", &base_copy, 3);
    printf("Expected BEFORE: [0]=0x6b086100, [1]=0x000008cc\n");
    bn_mulmod(&temp, &base_copy, &base_copy, &key.n);
    base_copy = temp;
    print_bn_words("base_copy AFTER square", &base_copy, 3);
    printf("Expected AFTER: [0]=0x34c10000, higher words non-zero\n\n");

    /* Iteration 4 */
    printf("=== Iteration 4 ===\n");
    print_bn_words("base_copy BEFORE square", &base_copy, 3);
    bn_mulmod(&temp, &base_copy, &base_copy, &key.n);
    base_copy = temp;
    print_bn_words("base_copy AFTER square", &base_copy, 5);
    printf("Expected AFTER: [0]=0x00000000, but higher words should be non-zero!\n\n");

    /* Iteration 5 */
    printf("=== Iteration 5 ===\n");
    print_bn_words("base_copy BEFORE square", &base_copy, 5);
    bn_mulmod(&temp, &base_copy, &base_copy, &key.n);
    base_copy = temp;
    print_bn_words("base_copy AFTER square", &base_copy, 5);
    printf("Expected AFTER: [0]=0x00000000, but higher words should still be non-zero!\n\n");

    return 0;
}
