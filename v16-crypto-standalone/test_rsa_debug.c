#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "rsa.h"

void print_bn_words(const char *name, const bn_t *x) {
    printf("%s words: ", name);
    int found_nonzero = 0;
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (x->array[i] != 0 || found_nonzero || i == 0) {
            printf("[%d]=%08x ", i, x->array[i]);
            found_nonzero = 1;
            if (found_nonzero && i > 0 && i < 3) break; // Show first few
        }
    }
    printf("\n");
}

int main() {
    rsa_key_t key;
    bn_t m, c, m2;

    printf("=== RSA Debug Test ===\n\n");

    /* Initialize key */
    rsa_init_key(&key);
    printf("Key initialized:\n");
    print_bn_words("n", &key.n);
    print_bn_words("e", &key.e);
    print_bn_words("d", &key.d);

    /* Create a test message */
    bn_zero(&m);
    m.array[0] = 0x42;
    printf("\nMessage:\n");
    print_bn_words("m", &m);

    /* Encrypt */
    printf("\nEncrypting c = m^e mod n...\n");
    bn_modexp(&c, &m, &key.e, &key.n);
    print_bn_words("c", &c);

    /* Decrypt */
    printf("\nDecrypting m2 = c^d mod n...\n");
    bn_modexp(&m2, &c, &key.d, &key.n);
    print_bn_words("m2", &m2);

    /* Compare */
    printf("\nComparison:\n");
    printf("m.array[0] = %u\n", m.array[0]);
    printf("m2.array[0] = %u\n", m2.array[0]);

    if (m.array[0] == m2.array[0]) {
        printf("✓ Match!\n");
    } else {
        printf("✗ Mismatch\n");
    }

    return 0;
}
