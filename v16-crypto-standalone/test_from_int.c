#include <stdio.h>
#include <stdint.h>
#include "bn_tinybignum.h"

int main() {
    struct bn n;
    
    printf("Test bignum_from_int with 64-bit value\n\n");
    
    uint64_t val = 0x0ade69eac1000000ULL;
    printf("Input value: 0x%016llx\n", (unsigned long long)val);
    printf("Expected:\n");
    printf("  array[0] = 0xc1000000\n");
    printf("  array[1] = 0x0ade69ea\n\n");
    
    bignum_from_int(&n, val);
    
    printf("Actual:\n");
    printf("  array[0] = 0x%08x\n", n.array[0]);
    printf("  array[1] = 0x%08x\n", n.array[1]);
    
    if (n.array[0] == 0xc1000000 && n.array[1] == 0x0ade69ea) {
        printf("\n✓ CORRECT\n");
        return 0;
    } else {
        printf("\n✗ WRONG\n");
        return 1;
    }
}
