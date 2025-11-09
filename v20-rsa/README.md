# v20-rsa: RSA/DH SSH Server

## Status: 🟡 **EXPERIMENTAL - RSA IMPLEMENTATION**

This version replaces elliptic curve cryptography with RSA and traditional Diffie-Hellman.

## Major Changes from v19

✅ **Complete replacement of ECC with RSA/DH:**
- **Key Exchange**: DH Group14 (2048-bit) instead of Curve25519
- **Host Key**: RSA-2048 instead of Ed25519
- **Signature Algorithm**: ssh-rsa instead of ssh-ed25519
- All other crypto remains the same (AES-128-CTR, HMAC-SHA256, SHA-256)

## Why RSA/DH instead of ECC?

This version demonstrates an alternative cryptographic approach:
- **RSA** is the traditional public-key algorithm, widely supported
- **DH Group14** is the classic key exchange method (RFC 3526)
- Provides compatibility with older SSH implementations that don't support ECC

## Build Instructions

```bash
cd v20-rsa
make clean
make
ls -lh nano_ssh_server_production  # ~139KB binary
```

## Binary Size: ~139KB

Larger than v19 (41KB) due to:
- Big integer arithmetic library
- RSA-2048 operations
- DH Group14 implementation

## Testing

```bash
# Start server
./nano_ssh_server_production

# Connect (in another terminal)
ssh -p 2222 user@localhost
# Password: password123
```

---

**Created:** November 9, 2025  
**Status:** 🟡 **EXPERIMENTAL**  
**Binary Size:** ~139KB
