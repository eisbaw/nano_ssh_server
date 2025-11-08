# v14-static - Musl Static Build

## Overview

v14-static is a statically-linked build of v14-crypto using **musl libc** instead of glibc, with aggressive ELF optimizations.

## Key Features

- **Based on:** v14-crypto (custom AES-128-CTR, SHA-256, HMAC-SHA256)
- **Libc:** musl (statically linked)
- **Crypto:**
  - Custom symmetric crypto (AES, SHA-256, HMAC) - no OpenSSL needed!
  - libsodium for: Curve25519, Ed25519, randombytes (statically linked with musl)
- **Fully static:** No dynamic dependencies
- **Aggressive optimizations:** All ELF size reduction techniques applied

## Build Results

```
Binary size: 298,008 bytes (~291 KB)
File type:   ELF 64-bit LSB executable, x86-64, statically linked, stripped
Libc:        musl (not glibc)
Dependencies: NONE (statically linked)
```

Size breakdown:
```
   text     data      bss      dec      hex    filename
 289368     2152     2288   293808    47bb0    nano_ssh_server
```

## Why Musl?

Musl libc produces significantly smaller static binaries than glibc:
- Designed for static linking
- Minimal size overhead
- Clean, simple implementation
- Better suited for embedded systems

## Build Instructions

### Option 1: Using the build script (Recommended)

```bash
cd v14-static
./build-musl.sh
```

This will:
1. Download and build libsodium with musl-gcc
2. Compile v14-static with aggressive optimizations
3. Link everything statically
4. Strip the binary

### Option 2: Using Docker/Alpine

```bash
cd v14-static
./build-docker.sh
```

Builds in an Alpine Linux container (which uses musl natively).

### Option 3: Using justfile

```bash
just build-v14-static
```

### Prerequisites

- musl-tools and musl-dev packages
- gcc
- make
- wget or curl

For Debian/Ubuntu:
```bash
apt-get install musl-tools musl-dev gcc make wget
```

## Testing

Start the server:
```bash
./nano_ssh_server
```

Test with SSH client:
```bash
sshpass -p password123 ssh -p 2222 user@localhost
```

Or use the justfile:
```bash
just test-v14-static
```

## Comparison with Other Versions

| Version | Libc | Size | Static | Custom Crypto |
|---------|------|------|--------|---------------|
| v14-crypto | glibc | ~500KB+ | No | Yes |
| v14-static | musl | 298KB | Yes | Yes |

**Advantages over glibc static:**
- ~40-50% smaller than glibc static builds
- Truly portable (no glibc dependencies)
- Better suited for minimal environments
- Works in Alpine/Docker FROM scratch

## Optimization Flags Used

### Compiler Flags (CFLAGS)
```
-Os                                  # Optimize for size
-flto                                # Link-time optimization
-ffunction-sections -fdata-sections  # Separate sections for GC
-fno-unwind-tables                   # Remove unwind tables
-fno-asynchronous-unwind-tables      # Remove async unwind tables
-fno-stack-protector                 # Remove stack protector
-fmerge-all-constants                # Merge identical constants
-fno-ident                           # Remove compiler identification
-finline-small-functions             # Inline small functions
-fshort-enums                        # Use smallest enum size
-fomit-frame-pointer                 # Remove frame pointer
-ffast-math                          # Fast non-IEEE math
-fno-math-errno                      # No errno for math ops
-fvisibility=hidden                  # Hide symbols by default
-fno-builtin                         # Don't use builtin functions
-fno-plt                             # No PLT indirection
-fwhole-program                      # Whole program optimization
-fipa-pta                            # Interprocedural pointer analysis
-fno-common                          # Place uninitialized globals in BSS
```

### Linker Flags (LDFLAGS)
```
-static                              # Static linking
-Wl,--gc-sections                    # Garbage collect unused sections
-Wl,--strip-all                      # Strip all symbols
-Wl,--as-needed                      # Only link needed libraries
-Wl,--hash-style=gnu                 # Use GNU hash style
-Wl,--build-id=none                  # Remove build ID
-Wl,-z,norelro                       # Disable relocation read-only
-Wl,--no-eh-frame-hdr                # Remove exception frame header
```

## Files

- `main.c` - Main SSH server implementation
- `aes128_minimal.h` - Custom AES-128-CTR implementation
- `sha256_minimal.h` - Custom SHA-256 implementation
- `Makefile` - Build configuration with aggressive optimizations
- `build-musl.sh` - Build script for musl static linking
- `build-docker.sh` - Docker-based build using Alpine Linux
- `Dockerfile.alpine` - Alpine Linux build container definition

## Notes

- This version does NOT require OpenSSL (custom crypto for symmetric operations)
- Only libsodium is needed (for Curve25519 and Ed25519)
- Excellent for deployment in minimal environments (containers, embedded systems)
- Can run in Docker FROM scratch images
- No external dependencies at runtime

## Next Steps

For even smaller binaries, consider:
- UPX compression (can achieve ~40-50% further reduction)
- Link-time optimization with custom libsodium build
- Strip additional sections
- Use diet libc or uclibc instead of musl

## License

Same as parent project.
