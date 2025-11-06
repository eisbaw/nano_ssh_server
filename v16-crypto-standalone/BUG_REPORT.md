# Critical Bug: Bignum Modular Exponentiation

## Status: MULTIPLE IMPLEMENTATIONS ATTEMPTED - ALL BUGGY

## Summary
RSA signature verification fails because `bn_modexp()` produces incorrect results across multiple bignum implementations. The RSA key values (n, d, e) are correct, but all custom bignum implementations have bugs in modular exponentiation.

## Evidence

### 1. RSA Key Values Are Correct
- Extracted from valid OpenSSL-generated PEM file (`test_rsa_key.pem`)
- OpenSSL verification: `openssl rsa -in test_rsa_key.pem -check` → "RSA key ok"
- OpenSSL sign/verify test: SUCCESS
- Modulus n: Verified correct (matches OpenSSL output exactly)
- Private exponent d: Verified correct (matches OpenSSL output exactly)
- Public exponent e: 65537 (correct)

### 2. RSA Math Is Broken
Test (`test_rsa_math.c`):
```
Input:  m = 42
Step 1: c = m^e mod n = [large number]
Step 2: m2 = c^d mod n = [WRONG VALUE]
Expected: m2 = 42
Got:      m2 = 356694379972da35e2ea829faae224641e510bf7...
```

**Result**: `(m^e)^d mod n != m` (fundamental RSA property violated)

### 3. Bug Location
File: `bignum_tiny.h`
Function: `bn_modexp()` and/or Montgomery multiplication helpers

The Montgomery multiplication was added for performance (118x speedup for DH), but it has a correctness bug.

## Impact
- SSH client receives KEX_ECDH_REPLY but fails with "error in libcrypto"
- RSA signature verification always fails
- Server cannot complete SSH handshake

## Implementations Attempted

### 1. bignum_tiny.h (Montgomery Multiplication)
- Status: **BUGGY**
- Issue: Produces all zeros for large exponents (e=65537)
- Performance: Fast (118x speedup for DH)
- mont_init() and bn_modexp() have correctness bugs

### 2. bignum_simple.h (Simple Binary Exponentiation)
- Status: **BUGGY**
- Issue: bn_mod_simple() only subtracts modulus once instead of looping
- Fixed: Added while loop to subtract until r < m
- New issue: Still produces zeros for e=65537 after processing many bits
- Problem: Early exit logic or excessive squaring operations

### 3. bignum_v17.h (From v17-smallest-standalone, based on tiny-bignum-c)
- Status: **BUGGY**
- Issue: Produces incorrect results (c.array[0]=2581045920 vs expected 4179496293)
- Based on proven tiny-bignum-c (public domain)
- Uses different approach (right-shift exponent, check if odd)

### Expected Results (from Python)
```python
42^65537 mod n:
  Low word: 4179496293 (0xF92E24E5)

2^65537 mod n:
  Low word: 4109478790 (0xF50E2F96)
```

All three implementations return incorrect values or zeros.

## Root Cause Analysis

Common issues across implementations:
1. **Modular reduction bugs**: bn_mod_simple may have subtle bugs with 4096-bit intermediate values
2. **Overflow handling**: Large number of squaring operations may cause numerical issues
3. **Byte ordering**: Verified correct - not the issue
4. **Exponent processing**: Early exit logic may skip necessary operations

## Recommendations

1. **Use proven bignum library**: Import tiny-bignum-c directly from official source
2. **Alternative**: Switch to Ed25519 host keys (no RSA, smaller code)
3. **Debug approach**: Add comprehensive tracing to see where values diverge from Python
4. **Comparison testing**: Generate test vectors with OpenSSL/Python for each intermediate step

## Next Steps
1. Consider importing official tiny-bignum-c (public domain, proven correct)
2. OR: Switch SSH host key algorithm from RSA-2048 to Ed25519 (25519 library is proven)
3. OR: Continue debugging with detailed step-by-step comparison against Python
4. Test against real SSH client once fixed

## Test Commands
```bash
# Test RSA math property
gcc -o test_rsa_math test_rsa_math.c -I. && ./test_rsa_math

# Test against SSH client
./nano_ssh_server &
ssh -p 2222 root@127.0.0.1
```

## Files
- `test_rsa_key.pem`: Valid RSA-2048 key (OpenSSL verified)
- `test_rsa_math.c`: Demonstrates the bug
- `bignum_tiny.h`: Contains buggy Montgomery implementation
- `rsa.h`: RSA key loading (CORRECT)
