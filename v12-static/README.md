# v12-static: Fully Static Nano SSH Server

## Overview

This is the **statically linked** version of the Nano SSH Server. All dependencies (libsodium, libcrypto, glibc) are compiled directly into the binary, making it completely self-contained with **zero external dependencies**.

## Quick Stats

| Metric | Value | vs v11-opt10 |
|--------|-------|--------------|
| **Uncompressed Size** | 5.2 MB | 346× larger |
| **Compressed Size (UPX)** | 1.7 MB | 148× larger |
| **Dependencies** | **None** | ✅ Fully static |
| **Portability** | ✅ Excellent | Works anywhere |

## Build

```bash
# Install dependencies (static libraries)
apt-get install libsodium-dev libssl-dev upx-ucl

# Build
make clean
make
```

## Output Files

- `nano_ssh_server` - Static binary (5,364,832 bytes)
- `nano_ssh_server.upx` - Compressed with UPX (1,711,404 bytes)

## Verification

```bash
# Confirm static linking
$ ldd nano_ssh_server
not a dynamic executable

# Confirm it runs
$ ./nano_ssh_server
[Server starts on port 2222]
```

## Use Cases

### ✅ Perfect For:

- **Docker/Kubernetes:** FROM scratch compatible
- **Portable deployment:** No dependency hell
- **Legacy systems:** Library version independence
- **Air-gapped systems:** No external dependencies
- **Security deployments:** Control exact library versions

### ❌ Not Ideal For:

- **Embedded systems:** Too large (use v11-opt10 at 11KB)
- **Size-critical apps:** 346× larger than dynamic version
- **Multiple instances:** No shared library benefits

## Why So Large?

The binary includes:
- libsodium (~2-3MB): Curve25519, Ed25519, HMAC-SHA256
- libcrypto (~2-3MB): AES-128-CTR encryption
- glibc (~500KB): Standard C library
- SSH server (~15KB): Protocol implementation

## Dockerfile Example

```dockerfile
FROM scratch
COPY v12-static/nano_ssh_server.upx /nano_ssh_server
EXPOSE 2222
ENTRYPOINT ["/nano_ssh_server"]
```

**Total image size:** 1.7MB 🎉

## Comparison with v11

| Aspect | v11-opt10 (Dynamic) | v12-static |
|--------|---------------------|------------|
| Size | 11KB | 1.7MB (compressed) |
| Dependencies | 3 libraries | None |
| Portability | Medium | Excellent |
| Containers | Needs base image | FROM scratch |
| Security updates | Automatic | Requires rebuild |

## Documentation

See [SIZE_COMPARISON.md](SIZE_COMPARISON.md) for detailed analysis.

## Testing

```bash
# Start server
./nano_ssh_server

# In another terminal, connect
ssh -p 2222 user@localhost
# Password: password123
```

Expected output: "Hello World"

## Credits

Based on v11-opt10 by the Nano SSH Server project.

**Static linking modification:** 2025-11-05
