# v16-crypto-standalone Bug Report

## Date
2025-11-06

## Test Environment
- OpenSSH Client: 9.6p1 Ubuntu-3ubuntu13.14
- Server: v16-crypto-standalone (freshly compiled)
- Platform: Linux x86_64

## Issue Summary
**v16-crypto-standalone FAILS to complete SSH handshake with real SSH clients due to invalid DH public key**

## Test Results

### Connection Attempt

```bash
$ sshpass -p "password123" ssh -v \
    -o HostKeyAlgorithms=+ssh-rsa \
    -o PubkeyAcceptedKeyTypes=+ssh-rsa \
    -p 2222 user@localhost
```

### What Worked ✅
1. **TCP Connection**: Established successfully
2. **Version Exchange**:
   - Client: SSH-2.0-OpenSSH_9.6p1
   - Server: SSH-2.0-NanoSSH_0.1
3. **Algorithm Negotiation**:
   - KEX: diffie-hellman-group14-sha256
   - Host Key: ssh-rsa
   - Cipher: aes128-ctr
   - MAC: hmac-sha2-256
4. **Host Key Verification**: RSA host key accepted

### What Failed ❌
**DH Key Exchange** - Error from OpenSSH client:
```
invalid public DH value: <= 1
ssh_dispatch_run_fatal: Connection to 127.0.0.1 port 2222: incomplete message
```

## Root Cause Analysis

### Problem
The OpenSSH client rejected the server's DH public key because it's either **0** or **1**, which are cryptographically insecure values.

### Why This Happens
OpenSSH performs validation on DH public keys to prevent attacks:
```c
// OpenSSH validation (from dh.c)
if (pub <= 1 || pub >= p-1) {
    return SSH_ERR_MESSAGE_INCOMPLETE;
}
```

### Possible Causes

1. **Bignum modexp bug**: The `bn_modexp()` function in `bignum_tiny.h` may be computing incorrect results for `2^priv mod p`

2. **Byte order issue**: The DH public key export might have endianness problems

3. **Uninitialized memory**: The bignum structures might not be properly zeroed

4. **Overflow/underflow**: The modexp calculation could be overflowing

## Evidence

### No End-to-End Testing
Reviewing `/home/user/nano_ssh_server/v16-crypto-standalone/V16_IMPLEMENTATION_REPORT.md`:
- ✅ Unit tests passed (25/25 tests)
- ✅ Crypto primitives tested individually
- ❌ **NO evidence of successful SSH client connection test**
- ❌ **NO integration test with OpenSSH**

The `manual_test.log` file shows successful connections, but these are from **older versions using libsodium** (Curve25519), not the custom DH Group14 implementation.

### Test Comparison

| Version | KEX Algorithm | Library | Status |
|---------|---------------|---------|--------|
| v12-opt11 (manual_test.log) | curve25519-sha256 | libsodium | ✅ Works |
| v16-crypto-standalone | diffie-hellman-group14-sha256 | Custom | ❌ **Fails** |

## Impact
**CRITICAL** - v16-crypto-standalone cannot establish SSH connections with real clients.

## Recommendations

### Immediate Actions
1. **Debug bignum library**: Add extensive logging to `bn_modexp()` to trace the computation
2. **Test vectors**: Create test cases for DH Group14 key generation with known outputs
3. **Compare with working implementation**: Test against a known-good DH library (like OpenSSL's BIGNUM)

### Testing Requirements
Before claiming v16-crypto-standalone works:
1. ✅ Unit tests (already passing)
2. ❌ **Integration test with OpenSSH client** (currently failing)
3. ❌ **Manual verification of "Hello World" message** (not reached)
4. ❌ **Multiple connection tests** (only tested once)

### Debugging Steps

1. **Add debug output to DH keypair generation**:
   ```c
   printf("Private key (hex): ");
   for(int i=0; i<32; i++) printf("%02x", private_key[i]);
   printf("\n");

   printf("Public key (hex): ");
   for(int i=0; i<32; i++) printf("%02x", public_key[i]);
   printf("\n");
   ```

2. **Verify bignum operations**:
   ```c
   // After bn_modexp(&pub, &generator, &priv, &prime);
   printf("pub.array[0] = %u\n", pub.array[0]);
   printf("pub is_zero = %d\n", bn_is_zero(&pub));
   ```

3. **Test with OpenSSL for comparison**:
   Create a parallel implementation using OpenSSL's BN_mod_exp() and compare outputs

## Conclusion

**v16-crypto-standalone has a critical bug preventing real SSH connections.**

While the concept is sound and unit tests pass, the DH key exchange implementation fails in practice. The server cannot be recommended for use until this issue is resolved and verified with end-to-end testing against real SSH clients.

---

**Status**: BROKEN
**Severity**: CRITICAL
**Affects**: All SSH connections using diffie-hellman-group14-sha256
**Workaround**: None - fundamental implementation issue
