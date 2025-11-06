# Custom Bignum Library Design
## Goal: Minimal size, correct implementation for DH/RSA

### Requirements
- 2048-bit arithmetic (64 × 32-bit words)
- Operations: add, sub, mul, mod, modexp, cmp, byte conversion
- Target size: 1-2 KB (vs 2-3 KB for external library)

### Size Optimization Strategies

1. **Simple algorithms over fast algorithms**
   - Use schoolbook multiplication (O(n²)) instead of Karatsuba
   - Use binary modular exponentiation (simple but works)
   - No assembly optimizations

2. **Code reuse**
   - Implement mul_add_word() helper, reuse in multiplication
   - Implement shift operations as building blocks
   - Share code between similar operations

3. **Minimal API surface**
   - Only implement what's strictly needed
   - No fancy features (negative numbers, arbitrary precision)
   - Fixed 2048-bit size

4. **Inlining strategy**
   - Inline only trivial helpers (set_zero, is_zero)
   - Keep complex operations as regular functions

### API Design

```c
#define BN_WORDS 64  /* 64 × 32 = 2048 bits */

typedef struct {
    uint32_t w[BN_WORDS];
} bn_t;

/* Initialization */
void bn_zero(bn_t *a);
void bn_from_bytes(bn_t *a, const uint8_t *bytes, size_t len);
void bn_to_bytes(const bn_t *a, uint8_t *bytes, size_t len);

/* Comparison */
int bn_cmp(const bn_t *a, const bn_t *b);  /* returns -1, 0, 1 */
int bn_is_zero(const bn_t *a);

/* Arithmetic */
void bn_add(bn_t *r, const bn_t *a, const bn_t *b);  /* r = a + b */
void bn_sub(bn_t *r, const bn_t *a, const bn_t *b);  /* r = a - b */
void bn_mul(bn_t *r, const bn_t *a, const bn_t *b);  /* r = a * b */

/* Modular arithmetic */
void bn_mod(bn_t *r, const bn_t *a, const bn_t *m);  /* r = a % m */
void bn_modexp(bn_t *r, const bn_t *base, const bn_t *exp, const bn_t *mod);

/* Bitwise */
void bn_shl(bn_t *a, int bits);  /* Shift left */
void bn_shr(bn_t *a, int bits);  /* Shift right */
```

### Implementation Notes

#### Addition (bn_add)
- Simple ripple-carry addition
- ~30-40 bytes

#### Subtraction (bn_sub)
- Simple borrow propagation
- ~30-40 bytes

#### Multiplication (bn_mul)
- Schoolbook algorithm
- Helper: mul_add_word() for single-word multiply-add
- ~80-100 bytes

#### Modular Reduction (bn_mod)
- Simple repeated subtraction with optimization
- Not constant-time (OK for our use case)
- ~60-80 bytes

#### Modular Exponentiation (bn_modexp)
- Binary square-and-multiply (right-to-left)
- Most complex operation
- ~100-120 bytes

#### Total estimated size: 500-800 bytes

### Testing Strategy
- Port existing bignum tests
- Add edge cases (zero, one, max value)
- Verify against external library outputs
- Test with actual DH/RSA operations

### Implementation Plan
1. Write header with API
2. Implement basic operations (zero, cmp, add, sub)
3. Test basic operations
4. Implement multiplication
5. Test multiplication
6. Implement modular arithmetic (mod, modexp)
7. Test with DH/RSA test cases
8. Measure code size
9. Optimize if needed
