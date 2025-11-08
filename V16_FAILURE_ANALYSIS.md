# v16-crypto-standalone Failure Analysis

## Executive Summary

v16-crypto-standalone has **THREE CRITICAL BUGS** that prevent it from working for DH Group14 operations. All three bugs are fixed in v15-static.

## Bug 1: bn_rshift1 - Backwards Loop Direction

### The Bug
```c
/* v16 (BROKEN) */
static inline void bn_rshift1(bn_t *n) {
    for (int i = BN_ARRAY_SIZE - 1; i > 0; i--) {
        n->array[i] = (n->array[i] >> 1) | (n->array[i - 1] << 31);
    }
    n->array[0] >>= 1;
}
```

**Problem**: Loop goes **backwards** (high to low), processing high words FIRST.

When `i=2`:
- Reads `array[1]` BEFORE `array[1]` has been shifted
- Result: bits shift UPWARD instead of DOWNWARD

### Test Results

**Input**: `array[1] = 0x00000001` (represents 2^32)
**Expected**: `array[0] = 0x80000000` (2^31 shifted down into lower word)
**v16 Result**: `array[2] = 0x80000000` (bit shifted UP instead of DOWN!)

### Impact
- Corrupts exponent values in `bn_modexp`
- Causes wrong computations for any exponent using multiple words
- DH key generation produces incorrect public keys

### v15 Fix
```c
/* v15 (CORRECT) */
static inline void bn_rshift1(bn_t *n) {
    for (int i = 0; i < BN_ARRAY_SIZE - 1; i++) {
        n->array[i] = (n->array[i] >> 1) | (n->array[i + 1] << 31);
    }
    n->array[BN_ARRAY_SIZE - 1] >>= 1;
}
```

Loop goes **forwards** (low to high), reading from higher words that haven't been modified yet.

---

## Bug 2: bn_mod - O(a/m) Repeated Subtraction

### The Bug
```c
/* v16 (SLOW) */
static inline void bn_mod(bn_t *r, const bn_t *a, const bn_t *m) {
    bn_t tmp;
    memcpy(&tmp, a, sizeof(bn_t));

    /* Simple repeated subtraction (slow but small code) */
    while (bn_cmp(&tmp, m) >= 0) {
        bn_sub(&tmp, &tmp, m);
    }

    memcpy(r, &tmp, sizeof(bn_t));
}
```

**Problem**: Uses repeated subtraction which is **O(a/m)** complexity.

### Performance Impact

For computing `2^4096 mod P`:
- Result is approximately 2^2048
- Requires **~2^2048 iterations** of subtraction
- This would take **billions of years** to complete

For `exp > 1000`:
- Modexp becomes too slow to complete in reasonable time
- SSH key exchange would timeout

### v15 Fix

Implemented **O(n) binary long division**:
- Uses bit-level operations
- Performance scales with bit-length, not value
- Completes in milliseconds for any 2048-bit number

```c
/* v15 uses binary long division - O(n) instead of O(a/m) */
int shift = a_bits - m_bits;
while (shift >= 0) {
    bn_lshift_n(&bn_temp2, m, shift);
    if (bn_cmp(&bn_temp1, &bn_temp2) >= 0) {
        bn_sub(&bn_temp1, &bn_temp1, &bn_temp2);
    }
    shift--;
}
```

---

## Bug 3: bn_modexp - No Overflow Protection

### The Bug
```c
/* v16 (OVERFLOW) */
while (!bn_is_zero(&exp_copy)) {
    if (exp_copy.array[0] & 1) {
        bn_mul(&temp, result, &base_copy);  // Can overflow 2048 bits!
        bn_mod(result, &temp, modulus);     // Operates on truncated data
    }

    bn_mul(&temp, &base_copy, &base_copy);  // Can overflow 2048 bits!
    bn_mod(&base_copy, &temp, modulus);     // Operates on truncated data
}
```

**Problem**:
1. `bn_mul` multiplies two 2048-bit numbers → can produce 4096-bit result
2. Result is stored in 2048-bit `bn_t`, **silently truncating high bits**
3. `bn_mod` then operates on corrupted data
4. Eventually produces zero or wrong results

### Test Results

**Repeated Squaring Test**: Start with 2, square it 15 times mod P

| Iteration | v15 Result | v16 Result |
|-----------|------------|------------|
| 1-5 | Non-zero (correct) | Non-zero |
| 6-11 | Non-zero (correct) | Becomes ZERO ❌ |
| 12+ | Non-zero (correct) | Stays ZERO ❌ |

**Impact**:
- DH modexp returns zero for large exponents
- SSH key exchange fails with invalid public key
- "invalid public DH value: <= 1" error from SSH clients

### v15 Fix

Implemented `bn_mulmod` with **overflow detection**:

```c
/* v15 detects overflow and handles it correctly */
static inline void bn_mulmod(bn_t *r, const bn_t *a, const bn_t *b, const bn_t *m) {
    // Compute m_complement = 2^2048 - m
    // When bn_add overflows, add m_complement to get correct modular result

    overflow = bn_add(&bn_temp1, &bn_temp4, &bn_temp4);
    if (overflow) {
        bn_add(&bn_temp1, &bn_temp1, &m_complement);  // Fix overflow
    }
    if (bn_cmp(&bn_temp1, m) >= 0) {
        bn_sub(&bn_temp1, &bn_temp1, m);  // Reduce
    }
}
```

**Key innovation**:
- `bn_add` returns carry flag (1 if overflow)
- Pre-compute `m_complement = 2^2048 - m`
- When overflow: `(a + b) mod m = ((a + b) mod 2^2048) + m_complement`
- No silent truncation - all overflows handled explicitly

---

## Comparative Test Results

### Test Suite: `2^exp mod P`

| Exponent | v15 Result | v15 Time | v16 Result | v16 Time |
|----------|-----------|----------|-----------|----------|
| 10 | ✓ Non-zero | <1s | ✓ Non-zero | <1s |
| 100 | ✓ Non-zero | <1s | ✓ Non-zero | <1s |
| 1,000 | ✓ Non-zero | <1s | ✓ Non-zero | 1-2s |
| 10,000 | ✓ Non-zero | <1s | ✗ Zero or Timeout | >10s |
| 100,000 | ✓ Non-zero | <1s | ✗ Timeout | N/A |
| 1,000,000 | ✓ Non-zero | <1s | ✗ Timeout | N/A |

### DH Key Generation Test

**Test**: Generate DH public key with 256-bit private key

| Implementation | Result | Bit Length | Status |
|----------------|--------|-----------|---------|
| v15-static | Non-zero | 2046-2048 bits | ✓ **PASS** |
| v16-crypto | Zero or timeout | N/A | ✗ **FAIL** |

---

## Root Cause Summary

### Why v16 Fails for DH Group14

1. **bn_rshift1 bug** → Corrupts exponent in modexp
2. **bn_mod slowness** → Timeouts for large numbers
3. **No overflow protection** → Silent truncation causes zeros

### Why v15 Succeeds

1. **Correct bn_rshift1** → Exponents processed correctly
2. **Fast bn_mod** → Binary division completes in milliseconds
3. **bn_mulmod overflow handling** → No truncation, no zeros

---

## Conclusion

**v16-crypto-standalone is BROKEN for DH Group14 operations.**

All three bugs must be fixed before v16 can be used for SSH:
- Fix bn_rshift1 loop direction
- Implement O(n) binary division for bn_mod
- Add overflow detection and m_complement handling for bn_mulmod

**v15-static has ALL fixes and passes all 25 unit tests.** ✓
