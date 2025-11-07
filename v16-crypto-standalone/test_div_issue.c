/*
 * Narrow down the division bug
 */
#include <stdio.h>
#include <stdint.h>
#include "bn_tinybignum.h"

int main() {
    struct bn a, b, quotient, remainder;
    
    printf("Test tiny-bignum-c division with small numbers first\n\n");
    
    /* Test 1: 100 / 7 = 14 remainder 2 */
    bignum_init(&a);
    bignum_init(&b);
    bignum_from_int(&a, 100);
    bignum_from_int(&b, 7);
    
    bignum_divmod(&a, &b, &quotient, &remainder);
    
    int q = bignum_to_int(&quotient);
    int r = bignum_to_int(&remainder);
    
    printf("Test 1: 100 / 7\n");
    printf("  Quotient: %d (expected 14)\n", q);
    printf("  Remainder: %d (expected 2)\n", r);
    
    if (q == 14 && r == 2) {
        printf("  ✓ PASS\n\n");
    } else {
        printf("  ✗ FAIL\n\n");
    }
    
    /* Test 2: 1000 / 7 = 142 remainder 6 */
    bignum_from_int(&a, 1000);
    bignum_divmod(&a, &b, &quotient, &remainder);
    
    q = bignum_to_int(&quotient);
    r = bignum_to_int(&remainder);
    
    printf("Test 2: 1000 / 7\n");
    printf("  Quotient: %d (expected 142)\n", q);
    printf("  Remainder: %d (expected 6)\n", r);
    
    if (q == 142 && r == 6) {
        printf("  ✓ PASS\n\n");
    } else {
        printf("  ✗ FAIL\n\n");
    }
    
    /* Test 3: Large number divided by large number */
    printf("Test 3: 0xFFFFFFFF / 0x10000\n");
    bignum_from_int(&a, 0xFFFFFFFF);
    bignum_from_int(&b, 0x10000);
    
    bignum_divmod(&a, &b, &quotient, &remainder);
    
    uint64_t q64 = ((uint64_t)quotient.array[1] << 32) | quotient.array[0];
    uint64_t r64 = ((uint64_t)remainder.array[1] << 32) | remainder.array[0];
    
    printf("  Quotient: 0x%llx (expected 0xffff)\n", (unsigned long long)q64);
    printf("  Remainder: 0x%llx (expected 0xffff)\n", (unsigned long long)r64);
    
    if (q64 == 0xffff && r64 == 0xffff) {
        printf("  ✓ PASS\n\n");
    } else {
        printf("  ✗ FAIL\n\n");
    }
    
    return 0;
}
