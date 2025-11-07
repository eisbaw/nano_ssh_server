/*
 * Check if overflow detection is triggering prematurely
 */
#include <stdio.h>
#include <stdint.h>
#include "bn_tinybignum.h"
#include "bignum_adapter.h"
#include "rsa.h"

int main() {
    rsa_key_t key;
    rsa_init_key(&key);
    
    printf("Checking overflow detection in bignum_div\n\n");
    
    const uint64_t half_max = 1 + (uint64_t)(MAX_VAL / 2);
    
    printf("MAX_VAL (from header) = 0x%llx\n", (unsigned long long)MAX_VAL);
    printf("half_max = 0x%llx (%llu)\n", (unsigned long long)half_max, (unsigned long long)half_max);
    printf("\n");
    
    printf("RSA modulus n:\n");
    printf("  BN_ARRAY_SIZE = %d\n", BN_ARRAY_SIZE);
    printf("  n.array[63] = 0x%08x (highest word)\n", key.n.array[BN_ARRAY_SIZE - 1]);
    printf("  n.array[62] = 0x%08x\n", key.n.array[BN_ARRAY_SIZE - 2]);
    printf("\n");
    
    if (key.n.array[BN_ARRAY_SIZE - 1] >= half_max) {
        printf("✗ OVERFLOW CHECK WOULD TRIGGER immediately!\n");
        printf("  This prevents division from working for 2048-bit numbers.\n");
        printf("  The modulus n naturally has high bits set.\n");
    } else {
        printf("✓ Overflow check would not trigger for n itself.\n");
    }
    
    /* Check after one shift */
    struct bn n_shifted;
    bignum_lshift((struct bn*)&key.n, &n_shifted, 1);
    
    printf("\nAfter one left shift (n << 1):\n");
    printf("  n_shifted.array[63] = 0x%08x\n", n_shifted.array[BN_ARRAY_SIZE - 1]);
    
    if (n_shifted.array[BN_ARRAY_SIZE - 1] >= half_max) {
        printf("✗ OVERFLOW CHECK TRIGGERS after just one shift!\n");
        printf("  This causes bignum_div to exit early.\n");
        printf("  Result: (2*n) / n returns 0 instead of 2.\n");
    } else {
        printf("✓ Still OK after one shift.\n");
    }
    
    return 0;
}
