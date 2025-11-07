#include <stdio.h>
#include <stdint.h>
#include "bn_tinybignum.h"

static void print_first_words(const char *name, const struct bn *n) {
    printf("%s: [0]=0x%08x [1]=0x%08x [2]=0x%08x\n", name, n->array[0], n->array[1], n->array[2]);
}

int main() {
    struct bn a, b, result;
    
    printf("Tracing multiplication of 0x34c10000 * 0x34c10000\n\n");
    
    bignum_init(&a);
    bignum_init(&b);
    a.array[0] = 0x34c10000;
    b.array[0] = 0x34c10000;
    
    print_first_words("a", &a);
    print_first_words("b", &b);
    printf("\n");
    
    /* Manual multiplication trace */
    printf("Expected result:\n");
    printf("  0x34c10000 * 0x34c10000 = 0x0ade69ea_c1000000\n");
    printf("  array[0] = 0xc1000000\n");
    printf("  array[1] = 0x0ade69ea\n\n");
    
    bignum_mul(&a, &b, &result);
    
    printf("Actual result:\n");
    print_first_words("result", &result);
    
    printf("\nChecking intermediate calculation:\n");
    uint64_t intermediate = (uint64_t)0x34c10000 * (uint64_t)0x34c10000;
    printf("  intermediate (uint64) = 0x%016llx\n", (unsigned long long)intermediate);
    printf("  This should match 0x0ade69eac1000000\n");
    
    if (intermediate == 0x0ade69eac1000000ULL) {
        printf("  ✓ Intermediate calculation correct\n");
    } else {
        printf("  ✗ Intermediate calculation WRONG\n");
    }
    
    return 0;
}
