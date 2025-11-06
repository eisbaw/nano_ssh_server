#include <stdio.h>
#include <stdint.h>
#include "rsa.h"

int main() {
    rsa_key_t key;
    bn_t m, c;

    rsa_init_key(&key);

    bn_zero(&m);
    m.array[0] = 42;

    printf("Computing c = 42^65537 mod n...\n");
    bn_modexp(&c, &m, &key.e, &key.n);

    int is_zero = 1;
    for (int i = 0; i < BN_WORDS; i++) {
        if (c.array[i] != 0) {
            is_zero = 0;
            break;
        }
    }

    if (is_zero) {
        printf("Result is ALL ZEROS - THIS IS WRONG\n");
    } else {
        printf("Result is non-zero:\n");
        for (int i = BN_WORDS - 1; i >= 0; i--) {
            if (c.array[i] != 0) {
                printf("  c.array[%d] = %08x\n", i, c.array[i]);
                if (i < BN_WORDS - 5) break;
            }
        }
    }

    /* Now convert to bytes and check */
    uint8_t c_bytes[256];
    bn_to_bytes(&c, c_bytes, 256);
    
    printf("\nFirst 32 bytes: ");
    for (int i = 0; i < 32; i++) {
        printf("%02x", c_bytes[i]);
    }
    printf("\n");

    return 0;
}
