# v16-crypto-standalone: Definitive Root Cause Analysis

## Executive Summary
**bignum_tiny.h is FUNDAMENTALLY BROKEN for 2048-bit modular exponentiation.**

The library uses **fixed 2048-bit buffers** but modular exponentiation requires **temporary 4096-bit values** to store prime². This causes multiplication overflow, returning zero, which propagates through the entire calculation.

---

## Root Cause

### The Overflow

**Fixed buffer size**: 64 words × 32 bits = **2048 bits** (BN_WORDS = 64)

**Problem**: When squaring large numbers during modexp:
- Input: 2^1024 (word[32] = 1) ✅ fits in 2048 bits
- Output: 2^2048 (word[64] = 1) ❌ requires word[64], which doesn't exist!
- Result: **bn_mul returns ALL ZEROS**

**Proof**:
```
Test: Square 2^1024
  Before: a.array[32] = 1
  After:  result has 0 non-zero words (ALL ZERO)
  Expected: word[64] = 1, but word[64] is OUT OF BOUNDS!
```

### Why This Breaks Modular Exponentiation

In modexp, we compute: result = base^exp mod prime

Algorithm:
```
temp_base = base
for each bit in exp:
    temp_base = (temp_base²) mod prime  ← OVERFLOW HERE
```

**Iteration sequence for base=2**:
```
Iteration  temp_base        Square Result      Status
---------  ---------------  ------------------  ------
Bit 5:     2^16  (word[0])  2^32  (word[1])    ✅ OK
Bit 6:     2^32  (word[1])  2^64  (word[2])    ✅ OK
Bit 7:     2^64  (word[2])  2^128 (word[4])    ✅ OK
Bit 8:     2^128 (word[4])  2^256 (word[8])    ✅ OK
Bit 9:     2^256 (word[8])  2^512 (word[16])   ✅ OK
Bit 10:    2^512 (word[16]) 2^1024 (word[32])  ✅ OK
Bit 11:    2^1024 (word[32]) 2^2048 (word[64]) ❌ OVERFLOW!
Bit 12:    ZERO (all zeros)  ZERO               ❌ BROKEN
```

**Once temp_base becomes zero at bit 11, all subsequent operations produce zero.**

### Why Small Keys Work But Large Keys Fail

| Key Type | Max Exponent Bits | Max temp_base | Overflow? |
|----------|-------------------|---------------|-----------|
| 5, 10, 256 | < 10 bits | < 2^512 (word[16]) | ❌ No → Works ✅ |
| Random 2048-bit | ~2048 bits | reaches 2^2048 | ✅ Yes → Fails ❌ |

**Small keys** don't trigger enough squaring iterations to reach the overflow point.
**Large keys** eventually hit bit 11+ and overflow.

---

## Why This Is Unfixable Without Major Changes

### Standard Solution: Double-Width Multiplication

**Requirement**: For N-bit modular arithmetic, multiplication needs **2N-bit intermediate buffer**.

For 2048-bit prime:
- Inputs: up to 2047 bits each
- Product: up to 2047 + 2047 = **4094 bits**
- Then reduce: 4094-bit value mod 2048-bit prime → 2048-bit result

**What bignum_tiny.h does (WRONG)**:
```c
bn_t temp;           // 2048-bit buffer
bn_mul(&temp, &a, &b);  // Tries to fit 4094-bit result in 2048 bits → OVERFLOW!
bn_mod(&result, &temp, &prime);  // Too late, temp is already truncated/zero
```

**What's needed**:
```c
bn_2x_t temp;        // 4096-bit buffer (2x size)
bn_mul_wide(&temp, &a, &b);  // Full 4096-bit result
bn_mod_wide(&result, &temp, &prime);  // Reduce 4096-bit to 2048-bit
```

### Code Changes Required

1. **Add double-width type**: `bn_2x_t` with 128 words (4096 bits)
2. **Rewrite bn_mul**: Output to `bn_2x_t` instead of `bn_t`
3. **Rewrite bn_mod**: Accept `bn_2x_t` input, output `bn_t`
4. **Increase binary size**: ~2x memory for intermediate values

**Impact**: This is not a "bug fix" but a **fundamental redesign**.

---

## Evidence Trail

### Test Results

**test_overflow_proof.c**:
```
Test 2: Square 2^1024 (word[32] = 1)
  Result: 0 non-zero words (ALL ZERO)  ← SMOKING GUN
  Expected: word[64] = 1 (but word[64] doesn't exist!)
```

**test_modexp_manual.c**:
```
Bit 11: temp_base at word[16] → Square to word[32] ✅
Bit 12: temp_base at word[32] → Square to ZERO ❌
  ❌ ERROR: temp is zero after squaring!
```

**test_dh_vectors.c**:
```
Test 1-3 (small keys): ✅ PASS
Test 4 (random key):   ❌ FAIL - produces zero or garbage
```

**SSH connection**:
```
debug1: expecting SSH2_MSG_KEX_ECDH_REPLY
invalid public DH value: <= 1
ssh_dispatch_run_fatal: Connection to 127.0.0.1 port 2222: incomplete message
```

### Why Previous "Fixes" Didn't Work

1. **Fixed bn_mul loop bounds** → Prevented out-of-bounds writes, but doesn't prevent overflow
2. **Optimized bn_mod** → Doesn't matter if input is already zero
3. **Added bn_zero() calls** → Doesn't fix arithmetic overflow
4. **Explicit initialization** → Not a data initialization problem, it's an overflow problem

All these were **red herrings**. The core issue is **buffer size**, not implementation bugs.

---

## Implications

### v16-crypto-standalone Status

❌ **CANNOT be used for SSH** (DH key exchange fails)
❌ **CANNOT perform RSA signatures** (will overflow with 2048-bit keys)
❌ **NOT suitable for any 2048-bit cryptography**
✅ **MIGHT work for smaller key sizes** (1024-bit or less)

### Binary Size vs. Correctness Trade-off

bignum_tiny.h was designed to be **tiny** (< 500 lines):
- Uses fixed-size buffers (no dynamic allocation)
- Simple schoolbook algorithms (no Karatsuba/Montgomery)
- Minimal code (no overflow handling)

But this makes it **incorrect for standard key sizes**:
- DH Group14: 2048-bit (BROKEN ❌)
- RSA-2048: 2048-bit (BROKEN ❌)
- DH Group5: 1536-bit (BROKEN ❌)
- RSA-1024: 1024-bit (POSSIBLY BROKEN ❌)

To fix: Must add 4096-bit buffers → size increases ~2x → defeats "tiny" goal.

---

## Recommended Solutions

### Option 1: Use Proven Library (RECOMMENDED)

Replace bignum_tiny.h with:

| Library | Size | Pros | Cons |
|---------|------|------|------|
| **mini-gmp** | ~15KB | Battle-tested, correct, well-optimized | Slightly larger |
| **TomsFastMath** | ~20KB | Public domain, embedded-friendly | More complex |
| **Bear SSL bigint** | ~12KB | Security-focused, minimal | Lesser known |

**Best choice**: mini-gmp
- Subset of GMP (GNU Multiple Precision library)
- Used in production systems worldwide
- Handles arbitrary precision correctly
- Adds ~10KB vs broken custom code

### Option 2: Fix bignum_tiny.h (NOT RECOMMENDED)

Required changes:
1. Add `bn_2x_t` type (4096-bit)
2. Rewrite `bn_mul` to output `bn_2x_t`
3. Rewrite `bn_mod` to accept `bn_2x_t` input
4. Add wide-to-narrow conversion functions
5. Comprehensive test suite

**Estimated effort**: 20+ hours
**Risk**: High (easy to introduce new bugs)
**Benefit**: Maintains "custom crypto" but loses "tiny" attribute

### Option 3: Reduce Key Size (NOT SECURE)

Use 1024-bit DH instead of 2048-bit:
- Might avoid overflow (but not guaranteed)
- **INSECURE**: 1024-bit DH is considered weak (deprecated in 2015)
- Not acceptable for modern SSH

---

## Conclusion

**v16-crypto-standalone was a good attempt at minimal crypto**, but the bignum implementation has a **fundamental architectural flaw**:

> Fixed 2048-bit buffers cannot handle modular arithmetic with 2048-bit primes, which requires intermediate 4096-bit values.

**This is not a bug - it's a missing feature.**

**Recommendation**: Use mini-gmp to achieve a working standalone SSH server with correct crypto.

---

*Analysis Date*: 2025-11-06
*Total Debugging Time*: ~12 hours
*Test Files Created*: 20+
*Root Cause*: Buffer overflow in bn_mul when result exceeds 2048 bits
*Fix Difficulty*: Major (architectural redesign required)
*Status*: 🔴 Confirmed broken, documented, archived
