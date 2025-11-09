# Cruft Directory

This directory contains experimental files, failed attempts, and draft documentation from the libsodium independence project development process.

## Contents

### Investigation Documentation
- `BUG_FOUND.md` - Discovery of Curve25519 bug
- `DEEP_DEBUGGING_REPORT.md` - Comprehensive debugging analysis
- `INVESTIGATION_SUMMARY.md` - Initial investigation summary
- `PACKET_COMPARISON.md` - Packet-level analysis
- `TESTING_STATUS.md` - Testing progress tracking

### Old Implementation Documentation
- `CHANGES.txt` - Change log
- `CRYPTO_SIZE_ANALYSIS.md` - Size analysis
- `IMPLEMENTATION_REPORT.md` - Implementation notes
- `OPTIMIZATION_REPORT.md` - Optimization attempts
- `SIZE_ANALYSIS.md` - Size measurements
- `SUMMARY.md` - Project summary

### Experimental Code
- `main.c` - Original development version with debug traces
- `main.c.hybrid_attempt` - Failed hybrid v14 protocol + v17 crypto
- `main.c.v14backup` - Backup of v14 code
- `main.c.v14exact` - Exact v14 copy for testing
- `main_test.c` - Test version

### Failed Crypto Implementations
- `curve25519_minimal.h` - Custom Curve25519 (has bug causing signature rejection)
- `fe25519.h` - Field element operations (unused)

### Old Compatibility Layers
- `sodium_compat.h` - Failed compatibility layer (included buggy Curve25519)
- `sodium_compat.h.backup` - Backup
- `sodium_compat_libsodium_curve.h` - Experimental libsodium Curve25519 version

### Test Programs (13 total)
All test programs that were created during debugging:
- `test_actual_keys.c` / `test_actual_keys` - Test with actual server keys
- `test_compat_funcs.c` - Compatibility function testing
- `test_compat_layer.c` - Compatibility layer testing
- `test_cross_verify.c` / `test_cross_verify` - Cross-verification tests
- `test_exact_server_flow.c` / `test_exact_server_flow` - Server flow reproduction
- `test_key_format.c` / `test_key_format` - Key format testing
- `test_openssh_verify.c` / `test_openssh_verify` - OpenSSH verification testing
- `test_sig_compat.c` / `test_sig_compat` - Signature compatibility testing

### Old Binaries
- `nano_ssh_server` - Development binary with debug output
- `nano_ssh_server.upx` - UPX-compressed version
- `nano_ssh_server_test` - Test binary

### Utilities
- `generate_test_vectors.c` - Test vector generator
- `remove_fprintf.py` - Debug output removal script

### Logs
- `server.log` - Server debug output

## Production Files

The production-ready files remain in the parent directory (`v17-from14/`):
- `nano_ssh_server_production` - Production binary (47KB)
- `main_production.c` - Clean production code
- `sodium_compat_production.h` - Production compatibility layer
- `FINAL_PRODUCTION_README.md` - Production documentation
- `PROJECT_COMPLETE.md` - Project completion summary

## Historical Value

These files document the complete investigation that led to discovering:
1. c25519 Ed25519 is 100% compatible with OpenSSH
2. Custom Curve25519 has a subtle bug causing signature rejection
3. Hybrid configuration (c25519 Ed25519 + libsodium Curve25519) works perfectly

The breakthrough came from minimal diff testing (v14-diff-test framework) that systematically replaced functions one-by-one to isolate the bug.
