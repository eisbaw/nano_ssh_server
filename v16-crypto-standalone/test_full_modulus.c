#include <stdio.h>
#include <stdint.h>
#include "bignum_adapter.h"
#include "rsa.h"

int main() {
    rsa_key_t key;
    rsa_init_key(&key);

    printf("RSA Modulus (all 64 words):\n");
    for (int i = 0; i < BN_ARRAY_SIZE && i < 64; i++) {
        if (i % 4 == 0) printf("\n[%2d-%2d]: ", i, i+3);
        printf("0x%08x ", key.n.array[i]);
    }
    printf("\n\n");

    printf("Expected from rsa.h (last 32 bytes, big-endian):\n");
    printf("0xcb 0x62 0x70 0x1f 0x4a 0xe4 0x86 0xbf\n\n");

    printf("In little-endian words:\n");
    printf("array[0] should be: 0xbf86e44a (bytes [bf 86 e4 4a] reversed)\n");
    printf("array[1] should be: 0x1f7062cb (bytes [1f 70 62 cb] reversed)\n\n");

    printf("Actual:\n");
    printf("array[0] = 0x%08x\n", key.n.array[0]);
    printf("array[1] = 0x%08x\n", key.n.array[1]);

    /* Check if modulus is zero */
    int all_zero = 1;
    for (int i = 0; i < BN_ARRAY_SIZE; i++) {
        if (key.n.array[i] != 0) {
            all_zero = 0;
            break;
        }
    }

    if (all_zero) {
        printf("\nERROR: Modulus is all zeros!\n");
        return 1;
    }

    /* Check how many non-zero words */
    int nonzero_count = 0;
    int highest_nonzero = -1;
    for (int i = 0; i < BN_ARRAY_SIZE; i++) {
        if (key.n.array[i] != 0) {
            nonzero_count++;
            highest_nonzero = i;
        }
    }

    printf("\nNon-zero words: %d\n", nonzero_count);
    printf("Highest non-zero word index: %d\n", highest_nonzero);
    printf("Expected for 2048-bit number: 64 non-zero words (0-63)\n");

    return 0;
}
