# SSH Server Version Test Results

Test Date: 2025-11-09
Test Script: `test_ssh_versions_final.sh`

## Summary

**Overall Results: 5/9 PASS (55%)**

All versions tested with real SSH client using sshpass and port 2222.

## Detailed Results

### ✅ PASSING (5 versions)

| Version | Status | Notes |
|---------|--------|-------|
| v0-vanilla | ✅ PASS | Baseline implementation with libsodium + OpenSSL |
| v12-static | ✅ PASS | Fully static build (5.2 MB) |
| v17-from14 | ✅ PASS | Uses libsodium + custom Ed25519 |
| v19-donna | ✅ PASS | Uses Curve25519-donna |
| v20-opt | ✅ PASS | Latest optimized version |

### ❌ FAILING (4 versions)

| Version | Status | Issue | Details |
|---------|--------|-------|---------|
| v14-opt12 | ❌ FAIL | Segmentation fault | Crashes during initialization (before main), likely due to aggressive linker script |
| v15-crypto | ❌ FAIL | Connection closes | RSA-based, connection closes after handshake |
| v16-crypto-standalone | ❌ FAIL | Connection closes | RSA-based, connection closes after handshake |
| v18-selfcontained | ❌ FAIL | Signature verification | Ed25519 signature verification fails: "incorrect signature" |

## Known Issues

### RSA Versions (v15-crypto, v16-crypto-standalone)
- Both versions use RSA-2048 host keys
- Connection established but closes immediately
- Likely RSA implementation or key exchange issue
- Needs further investigation of RSA signature generation

### v14-opt12 - Segmentation Fault
```
Program received signal SIGSEGV, Segmentation fault.
#0  0x0000000000401952 in ?? ()
```
- Crashes during libc initialization (before main)
- Caused by aggressive optimization + custom linker script
- Binary: 11.4 KB (smallest version)
- May need to relax some optimizations for stability

### v18-selfcontained - Ed25519 Signature Error
```
ssh_dispatch_run_fatal: Connection to 127.0.0.1 port 2222: incorrect signature
```
- Key exchange completes successfully
- Ed25519 signature verification fails
- Possible issue with custom Ed25519 implementation
- Server logs show exchange hash and signature generation

## Test Configuration

- **Port**: 2222 (all servers)
- **Method**: Sequential testing (one server at a time)
- **Authentication**: Password (`user` / `password123`)
- **SSH Client**: OpenSSH with sshpass
- **Test Command**: `echo 'Hello World'`
- **Success Criteria**: SSH client receives and prints "Hello World"

## Build Status

All 9 versions successfully built:
- v0-vanilla: 70 KB
- v12-static: 5.2 MB
- v14-opt12: 11.4 KB (segfaults)
- v15-crypto: 21 KB
- v16-crypto-standalone: 21 KB
- v17-from14: 25 KB
- v18-selfcontained: 25 KB
- v19-donna: 41 KB
- v20-opt: 41 KB

## Recommendations

1. **Use passing versions for production/testing**: v0-vanilla, v12-static, v17-from14, v19-donna, v20-opt
2. **Investigate RSA implementations** in v15-crypto and v16-crypto-standalone
3. **Fix Ed25519 signature** in v18-selfcontained
4. **Relax optimizations** in v14-opt12 to fix segfault
5. **Consider v19-donna or v20-opt** as the recommended versions (both pass, reasonable size)

## Success Rate by Category

- **Baseline versions**: 1/1 (100%) - v0-vanilla ✅
- **Libsodium-based**: 3/4 (75%) - v0, v17, v19, v20 ✅ | v14 ❌
- **Custom crypto (RSA)**: 0/2 (0%) - v15, v16 ❌
- **Custom crypto (Ed25519)**: 2/3 (67%) - v19, v20 ✅ | v18 ❌
- **Static build**: 1/1 (100%) - v12-static ✅
- **Optimized**: 3/5 (60%) - v12, v17, v19, v20 ✅ | v14 ❌

## Conclusion

The test suite successfully validates that 5 out of 9 versions (55%) are fully functional with real SSH clients. The passing versions cover a good range from baseline (70 KB) to highly optimized (25-41 KB) implementations. The failing versions have identified specific issues that can be addressed in future development.
