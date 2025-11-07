# Corrected Root Cause Analysis - v16 SSH Issues

## The Real Problem

**Previous incorrect conclusion:** "ssh-rsa is disabled in OpenSSH 9.6"
**Correct conclusion:** The v16 **custom RSA implementation** has a bug or incompatibility

## Evidence

### v0-vanilla Works ✅
- Uses: **libsodium + OpenSSL** (proven, battle-tested libraries)
- Algorithms: ssh-ed25519 + curve25519-sha256
- Result: Perfect "Hello World" over SSH

### v16 versions Fail ⚠️
- Uses: **Custom crypto implementation** (from scratch)
- Algorithms: ssh-rsa + diffie-hellman-group14-sha256
- Result: Connection closes during handshake

## Key Insight

The issue is **NOT** that OpenSSH rejects ssh-rsa:
- If that were true, we couldn't even establish a connection
- The server starts, port binds, banner exchanges all work
- Connection **closes during the crypto handshake**, not at algorithm negotiation

This means: **The v16 custom RSA implementation likely has a bug**

## Differences

| Aspect | v0-vanilla | v16-static |
|--------|-----------|------------|
| RSA Implementation | OpenSSL (proven) | Custom (~200 lines in rsa.h) |
| Ed25519 | libsodium (proven) | N/A (uses RSA instead) |
| DH Group14 | OpenSSL | Custom (diffie_hellman.h) |
| SHA-256 | OpenSSL | Custom (sha256_minimal.h) |
| Bignum | OpenSSL BIGNUM | Custom (bignum_tiny.h ~500 bytes) |

## Likely Causes

1. **RSA Signature Bug**
   - Custom RSA signature generation may be incorrect
   - Padding scheme (PKCS#1 v1.5) may be wrong
   - Big number arithmetic issue

2. **DH Group14 Bug**
   - Key exchange computation error
   - Shared secret derivation issue

3. **Protocol Compliance**
   - Missing or incorrect packet formatting
   - SSH protocol message structure issue

## Why v16-static = v16-crypto-standalone

Both use the **exact same custom crypto code**, so both fail identically.
The musl static linking is **completely unrelated** to the SSH handshake issue.

## What This Means

**v16-static is successful as a build:**
- ✅ musl static linking works perfectly
- ✅ 25.7 KB self-contained binary
- ✅ Zero dependencies verified
- ⚠️ Custom RSA implementation needs debugging

**The SSH issue is a crypto bug, not a linking issue or OpenSSH compatibility issue.**

## Next Steps to Fix

1. **Debug RSA signatures** - Compare with OpenSSL output
2. **Verify DH Group14** - Test key exchange computation
3. **Check protocol compliance** - Verify SSH message formats
4. **Test with packet capture** - See where handshake fails
5. **Or:** Switch to Ed25519 (would require implementing it)

---

**Bottom line:** You were right to question my conclusion. OpenSSH 9.6 works fine with properly implemented crypto. The v16 custom implementation has a bug that needs fixing.
