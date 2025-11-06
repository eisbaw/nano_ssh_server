# v16-crypto-standalone Bignum Library - Critical Bugs Report

## Executive Summary
**STATUS**: 🔴 **BROKEN** - Cannot be used for SSH/DH key exchange

The custom `bignum_tiny.h` library has multiple critical bugs that cause **modular exponentiation to fail** for realistic key sizes. This makes v16-crypto-standalone **unusable for SSH**.

## Symptoms
- Simple DH test keys (5, 10, 256) work ✅
- Random/large DH keys fail ❌
- SSH client rejects public keys: "invalid public DH value: <= 1"
- All large modexp operations return 2^256 regardless of actual exponent

## Root Causes Identified

### Bug #1: bn_modexp produces constant result for large exponents
**File**: `bignum_tiny.h`, function `bn_modexp()`
**Impact**: CRITICAL

**Evidence**:
```
2^256 mod prime     → 2^256  (correct by accident)
2^65536 mod prime   → 2^256  (WRONG!)
2^(2^24) mod prime  → 2^256  (WRONG!)
```

All large exponents produce the same result: `word[8] = 1` = 2^256

**Cause**: The squaring loop in bn_modexp encounters a condition where `temp_base` becomes zero or gets stuck at an incorrect value after ~10-15 iterations, causing all subsequent calculations to be wrong.

### Bug #2: Interaction between bn_mul and bn_mod in modexp loop
**Details**:
- Individual tests show: `bn_mul` works ✅, `bn_mod` works ✅
- But when used together in the modexp squaring loop, `temp_base` eventually becomes zero
- Trace shows at iteration j=10, temp_base → 0, then stays 0
- Once zero, result = result × 0 = 0, making final public key invalid

### Bug #3: Uninitialized memory with certain compiler settings
**Evidence**:
- Without AddressSanitizer: result contains stack addresses (0x7eb5, 0x7ed3, etc.)
- With AddressSanitizer: result is completely zero
- This suggests uninitialized memory is being read and propagated

## Attempted Fixes
1. ✅ Fixed `bn_mul` loop bounds (was: `j < BN_WORDS - i`, now: `j < BN_WORDS && i+j < BN_WORDS`)
2. ✅ Implemented efficient `bn_shl` for multi-bit shifts
3. ✅ Added explicit `bn_zero()` calls in `bn_modexp`
4. ✅ Implemented hybrid `bn_mod` with repeated subtraction + binary division fallback
5. ❌ **None of these fixed the core issue**

## Test Results Summary

| Test Case | Expected | Actual | Status |
|-----------|----------|--------|--------|
| 2^5 mod prime | 32 | 32 | ✅ PASS |
| 2^10 mod prime | 1024 | 1024 | ✅ PASS |
| 2^256 mod prime | 2^256 | 2^256 | ✅ PASS (small exp) |
| 2^65536 mod prime | large number | 2^256 | ❌ FAIL |
| 2^(2^24) mod prime | large number | 2^256 | ❌ FAIL |
| Random 2048-bit key | valid public key | zero or garbage | ❌ FAIL |

## Why Small Keys Work But Large Keys Fail
- Small exponents (5, 10, 256) require < 10 squaring iterations
- Large exponents require many more iterations
- Bug manifests after ~10-15 iterations when temp_base becomes zero
- Once temp_base = 0, all subsequent operations produce 0 or wrong values

## Recommendation

### Option 1: Use Proven Bignum Library (RECOMMENDED)
Replace `bignum_tiny.h` with a battle-tested library:
- **mini-gmp**: Minimal GMP subset, ~10KB, well-tested
- **TomsFastMath**: Public domain, optimized for embedded systems
- **Bear SSL bigint**: Thomas Pornin's implementation, security-focused

**Pros**: Immediate fix, proven correctness, good size/performance
**Cons**: Adds external dependency (but that's acceptable vs broken code)

### Option 2: Fix bignum_tiny.h
Deep debugging required to find the exact interaction bug between bn_mul/bn_mod/bn_modexp.

**Pros**: Maintains "100% standalone" goal
**Cons**:
- Unknown time to fix (already spent 8+ hours debugging)
- High risk of introducing new bugs
- No test suite to verify correctness
- Single developer code, not peer-reviewed

## Impact on v16-crypto-standalone
- ❌ **Cannot perform DH key exchange**
- ❌ **SSH connections fail immediately**
- ❌ **Not suitable for any production use**
- ❌ **Not suitable as "smallest working SSH server"**

## Files with Evidence
- `test_dh_vectors.c` - Shows simple keys pass, random keys fail
- `test_modexp_trace.c` - Shows temp_base becomes zero at j=10
- `test_real_modexp.c` - Shows all large exponents return 2^256
- `V16_CRITICAL_BUG_FOUND.md` - Initial bug discovery notes

## Conclusion
The custom bignum implementation in v16-crypto-standalone is **fundamentally broken** for the primary use case (DH key exchange).

**Recommended action**: Replace `bignum_tiny.h` with mini-gmp or similar proven library to achieve a working standalone SSH server.

---
*Report Date*: 2025-11-06
*Time Spent Debugging*: ~8 hours
*Status*: Bugs identified but not fixed
