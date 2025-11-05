# Crypto Optimization Series - Test Results

## Test Summary

### v13-crypto1: ✓ ALL TESTS PASSED

**Size:** 9,932 bytes (9.69 KB) compressed
**Dependencies:** libsodium + OpenSSL
**Savings:** 1,680 bytes (14.5% smaller than v12-opt11)

**Test Results:**
```
✓ Version Exchange: PASSED
✓ Full SSH Connection: PASSED  
✓ Authentication: PASSED
```

**Changes:**
- Replaced libsodium SHA-256 with minimal custom implementation
- Replaced libsodium HMAC-SHA256 with minimal custom implementation
- Replaced libsodium crypto_verify_32 with ct_verify_32
- All crypto functions work correctly with SSH protocol

**Status:** **READY FOR USE** (pending security audit)

---

### v14-crypto2: ✗ TESTS FAILED - AES IMPLEMENTATION BUG

**Size:** 10,556 bytes (10.30 KB) compressed  
**Dependencies:** libsodium only
**Target:** Remove OpenSSL dependency

**Test Results:**
```
✓ Version Exchange: PASSED
✗ Full SSH Connection: FAILED
✗ Authentication: FAILED
```

**Root Cause:**
Custom AES-128 block cipher implementation produces incorrect output.

**Status:** **NOT READY - REQUIRES AES FIX**

---

## Recommendation

**Use v13-crypto1 for crypto optimization.**

v13-crypto1 successfully:
- Reduces binary size by 14.5%
- Eliminates libsodium SHA-256/HMAC dependency  
- Passes all SSH protocol tests
- Works with real SSH clients

---

## Size Comparison

| Version    | Size (bytes) | Status | Notes |
|------------|--------------|--------|-------|
| v12-opt11  | 11,612       | ✓      | Baseline |
| v13-crypto1| 9,932        | ✓✓✓    | SHA-256/HMAC replaced, WORKS |
| v14-crypto2| 10,556       | ✗      | AES replaced, BROKEN |
