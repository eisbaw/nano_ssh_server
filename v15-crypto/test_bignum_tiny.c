/* Test suite for bignum_tiny.h - custom minimal bignum library */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "bignum_tiny.h"

int test_count = 0;
int pass_count = 0;

#define TEST(name) do { \
    printf("Testing %s... ", name); \
    test_count++; \
} while(0)

#define PASS() do { \
    printf("PASS\n"); \
    pass_count++; \
} while(0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); \
} while(0)

void print_bn(const bn_t *a) {
    uint8_t bytes[BN_BYTES];
    bn_to_bytes(a, bytes, BN_BYTES);
    for (int i = 0; i < BN_BYTES; i++) {
        printf("%02x", bytes[i]);
    }
}

int test_zero_and_is_zero() {
    TEST("bn_zero and bn_is_zero");
    
    bn_t a;
    bn_zero(&a);
    
    if (!bn_is_zero(&a)) {
        FAIL("zero not detected");
        return 0;
    }
    
    a.w[0] = 1;
    if (bn_is_zero(&a)) {
        FAIL("non-zero detected as zero");
        return 0;
    }
    
    PASS();
    return 1;
}

int test_from_to_bytes() {
    TEST("bn_from_bytes and bn_to_bytes");
    
    uint8_t input[32] = {
        0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11,
        0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99
    };
    uint8_t output[32];
    
    bn_t a;
    bn_from_bytes(&a, input, 32);
    bn_to_bytes(&a, output, 32);
    
    if (memcmp(input, output, 32) != 0) {
        FAIL("round-trip conversion failed");
        return 0;
    }
    
    PASS();
    return 1;
}

int test_cmp() {
    TEST("bn_cmp");
    
    bn_t a, b;
    
    /* Test equal */
    bn_zero(&a);
    bn_zero(&b);
    if (bn_cmp(&a, &b) != 0) {
        FAIL("equal comparison failed");
        return 0;
    }
    
    /* Test a > b */
    a.w[0] = 2;
    b.w[0] = 1;
    if (bn_cmp(&a, &b) != 1) {
        FAIL("greater than comparison failed");
        return 0;
    }
    
    /* Test a < b */
    if (bn_cmp(&b, &a) != -1) {
        FAIL("less than comparison failed");
        return 0;
    }
    
    /* Test high word comparison */
    bn_zero(&a);
    bn_zero(&b);
    a.w[BN_WORDS-1] = 1;
    if (bn_cmp(&a, &b) != 1) {
        FAIL("high word comparison failed");
        return 0;
    }
    
    PASS();
    return 1;
}

int test_add() {
    TEST("bn_add");
    
    bn_t a, b, r, expected;
    
    /* Simple addition: 5 + 3 = 8 */
    bn_zero(&a);
    bn_zero(&b);
    a.w[0] = 5;
    b.w[0] = 3;
    bn_add(&r, &a, &b);
    
    if (r.w[0] != 8) {
        FAIL("simple addition failed");
        return 0;
    }
    
    /* Test carry: 0xFFFFFFFF + 1 = 0x100000000 */
    bn_zero(&a);
    bn_zero(&b);
    a.w[0] = 0xFFFFFFFF;
    b.w[0] = 1;
    bn_add(&r, &a, &b);
    
    if (r.w[0] != 0 || r.w[1] != 1) {
        FAIL("carry failed");
        return 0;
    }
    
    PASS();
    return 1;
}

int test_sub() {
    TEST("bn_sub");
    
    bn_t a, b, r;
    
    /* Simple subtraction: 8 - 3 = 5 */
    bn_zero(&a);
    bn_zero(&b);
    a.w[0] = 8;
    b.w[0] = 3;
    bn_sub(&r, &a, &b);
    
    if (r.w[0] != 5) {
        FAIL("simple subtraction failed");
        return 0;
    }
    
    /* Test borrow: 0x100000000 - 1 = 0xFFFFFFFF */
    bn_zero(&a);
    bn_zero(&b);
    a.w[1] = 1;
    b.w[0] = 1;
    bn_sub(&r, &a, &b);
    
    if (r.w[0] != 0xFFFFFFFF || r.w[1] != 0) {
        FAIL("borrow failed");
        return 0;
    }
    
    PASS();
    return 1;
}

int test_mul() {
    TEST("bn_mul");
    
    bn_t a, b, r;
    
    /* Simple multiplication: 7 * 6 = 42 */
    bn_zero(&a);
    bn_zero(&b);
    a.w[0] = 7;
    b.w[0] = 6;
    bn_mul(&r, &a, &b);
    
    if (r.w[0] != 42) {
        FAIL("simple multiplication failed");
        printf("  Expected: 42, Got: %u\n", r.w[0]);
        return 0;
    }
    
    /* Test overflow: 0x10000 * 0x10000 = 0x100000000 */
    bn_zero(&a);
    bn_zero(&b);
    a.w[0] = 0x10000;
    b.w[0] = 0x10000;
    bn_mul(&r, &a, &b);
    
    if (r.w[0] != 0 || r.w[1] != 1) {
        FAIL("overflow multiplication failed");
        printf("  Expected: w[0]=0 w[1]=1, Got: w[0]=%u w[1]=%u\n", r.w[0], r.w[1]);
        return 0;
    }
    
    /* Test larger values */
    bn_zero(&a);
    bn_zero(&b);
    a.w[0] = 0xFFFFFFFF;
    b.w[0] = 2;
    bn_mul(&r, &a, &b);
    
    if (r.w[0] != 0xFFFFFFFE || r.w[1] != 1) {
        FAIL("large value multiplication failed");
        return 0;
    }
    
    PASS();
    return 1;
}

int test_mod() {
    TEST("bn_mod");
    
    bn_t a, m, r;
    
    /* Simple modulo: 10 % 3 = 1 */
    bn_zero(&a);
    bn_zero(&m);
    a.w[0] = 10;
    m.w[0] = 3;
    bn_mod(&r, &a, &m);
    
    if (r.w[0] != 1) {
        FAIL("simple modulo failed");
        printf("  Expected: 1, Got: %u\n", r.w[0]);
        return 0;
    }
    
    /* Test a < m (should return a) */
    bn_zero(&a);
    bn_zero(&m);
    a.w[0] = 5;
    m.w[0] = 10;
    bn_mod(&r, &a, &m);
    
    if (r.w[0] != 5) {
        FAIL("a < m case failed");
        return 0;
    }
    
    PASS();
    return 1;
}

int test_modexp() {
    TEST("bn_modexp");
    
    bn_t base, exp, mod, r;
    
    /* Simple modexp: 2^3 mod 5 = 8 mod 5 = 3 */
    bn_zero(&base);
    bn_zero(&exp);
    bn_zero(&mod);
    base.w[0] = 2;
    exp.w[0] = 3;
    mod.w[0] = 5;
    bn_modexp(&r, &base, &exp, &mod);
    
    if (r.w[0] != 3) {
        FAIL("simple modexp failed");
        printf("  Expected: 3, Got: %u\n", r.w[0]);
        return 0;
    }
    
    /* Test 3^5 mod 7 = 243 mod 7 = 5 */
    bn_zero(&base);
    bn_zero(&exp);
    bn_zero(&mod);
    base.w[0] = 3;
    exp.w[0] = 5;
    mod.w[0] = 7;
    bn_modexp(&r, &base, &exp, &mod);
    
    if (r.w[0] != 5) {
        FAIL("modexp 3^5 mod 7 failed");
        printf("  Expected: 5, Got: %u\n", r.w[0]);
        return 0;
    }
    
    /* Test larger exponent: 2^10 mod 1000 = 1024 mod 1000 = 24 */
    bn_zero(&base);
    bn_zero(&exp);
    bn_zero(&mod);
    base.w[0] = 2;
    exp.w[0] = 10;
    mod.w[0] = 1000;
    bn_modexp(&r, &base, &exp, &mod);
    
    if (r.w[0] != 24) {
        FAIL("modexp 2^10 mod 1000 failed");
        printf("  Expected: 24, Got: %u\n", r.w[0]);
        return 0;
    }
    
    PASS();
    return 1;
}

int test_shifts() {
    TEST("bn_shl1 and bn_shr1");
    
    bn_t a;
    
    /* Test left shift: 1 << 1 = 2 */
    bn_zero(&a);
    a.w[0] = 1;
    bn_shl1(&a);
    if (a.w[0] != 2) {
        FAIL("left shift 1 failed");
        return 0;
    }
    
    /* Test right shift: 4 >> 1 = 2 */
    bn_zero(&a);
    a.w[0] = 4;
    bn_shr1(&a);
    if (a.w[0] != 2) {
        FAIL("right shift 1 failed");
        return 0;
    }
    
    /* Test left shift with carry */
    bn_zero(&a);
    a.w[0] = 0x80000000;
    bn_shl1(&a);
    if (a.w[0] != 0 || a.w[1] != 1) {
        FAIL("left shift with carry failed");
        return 0;
    }
    
    /* Test right shift with carry */
    bn_zero(&a);
    a.w[1] = 1;
    bn_shr1(&a);
    if (a.w[0] != 0x80000000 || a.w[1] != 0) {
        FAIL("right shift with carry failed");
        return 0;
    }
    
    PASS();
    return 1;
}

int main(void) {
    printf("=== Testing bignum_tiny.h ===\n\n");
    
    test_zero_and_is_zero();
    test_from_to_bytes();
    test_cmp();
    test_add();
    test_sub();
    test_mul();
    test_mod();
    test_modexp();
    test_shifts();
    
    printf("\n=============================\n");
    printf("Results: %d/%d tests passed\n", pass_count, test_count);
    printf("=============================\n");
    
    return (pass_count == test_count) ? 0 : 1;
}
