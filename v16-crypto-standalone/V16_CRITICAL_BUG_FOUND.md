# CRITICAL BUG FOUND IN v16-crypto-standalone

## Summary
**bn_modexp()** produces ZERO or garbage for large exponents because **temp_base becomes zero** during the squaring loop.

## Root Cause
During modular exponentiation, we repeatedly square temp_base:
```c
bn_mul(&temp, &temp_base, &temp_base);
bn_mod(&temp_base, &temp, mod);
```

At iteration j=5 when computing 2^(large exponent):
- Before: temp_base = 65536 (2^16)
- Square: 65536² = 4,294,967,296 (2^32)
- After bn_mul: temp should contain 2^32
- After bn_mod: temp_base becomes **ZERO** (WRONG!)

Expected: 2^32 mod prime = 2^32 (since 2^32 << prime)
Actual: ZERO

## Bug Location
The bug is in either:
1. **bn_mul()** - produces wrong result when squaring 65536
2. **bn_mod()** - incorrectly reduces 2^32 to zero

## Evidence
```
=== Test trace output ===
j=4: temp_base.array[0]=65536
j=5: temp_base.array[0]=0      ← ZERO!
  ❌ temp_base became ZERO at i=0, j=10!
```

Once temp_base becomes zero, all subsequent operations produce zero:
- result × 0 = 0
- 0² = 0

This explains why:
- **Simple keys (5, 10, 256) work**: Small exponents don't trigger enough squaring iterations
- **Random keys fail**: Large exponents cause temp_base to become zero early

## Impact
- v16-crypto-standalone **CANNOT** perform DH key exchange with realistic key sizes
- SSH connections fail with "invalid public DH value: <= 1"
- All modular exponentiation with exponents > ~16 bits fails

## Next Steps
1. Add debug to bn_mul to verify it produces correct result for 65536²
2. Add debug to bn_mod to verify it handles 2^32 correctly
3. Fix the broken function
4. Retest with random keys

## Status
🔴 CRITICAL - Bug identified, fix in progress
