# Critical Bug: Montgomery Multiplication Implementation

## Status: IDENTIFIED - NOT YET FIXED

## Summary
RSA signature verification fails because `bn_modexp()` produces incorrect results. The RSA key values (n, d, e) are correct, but the Montgomery multiplication implementation has a bug.

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

## Next Steps
1. Debug Montgomery multiplication implementation
2. Either fix the bug or temporarily revert to simple binary modular exponentiation
3. Test against real SSH client once fixed

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
