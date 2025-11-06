#include <stdio.h>
#include <stdint.h>
#include "bignum_simple.h"

int main() {
    /* Test: subtract (5 << 3) from 50 */
    /* Expected: 50 - 40 = 10 */

    bn_2x_t temp;
    bn_t m, result;

    bn_2x_zero(&temp);
    temp.array[0] = 50;

    bn_zero(&m);
    m.array[0] = 5;

    /* Manually perform shift-and-subtract like bn_mod_simple does */
    /* We want to subtract m << 3 (which is 5 << 3 = 40) */

    int shift_bits = 3;
    int shift_words = 0;

    uint32_t borrow = 0;
    for (int i = 0; i < BN_WORDS + 1; i++) {
        int dest_idx = i + shift_words;
        if (dest_idx >= BN_2X_WORDS) break;

        uint64_t m_shifted = 0;
        if (i < BN_WORDS) {
            m_shifted = (uint64_t)m.array[i] << shift_bits;
        }
        if (i + 1 < BN_WORDS) {
            m_shifted |= (uint64_t)m.array[i + 1] >> (32 - shift_bits);
        }

        uint64_t diff = (uint64_t)temp.array[dest_idx] - m_shifted - (uint64_t)borrow;
        temp.array[dest_idx] = (uint32_t)(diff & 0xFFFFFFFF);
        borrow = (diff >> 32) & 1;
    }

    printf("Test: 50 - (5 << 3) = 50 - 40\n");
    printf("Expected: 10\n");
    printf("Got: %u\n", temp.array[0]);

    if (temp.array[0] == 10) {
        printf("✓ CORRECT\n");
    } else {
        printf("✗ WRONG\n");
    }

    return 0;
}
