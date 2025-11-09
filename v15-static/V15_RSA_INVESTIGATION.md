# RSA Signature Verification Issue in v15-static

## Problem
OpenSSH and Paramiko both report "incorrect signature" when connecting to v15-static, even though the RSA signature is cryptographically valid.

## What Works
- ✅ v14-static with Ed25519 signatures - **WORKS PERFECTLY**
- ✅ RSA signature computation is correct (verified with OpenSSL)
- ✅ PKCS#1 v1.5 padding is correct
- ✅ Host key blob is correctly formatted
- ✅ Signature blob structure matches RFC 8332
- ✅ Local RSA sign/verify works

## What Fails
- ❌ v15-static with RSA-2048 and rsa-sha2-256
- ❌ Both OpenSSH and Paramiko reject the signature
- ❌ Error: "incorrect signature" or "Signature verification (rsa-sha2-256) failed"

## Investigation Results

### Bugs Found and Fixed

1. **Fake RSA Key** (FIXED v15-011CUuVXeZTYwExpPBTfULM5:f029c97)
   - Original issue: Hardcoded RSA key was placeholder data
   - Fix: Generated valid RSA-2048 keypair with OpenSSL
   - Result: Progressed from "error in libcrypto" to "incorrect signature"

2. **Exchange Hash Q_C Padding Issue** (ATTEMPTED FIX)
   - Issue: Client sends Q_C as mpint, we were padding it to 256 bytes
   - Problem: `write_mpint()` strips leading zeros, changing the representation
   - Fix attempt: Keep original mpint data from client without modification
   - Result: Buffer overflow (fixed by increasing buffer to 257 bytes)
   - Current status: Still failing with "incorrect signature"

### Technical Details

**Exchange Hash Computation (RFC 4253 Section 8):**
```
H = SHA256(V_C || V_S || I_C || I_S || K_S || e || f || K)
```

Where:
- V_C, V_S: version strings (as SSH strings)
- I_C, I_S: KEXINIT payloads (as SSH strings)
- K_S: server host key (as SSH string)
- e (Q_C): client ephemeral public key (as mpint)
- f (Q_S): server ephemeral public key (as mpint)
- K: shared secret (as mpint)

**Current Implementation:**
- Q_C: Now using original bytes from client (after reading with `read_string`)
- Q_S: Generated locally, encoded with `write_mpint()`
- K: Computed locally, encoded with `write_mpint()`

### Verification Steps Taken

1. **OpenSSL Verification:**
   ```bash
   openssl rsautl -verify -pubin -inkey rsa_public.pem \
                  -in signature.bin -out decrypted.bin -raw
   # Result: Perfectly valid PKCS#1 v1.5 padded message
   ```

2. **Local RSA Test:**
   - Sign test message with private key
   - Verify with public key
   - Result: SUCCESS

3. **Paramiko Test:**
   - Attempted connection with paramiko library
   - Result: Same failure - "Signature verification (rsa-sha2-256) failed"

### Current Code State

**v15-static/main.c:**
- Lines 1085-1107: Keep original Q_C mpint data (257 bytes max)
- Lines 719-722: Add Q_C to exchange hash with length prefix (no write_mpint)
- Lines 1167-1183: RSA signature uses `write_string()` encoding

**v15-static/rsa.h:**
- Lines 33-102: Valid RSA-2048 keypair (generated with OpenSSL)

## Remaining Questions

1. **Is there a subtle difference in how DH Group14 exchange hash should be computed vs ECDH?**
   - v14 uses Curve25519 (ECDH) - works
   - v15 uses DH Group14 - fails

2. **Are Q_S and K being encoded correctly?**
   - Both use `write_mpint()` which strips leading zeros
   - Maybe they also need special handling?

3. **Is there an issue with the rsa-sha2-256 implementation specific to DH Group14?**
   - Algorithm negotiation succeeds
   - Client accepts host key
   - Only signature verification fails

## Next Steps

### Option 1: Continue RSA Investigation
- Capture actual SSH packets with Wireshark
- Compare byte-by-byte with working SSH server
- Check if Q_S and K also need original byte preservation
- Review OpenSSH source code for rsa-sha2-256 verification

### Option 2: Use Ed25519 (Known Working)
- v14-static proves Ed25519 works
- Much simpler implementation
- Industry standard for modern SSH

### Option 3: Try Different Approach
- Test with older SSH clients (maybe more verbose errors)
- Try diffie-hellman-group14-sha1 instead of sha256
- Test with different RSA key sizes

## Files Modified
- `v15-static/main.c`: Exchange hash Q_C handling, RSA signature
- `v15-static/rsa.h`: Valid RSA keypair

## References
- RFC 4253: SSH Transport Layer Protocol
- RFC 8332: RSA SHA-2 Signature Algorithms
- RFC 3526: DH Group14 Prime
