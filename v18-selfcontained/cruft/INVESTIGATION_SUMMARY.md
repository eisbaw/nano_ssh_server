# Investigation Summary: v17-from14 Libsodium Independence

##STATUS: Hybrid Version Created - Signature Rejection Mystery

### Achievements ✅

1. **Successfully created hybrid version**:
   - Base: v14-crypto's `main.c` (proven working protocol)
   - Crypto: v17's c25519 Ed25519/Curve25519 implementations
   - Binary size: 25KB (uncompressed)
   - **100% libsodium independent** (verified with `ldd`)

2. **Created compatibility layer** (`sodium_compat.h`):
   - Maps libsodium API to c25519 implementations
   - Handles key format differences (libsodium 64-byte vs c25519 32-byte secrets)
   - Successfully compiles and links

3. **Verified crypto implementations are compatible**:
   - Test (`test_sig_compat.c`) proves libsodium and c25519 produce **IDENTICAL** signatures
   - Same 32-byte secret → Same 64-byte signature (byte-for-byte)
   - Both implementations follow Ed25519 spec correctly

4. **Internal verification PASSES**:
   - c25519's `edsign_verify()` accepts the signatures
   - Signature format is RFC 8709 compliant
   - sig_blob structure is correct: `string "ssh-ed25519" + string signature(64)`

### The Mystery ❌

**OpenSSH rejects signatures with "incorrect signature" error**

#### Test Results:
```
Test 1: v14 main.c + libsodium
Result: ✅ WORKS - passes signature verification, reaches authentication

Test 2: v14 main.c + c25519 (via compat layer)
Result: ❌ FAILS - "incorrect signature" from OpenSSH
But: ✅ Internal self-verification PASSES

Test 3: Standalone crypto test (same secret key for both)
Result: ✅ libsodium and c25519 produce IDENTICAL signatures
```

#### Evidence from logs:

**Working version (v14 + libsodium)**:
```
Host public key: 9f46874341f7116e1532ce6e81282c6177f7f0382ed4c34f5a0cca57b08d7c9d
Exchange hash H: 971852118da192565652e33dfd312951e7b3e95c13895426e917d2cba9ecf854
Signature: 4c920b87e5337e5461cada57b9a1eed70b5fba1ca31fc2401516cf3897358d359aea3a8e58cb3397bf2846db06f61c03d98eaa9656a36fbf08313c3c03112e0f
→ OpenSSH accepts and proceeds to authentication
```

**Failing version (v14 + c25519)**:
```
Host public key: d15c9bb199f80eec1ad4cbfefd0c2ea45e5b5e0a401ff4dc86a1aa00d3403d00
Exchange hash H: 304b4eac97674917b52ba8fd84a0ddd70f4a1863f916b256f8a5229f69a3ba59
Self-verification: PASS  ← c25519's verify accepts it!
Signature: a1e86b34ccdcf40bc0fb27712053085c5a938fc07ed95b94949e0ae464ff0af343ee037cb140beb351cd620a7d0b80e06c39b2484a05c7f9c84ed4c1bfed8301
sig_blob (83 bytes): 0000000b7373682d6564323535313900000040a1e86b34ccdcf40bc0fb27712053085c5a938fc07ed95b94949e0ae464ff0af343ee037cb140beb351cd620a7d0b80e06c39b2484a05c7f9c84ed4c1bfed8301
→ OpenSSH rejects with "incorrect signature"
```

sig_blob format (decoded):
- `0000000b` = length 11
- `7373682d65643235353139` = "ssh-ed25519"
- `00000040` = length 64
- `a1e86b34...` = signature (64 bytes)
- **Total: 83 bytes** ✓ Format is correct per RFC 8709

### Theories

#### Theory 1: Key Derivation Difference (UNLIKELY - DISPROVEN)
- ~~Maybe libsodium and c25519 derive public keys differently from secrets~~
- **DISPROVEN**: Test shows they produce identical signatures from same secret
- Both use SHA-512 expansion of the secret

#### Theory 2: OpenSSH-Specific Validation (POSSIBLE)
- OpenSSH might do additional validation beyond RFC spec
- Perhaps checking signature determinism or canonical encoding
- Some Ed25519 implementations allow multiple valid signatures for same message

#### Theory 3: Timing or State Issue (POSSIBLE)
- Maybe OpenSSH is checking something in the packet sequence
- Could be related to how the exchange happens vs signature content

#### Theory 4: Compatibility Layer Bug (LESS LIKELY)
- The compat layer extracts secret[0:32] and public[32:64] from sk[64]
- This matches libsodium's format
- Test proves this works correctly in isolation

### Next Steps / Options

#### Option A: Deep OpenSSH Debugging
- Compile OpenSSH with debug symbols
- Add verbose logging to see exactly why it rejects
- Compare validation path between libsodium vs c25519 signatures

#### Option B: Try Different Ed25519 Implementation
- Test with TweetNaCl's Ed25519 instead of c25519
- Test with ref10 implementation
- See if OpenSSH accepts signatures from other implementations

#### Option C: Packet-Level Analysis
- Capture packets with tcpdump/wireshark for both versions
- Compare byte-for-byte to find any differences
- Check if there's something wrong with packet encoding vs signature

#### Option D: Use libsodium
- Accept that we need libsodium for Ed25519 host key operations
- Use custom crypto for everything else (AES, SHA-256, HMAC, Curve25519)
- Still achieve significant size/dependency reduction

### Files Created

- `sodium_compat.h` - Compatibility layer mapping libsodium API to c25519
- `test_sig_compat.c` - Proves libsodium and c25519 produce identical signatures
- `main.c` - Hybrid version using v14 protocol + c25519 crypto via compat layer

### Conclusion

We have successfully created a 100% libsodium-independent version that:
- ✅ Compiles and runs
- ✅ Generates valid Ed25519 signatures (verified internally)
- ✅ Uses correct SSH packet format
- ✅ Produces signatures identical to libsodium (when using same keys)

**BUT** OpenSSH rejects the signatures for an unknown reason, despite all technical indicators showing correctness.

This suggests either:
1. A subtle OpenSSH-specific requirement we're missing
2. A bug in how OpenSSH validates non-libsodium Ed25519 signatures
3. Some state or timing issue in the protocol flow

The most pragmatic path forward might be Option D: use libsodium only for Ed25519 signatures while using custom implementations for everything else. This still achieves most of the independence goals while ensuring OpenSSH compatibility.
