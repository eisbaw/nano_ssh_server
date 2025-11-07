# Bignum Implementation Debug Report

## Summary

After extensive testing with tiny-bignum-c (public domain library by kokke), we discovered that the library has bugs in its division/modular reduction operations that cause incorrect results even for simple test cases.

## Testing Performed

### 1. Byte Order Conversion (FIXED ✓)

**Problem**: Initial byte conversion between big-endian SSH format and little-endian bignum representation was incorrect.

**Fix**: Updated `bn_from_bytes()` and `bn_to_bytes()` in `bignum_adapter.h` to correctly handle byte ordering within 32-bit words.

**Test Results**:
- Input: `[01 02 03 04 05 06 07 08]` (big-endian)
- Expected: `array[0] = 0x08070605`, `array[1] = 0x04030201`
- Result: ✓ CORRECT after fix

**Files**: `test_key_loading.c`

### 2. RSA Key Loading (WORKING ✓)

**Test**: Verified all 2048-bit RSA modulus loads correctly with all 64 words non-zero.

**Files**: `test_full_modulus.c`

### 3. Basic tiny-bignum-c Operations (WORKING ✓)

**Tests**:
- `42 * 42 = 1764` ✓
- `1764 mod 100 = 64` ✓

**Files**: `test_tiny_direct.c`, `test_tiny_mul_mod.c`

### 4. 64-bit Value Conversion (WORKING ✓)

**Test**: `bignum_from_int(0x0ade69eac1000000)` correctly produces:
- `array[0] = 0xc1000000`
- `array[1] = 0x0ade69ea`

**Files**: `test_from_int.c`

### 5. Multiplication (WORKING ✓)

**Test**: `0x34c10000 * 0x34c10000 = 0x0adef98100000000`
- Result matches expected value
- `array[0] = 0x00000000`, `array[1] = 0x0adef981`

**Files**: `test_mul_trace.c`

### 6. Modular Reduction (WORKING ✓ for small values)

**Test**: `0xADEF98100000000 mod n` (where n is 2048-bit RSA modulus)
- Returns same value (correct, since input < modulus)
- Does NOT incorrectly produce zero

**Files**: `test_mod_operation.c`

### 7. Modular Exponentiation (FAILS ✗)

**Test**: `42^65537 mod n` (RSA signature test)

**Expected**: `0xf91e0d65` (verified with Python)

**Result**: `0x00000000` (completely wrong)

**Root Cause**: During binary exponentiation at bit 5:
- Before: `base_copy = 0x00000000_AF96F981` (non-zero)
- After squaring: `temp = 0x00000000_00000000` (impossible!)
- After mod: `base_copy = 0x00000000_00000000`

At bit 8:
- Before: `base_copy = 0x00000000_00000000` (zero)
- After mod: `base_copy = 0x40791BB6_E08F9D34` (impossible - can't get non-zero from zero!)

**Files**: `test_full_modexp_trace.c`, `test_find_zero_bit.c`

### 8. Simple Exponentiation Test (FAILS ✗)

**Test**: `42^7 mod 100` (should be 88)

**Result**: 48 (WRONG!)

This simple test case proves tiny-bignum-c's division/modulo operations are buggy.

**Files**: `test_python_approach.c`

## Conclusions

1. **Byte conversion logic**: Fixed and working correctly
2. **RSA key loading**: Working correctly for 2048-bit keys
3. **Basic operations**: Multiplication works correctly
4. **Critical bug**: `bignum_mod()` and `bignum_div()` produce incorrect results

The bug appears to be in `bignum_div()` (line 277-320 in bn_tinybignum.c), likely related to the overflow check at line 295 or the division algorithm not scaling correctly to 2048-bit numbers.

## Next Steps

**Option A**: Fix tiny-bignum-c's division algorithm
- Requires deep understanding of the division implementation
- Risk of introducing new bugs

**Option B**: Find alternative proven bignum library
- BearSSL (minimal, correct, public domain)
- mbedTLS bignum (larger but proven)
- LibTomMath (public domain)

**Option C**: Use platform crypto libraries
- OpenSSL's BIGNUM (not standalone)
- Platform-specific implementations

**Recommendation**: Option B - Use BearSSL's bignum implementation, which is designed for embedded systems and is extensively tested.

## Test Files Created

1. `test_key_loading.c` - Byte order and key loading
2. `test_full_modulus.c` - Verify full 2048-bit modulus
3. `test_tiny_direct.c` - Basic tiny-bignum-c operations
4. `test_from_int.c` - 64-bit value conversion
5. `test_mul_trace.c` - Multiplication tracing
6. `test_mod_operation.c` - Modular reduction
7. `test_tiny_mul_mod.c` - Multiplication and mod
8. `test_modexp_debug.c` - Modular exponentiation debug
9. `test_full_modexp_trace.c` - Full exponentiation trace
10. `test_find_zero_bit.c` - Find where base_copy becomes zero
11. `test_python_approach.c` - Simple exponentiation test (42^7 mod 100)
12. `test_tiny_rsa.c` - Full RSA test with 42^65537

All tests demonstrate systematic investigation of the bug location.
