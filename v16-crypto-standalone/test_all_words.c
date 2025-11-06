#include <stdio.h>
#include <stdint.h>
#include "bignum_simple.h"
#include "rsa.h"

void print_low_words(const char *name, const bn_t *x, int num_words) {
    printf("%s (low %d words): ", name, num_words);
    for (int i = num_words - 1; i >= 0; i--) {
        printf("[%d]=0x%08x ", i, x->array[i]);
    }
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

    printf("Testing iterations 0-3 of 42^65537 mod n\n");
    printf("Showing low 4 words of base_copy after each square\n\n");

    /* Iteration 0 */
    printf("=== Iteration 0 ===\n");
    bn_mulmod(&temp, &result, &base_copy, &key.n);
    result = temp;
    print_low_words("base_copy BEFORE", &base_copy, 4);
    bn_mulmod(&temp, &base_copy, &base_copy, &key.n);
    base_copy = temp;
    print_low_words("base_copy AFTER", &base_copy, 4);
    printf("Python: [0]=0x000006e4\n\n");

    /* Iteration 1 */
    printf("=== Iteration 1 ===\n");
    print_low_words("base_copy BEFORE", &base_copy, 4);
    bn_mulmod(&temp, &base_copy, &base_copy, &key.n);
    base_copy = temp;
    print_low_words("base_copy AFTER", &base_copy, 4);
    printf("Python: [0]=0x002f7b10\n\n");

    /* Iteration 2 */
    printf("=== Iteration 2 ===\n");
    print_low_words("base_copy BEFORE", &base_copy, 4);
    bn_mulmod(&temp, &base_copy, &base_copy, &key.n);
    base_copy = temp;
    print_low_words("base_copy AFTER", &base_copy, 4);
    printf("Python: [0]=0x6b086100, [1]=0x000008cc (9682651996416)\n\n");

    /* Iteration 3 */
    printf("=== Iteration 3 ===\n");
    print_low_words("base_copy BEFORE", &base_copy, 4);
    bn_mulmod(&temp, &base_copy, &base_copy, &key.n);
    base_copy = temp;
    print_low_words("base_copy AFTER", &base_copy, 4);
    printf("Python: [0]=0x34c10000, [1-3]=non-zero\n\n");

    return 0;
}
