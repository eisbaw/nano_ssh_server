#include <stdio.h>
#include <stdint.h>
#include "bignum_simple.h"
#include "rsa.h"

void print_bn_low32(const char *name, const bn_t *x) {
    printf("%s low 32 bits: 0x%08x (%u)\n", name, x->array[0], x->array[0]);
}

void print_bn_full(const char *name, const bn_t *x) {
    uint8_t bytes[256];
    bn_to_bytes(x, bytes, 256);
    printf("%s (first 16 bytes): ", name);
    for (int i = 0; i < 16; i++) {
        printf("%02x", bytes[i]);
    }
    printf("\n");
}

/* Traced version of bn_modexp */
void bn_modexp_traced(bn_t *r, const bn_t *base, const bn_t *exp, const bn_t *mod, int max_iterations) {
    bn_t result, base_copy, temp;

    printf("=== C Trace: base^exp mod n ===\n");
    printf("base.array[0] = %u\n", base->array[0]);
    printf("exp.array[0] = 0x%08x (%u)\n", exp->array[0], exp->array[0]);
    printf("mod.array[63] = 0x%08x (highest word)\n\n", mod->array[63]);

    /* result = 1 */
    bn_zero(&result);
    result.array[0] = 1;

    /* base_copy = base mod m */
    base_copy = *base;
    if (bn_cmp(&base_copy, mod) >= 0) {
        bn_2x_t wide;
        bn_2x_zero(&wide);
        for (int i = 0; i < BN_WORDS; i++) {
            wide.array[i] = base_copy.array[i];
        }
        bn_mod_simple(&base_copy, &wide, mod);
    }

    printf("Initial: result.array[0] = 1, base_copy.array[0] = %u\n\n", base_copy.array[0]);

    /* Find the highest non-zero word in exponent */
    int max_word = -1;
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (exp->array[i] != 0) {
            max_word = i;
            break;
        }
    }

    if (max_word < 0) {
        *r = result;
        return;
    }

    /* Binary exponentiation: process bits from LSB to MSB */
    int bit_count = 0;
    for (int i = 0; i <= max_word; i++) {
        uint32_t exp_word = exp->array[i];

        for (int j = 0; j < 32; j++) {
            if (max_iterations >= 0 && bit_count >= max_iterations) {
                printf("... (stopping trace after %d iterations)\n\n", max_iterations);
                goto finish;
            }

            printf("--- Iteration %d (bit %d of exponent) ---\n", bit_count, bit_count);
            printf("Exponent bit = %d\n", (exp_word & 1) ? 1 : 0);

            /* If this bit of exponent is 1, multiply result by base */
            if (exp_word & 1) {
                printf("  Bit is 1: result = (result * base_copy) mod n\n");
                print_bn_low32("    Before: result", &result);
                print_bn_low32("    Before: base_copy", &base_copy);
                
                bn_mulmod(&temp, &result, &base_copy, mod);
                result = temp;
                
                print_bn_low32("    After: result", &result);
            } else {
                printf("  Bit is 0: result unchanged\n");
                print_bn_low32("    result", &result);
            }

            exp_word >>= 1;

            /* Early exit if no more bits in this word and we're at the last word */
            if (exp_word == 0 && i == max_word) {
                printf("\n");
                break;
            }

            /* Square the base for next bit */
            printf("  Square base: base_copy = base_copy^2 mod n\n");
            print_bn_low32("    Before: base_copy", &base_copy);
            
            bn_mulmod(&temp, &base_copy, &base_copy, mod);
            base_copy = temp;
            
            print_bn_low32("    After: base_copy", &base_copy);
            printf("\n");

            bit_count++;
        }
    }

finish:
    printf("=== Final Result ===\n");
    print_bn_low32("result", &result);
    print_bn_full("result", &result);
    printf("\n");

    *r = result;
}

int main() {
    rsa_key_t key;
    bn_t m, c;

    rsa_init_key(&key);

    /* Test 1: Small exponent */
    printf("TEST 1: Small exponent (42^3)\n");
    printf("============================================================\n");
    bn_zero(&m);
    m.array[0] = 42;
    
    bn_t exp_small;
    bn_zero(&exp_small);
    exp_small.array[0] = 3;
    
    bn_modexp_traced(&c, &m, &exp_small, &key.n, 10);

    printf("\n\nTEST 2: Large exponent (42^65537, first 10 iterations)\n");
    printf("============================================================\n");
    bn_zero(&m);
    m.array[0] = 42;
    
    bn_modexp_traced(&c, &m, &key.e, &key.n, 10);

    return 0;
}
