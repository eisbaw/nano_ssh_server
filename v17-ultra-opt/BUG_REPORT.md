# v17-ultra-opt Bug Report

## Summary

v17-ultra-opt successfully achieves significant size reduction (11,852 bytes, 43% smaller than v16) through UPX compression, but contains a **critical bug** in the custom bignum library that prevents Diffie-Hellman key exchange from working correctly.

## Size Achievement

- v16-crypto-standalone: 20,824 bytes
- v17-ultra-opt (UPX compressed): 11,852 bytes
- **Reduction: 8,972 bytes (43.1%)**

## The Bug

**Location**: `bignum_tiny.h` - `bn_mod_double()` function

**Symptom**: Diffie-Hellman public key generation produces zero values, causing SSH handshake to fail with "invalid public DH value: <= 1"

**Root Cause**: The modular reduction of double-width (4096-bit) intermediate multiplication results is incorrectly implemented. After approximately 15 repeated modular squaring operations during exponentiation, intermediate values become zero instead of the correct reduced values.

**Test Evidence**:
```
After  0 squarings: 2
After  5 squarings: 2^32 (correct)
After 10 squarings: 2^1024 (appears correct but has issues)
After 15 squarings: 0 (WRONG - should be 2^(2^15) mod p)
```

## What Works

- Compilation and UPX compression: ✓
- Basic bignum operations (add, sub, cmp): ✓
- Multiplication of small numbers: ✓
- Modular operations on values < 2048 bits: ✓
- Modular exponentiation with small exponents (< 32 bits): ✓

## What Fails

- Modular exponentiation with large exponents (> 32 bits): ✗
- DH Group14 key generation: ✗
- SSH handshake: ✗
- **Cannot print "Hello World" over SSH**: ✗

## Attempted Fixes

Multiple approaches were attempted to fix `bn_mod_double()`:

1. **Shift-and-subtract with comparison**: Failed due to comparison logic errors
2. **Bit-by-bit long division**: Too slow (timeout)
3. **Word-level repeated subtraction**: Would require billions of iterations
4. **Decomposition approach**: Extremely complex and slow

## Recommendation

**Use v16-crypto-standalone (20,824 bytes) instead.**

While larger, v16 is:
- Fully functional
- Passes all tests
- Successfully completes SSH handshake
- Can print "Hello World" over SSH

## Future Work

To fix v17-ultra-opt, consider:

1. **Use external bignum library**: Link against a tested library like mini-gmp or TomsFastMath
   - Cost: Likely larger than 20KB
   - Benefit: Correctness guaranteed

2. **Implement Barrett reduction**: Proper algorithm for modular reduction
   - Cost: Complex implementation, more code
   - Benefit: Correct and reasonably fast

3. **Switch to smaller key sizes**: Use 1024-bit DH instead of 2048-bit
   - Cost: Security downgrade (not recommended)
   - Benefit: Double-width fits in single-width representation

4. **Use Montgomery multiplication**: Different approach to modexp
   - Cost: Complex implementation
   - Benefit: Faster and avoids double-width reduction

## Conclusion

v17-ultra-opt demonstrates that **11,852 bytes is achievable** through aggressive optimization and UPX compression, but the custom crypto implementation has a critical bug that prevents actual usage.

**Status**: ❌ Non-functional - DH key exchange broken
**Recommended**: Use v16-crypto-standalone instead
