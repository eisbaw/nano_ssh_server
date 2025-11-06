#include <stdio.h>
#include <stdint.h>
#include "bignum_simple.h"

/* Debug version of bn_mod_simple */
static void bn_mod_simple_debug(bn_t *r, const bn_2x_t *a, const bn_t *m) {
    bn_2x_t temp = *a;
    int subtractions = 0;

    /* Find highest set bit in modulus m */
    int m_high_bit = -1;
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (m->array[i] != 0) {
            uint32_t word = m->array[i];
            for (int bit = 31; bit >= 0; bit--) {
                if (word & (1U << bit)) {
                    m_high_bit = i * 32 + bit;
                    goto found_m_high;
                }
            }
        }
    }
found_m_high:
    printf("m_high_bit = %d\n", m_high_bit);

    /* Find highest set bit in temp */
    int temp_high_bit = -1;
    for (int i = BN_2X_WORDS - 1; i >= 0; i--) {
        if (temp.array[i] != 0) {
            uint32_t word = temp.array[i];
            for (int bit = 31; bit >= 0; bit--) {
                if (word & (1U << bit)) {
                    temp_high_bit = i * 32 + bit;
                    goto found_temp_high;
                }
            }
        }
    }
found_temp_high:
    printf("temp_high_bit (initial) = %d\n", temp_high_bit);

    /* Process from high bit downward */
    for (int bit_pos = BN_2X_WORDS * 32 - 1; bit_pos > m_high_bit; bit_pos--) {
        int word_idx = bit_pos / 32;
        int bit_idx = bit_pos % 32;

        /* Check if this bit is set in temp */
        if (!(temp.array[word_idx] & (1U << bit_idx))) {
            continue;  /* Skip if bit is 0 */
        }

        subtractions++;
        if (subtractions <= 5) {
            printf("Subtraction %d: bit_pos=%d\n", subtractions, bit_pos);
        }

        /* Subtract m shifted left by (bit_pos - m_high_bit) bits */
        int shift = bit_pos - m_high_bit;
        int shift_words = shift / 32;
        int shift_bits = shift % 32;

        /* Perform subtraction */
        if (shift_bits == 0) {
            uint32_t borrow = 0;
            for (int i = 0; i < BN_WORDS; i++) {
                int dest_idx = i + shift_words;
                if (dest_idx >= BN_2X_WORDS) break;

                uint64_t diff = (uint64_t)temp.array[dest_idx] - (uint64_t)m->array[i] - (uint64_t)borrow;
                temp.array[dest_idx] = (uint32_t)(diff & 0xFFFFFFFF);
                borrow = (diff >> 32) & 1;
            }
        } else {
            uint32_t borrow = 0;
            for (int i = 0; i < BN_WORDS + 1; i++) {
                int dest_idx = i + shift_words;
                if (dest_idx >= BN_2X_WORDS) break;

                uint64_t m_shifted = 0;
                if (i < BN_WORDS) {
                    m_shifted = (uint64_t)m->array[i] << shift_bits;
                }
                if (i > 0 && i - 1 < BN_WORDS) {
                    m_shifted |= (uint64_t)m->array[i - 1] >> (32 - shift_bits);
                }

                uint64_t diff = (uint64_t)temp.array[dest_idx] - m_shifted - (uint64_t)borrow;
                temp.array[dest_idx] = (uint32_t)(diff & 0xFFFFFFFF);
                borrow = (diff >> 32) & 1;
            }
        }
    }

    printf("Total subtractions: %d\n\n", subtractions);

    /* Copy low words to result */
    for (int i = 0; i < BN_WORDS; i++) {
        r->array[i] = temp.array[i];
    }

    /* Final reduction */
    int final_subs = 0;
    while (bn_cmp(r, m) >= 0) {
        bn_sub(r, r, m);
        final_subs++;
    }
    printf("Final reduction subtractions: %d\n", final_subs);
}

#include "rsa.h"

int main() {
    rsa_key_t key;
    bn_t base, temp;
    bn_2x_t product;

    rsa_init_key(&key);

    /* Get to iteration 8 */
    bn_zero(&base);
    base.array[0] = 42;
    for (int i = 0; i < 8; i++) {
        bn_mulmod(&temp, &base, &base, &key.n);
        base = temp;
    }

    /* Square */
    bn_mul_wide(&product, &base, &base);

    printf("Testing bn_mod_simple with debug output:\n\n");
    bn_mod_simple_debug(&temp, &product, &key.n);

    printf("\nResult: temp.array[0] = 0x%08x\n", temp.array[0]);
    printf("Expected: 0x0ee1c7e7\n");

    return 0;
}
