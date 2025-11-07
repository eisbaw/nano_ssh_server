# Nano SSH Server

World's smallest SSH server for microcontrollers - a minimal SSH-2.0 implementation that works with standard Linux SSH clients.

## Project Goals

Build the smallest possible SSH server that:
- ✅ Works with standard `ssh` clients (OpenSSH)
- ✅ Implements SSH-2.0 protocol
- ✅ Targets microcontrollers (ARM Cortex-M, ESP32, etc.)
- ✅ Prioritizes binary size over features
- ✅ Uses platform abstraction for portability

## Quick Start

### Prerequisites

**Option 1: Using Nix (Recommended)**
```bash
nix-shell
```

**Option 2: Manual Installation**
```bash
# Ubuntu/Debian
sudo apt-get install gcc make just openssh-client valgrind gdb sshpass xxd libsodium-dev

# Other systems: see shell.nix for required packages
```

### Build and Run

```bash
# Build v0-vanilla (Phase 1: working implementation)
just build v0-vanilla

# Run the server (port 2222)
just run v0-vanilla

# In another terminal, connect with SSH client
just connect
# Or manually:
ssh -p 2222 user@localhost
# Credentials: user / password123
```

### Available Commands

```bash
just --list              # Show all commands
just build <version>     # Build specific version
just test <version>      # Run tests
just clean <version>     # Clean build artifacts
just size-report         # Compare binary sizes
just debug <version>     # Run in debugger
just valgrind <version>  # Check for memory leaks
```

## Project Structure

```
nano_ssh_server/
├── shell.nix              # Nix development environment
├── justfile               # Task automation
├── Makefile               # Top-level build orchestration
├── PRD.md                 # Product Requirements Document
├── TODO.md                # Task breakdown and tracking
├── CLAUDE.md              # Development guidelines
├── README.md              # This file
├── docs/                  # Implementation guides and RFCs
├── cruft/                 # Historical reports and analysis
├── v0-vanilla/            # 70 KB - Baseline implementation
├── v1-portable/           # 70 KB - Platform abstraction
├── v2-opt1 → v13-opt11/   # 30→11 KB - Progressive optimizations
├── v12-static/            # 5.2 MB - Statically linked
├── v14-crypto/            # 16 KB - Drops OpenSSL
├── v15-crypto/            # 20 KB - Self-contained crypto ⭐
├── v16-crypto-standalone/ # 20 KB - Fully standalone ⭐
└── tests/                 # Test scripts
```

## Development Phases

### Phase 0: Project Setup ✅
- [x] Nix environment
- [x] Build system (Makefile + justfile)
- [x] Directory structure
- [x] Basic compilation test

### Phase 1: v0-vanilla ✅
**Goal:** Working SSH server, correctness first

Implements:
- TCP server (POSIX sockets)
- SSH-2.0 version exchange
- Binary packet protocol
- Curve25519 key exchange (via libsodium)
- ChaCha20-Poly1305 encryption (via libsodium)
- Password authentication
- Session channel
- Actual SSH protocol implementation

**Result:** 70 KB baseline implementation

### Phase 2: v1-portable ✅
**Goal:** Platform-independent code

- Platform abstraction layer
- Network abstraction (net_posix.c)
- Prepare for microcontroller deployment
- Identical functionality to v0

**Result:** 70 KB (same as v0, abstraction complete)

### Phase 3: Size Optimizations ✅
**Goal:** Progressively smaller binaries (v2-v16)

Major milestones achieved:
- **v2-v5:** Compiler optimizations (70 KB → 30 KB)
- **v6-v7:** Aggressive opts (30 KB → 23 KB)
- **v8-v10:** Advanced minimization (23 KB → 11.5 KB) ⭐
- **v11-v14:** Refinement & crypto experiments (11-16 KB)
- **v15-v16:** Self-contained crypto implementations (20 KB) 🔒

**Results:**
- Smallest with external libs: **11.4 KB** (v14-opt12)
- Self-contained: **20.3 KB** (v15-crypto, v16-crypto-standalone)
- Static build: **5.2 MB** (v12-static)
- Overall reduction: **84%** from baseline

## Implementation Details

### SSH Protocol Support

**Supported:**
- SSH-2.0 protocol
- Single cipher: ChaCha20-Poly1305
- Single key exchange: curve25519-sha256
- Single host key: ssh-ed25519
- Password authentication only
- Single session channel
- No compression

**Not Supported (Minimal Implementation):**
- SSH-1.0 protocol
- Multiple cipher negotiation
- Public key authentication
- Shell/PTY spawning (microcontrollers don't have bash)
- X11/port forwarding
- SFTP subsystem
- Multiple simultaneous connections

### Cryptography

**Two approaches implemented:**

**v0-v14: External libraries (libsodium)**
- ChaCha20-Poly1305: AEAD cipher
- Curve25519: Key exchange
- Ed25519: Host key signatures
- SHA-256: Exchange hash (from OpenSSL in most versions)

**v15-v16: Self-contained crypto ⭐ Recommended for embedded**
- AES-128-CTR: Symmetric encryption
- SHA-256: Hashing
- HMAC-SHA256: Message authentication
- DH Group14: Key exchange
- RSA-2048: Host key
- CSPRNG: Random number generation
- Custom bignum library (v16 only)
- **Zero external crypto dependencies** - only requires libc

## Testing

```bash
# Run all tests for a version
just test v0-vanilla

# Test all versions
just test-all

# Manual testing
just run v0-vanilla
# In another terminal:
ssh -vvv -p 2222 user@localhost

# Memory leak check
just valgrind v0-vanilla
```

## Development Guidelines

**MUST READ before coding:**
1. **CLAUDE.md** - Mandatory development practices
2. **PRD.md** - Project requirements and architecture
3. **TODO.md** - Task breakdown and priorities

**Key Rules:**
- Use `nix-shell` for reproducible builds
- Use `just` for all commands (never raw make/gcc)
- Test before optimize (all tests must pass)
- One task at a time (follow TODO.md)
- Commit frequently with clear messages

## Documentation

- **PRD.md** - Product Requirements Document
- **TODO.md** - Detailed task breakdown
- **CLAUDE.md** - Development guidelines (MUST READ)
- **docs/** - Implementation guides and RFC summaries

## Version Comparison & Binary Sizes

All 24 versions successfully built and analyzed. Size progression shows 84% reduction from baseline.

### Champions by Category

| Category | Version | Size | Dependencies | Notes |
|----------|---------|------|--------------|-------|
| **Smallest Overall** | v14-opt12 | **11.4 KB** | libsodium + OpenSSL | Requires external libs |
| **Smallest Self-Contained** | v15-crypto | **20.3 KB** | libc only | ⭐ Zero crypto deps |
| **Most Standalone** | v16-crypto-standalone | **20.3 KB** | libc only | ⭐ Custom bignum |
| **Fully Static** | v12-static | **5.2 MB** | none | No runtime deps |

### Complete Version Table

| Version | Size (KB) | Linking | Dependencies | Optimization Focus |
|---------|-----------|---------|--------------|-------------------|
| v0-vanilla | 70.06 | Dynamic | libsodium + libcrypto + libc | Baseline implementation |
| v1-portable | 70.06 | Dynamic | libsodium + libcrypto + libc | Platform abstraction |
| v2-opt1 | 30.12 | Dynamic | libsodium + libcrypto + libc | Compiler optimizations |
| v3-opt2 | 30.12 | Dynamic | libsodium + libcrypto + libc | Further optimizations |
| v4-opt3 | 30.12 | Dynamic | libsodium + libcrypto + libc | Static buffers |
| v5-opt4 | 30.04 | Dynamic | libsodium + libcrypto + libc | State machine |
| v6-opt5 | 26.04 | Dynamic | libsodium + libcrypto + libc | Aggressive opts |
| v7-opt6 | 22.85 | Dynamic | libsodium + libcrypto + libc | Size reduction |
| v8-opt7 | 15.18 | Dynamic | libsodium + libcrypto + libc | Minimization |
| v9-opt8 | 15.18 | Dynamic | libsodium + libcrypto + libc | Continued opts |
| **v10-opt9** | **11.53** | Dynamic | libsodium + libcrypto + libc | **Smallest w/ libs** |
| v11-opt10 | 15.18 | Dynamic | libsodium + libcrypto + libc | Refinement |
| v12-dunkels1 | 15.16 | Dynamic | libsodium + libcrypto + libc | Dunkels style |
| v12-opt11 | 15.18 | Dynamic | libsodium + libcrypto + libc | Further refinement |
| v12-static | 5239.09 | **Static** | **(none)** | Fully static |
| v13-crypto1 | 15.39 | Dynamic | libsodium + libcrypto + libc | Crypto work |
| v13-opt11 | 11.63 | Dynamic | libsodium + libcrypto + libc | Optimization |
| v14-crypto | 15.60 | Dynamic | libsodium + libc | Drops OpenSSL |
| v14-dunkels3 | 15.18 | Dynamic | libsodium + libcrypto + libc | Dunkels iter 3 |
| **v14-opt12** | **11.39** | Dynamic | libsodium + libcrypto + libc | **Smallest overall** |
| **v15-crypto** | **20.33** | Dynamic | **libc only** | **Self-contained** |
| v15-dunkels4 | 15.18 | Dynamic | libsodium + libcrypto + libc | Dunkels iter 4 |
| v15-opt13 | 15.18 | Dynamic | libsodium + libcrypto + libc | Further opts |
| **v16-crypto-standalone** | **20.33** | Dynamic | **libc only** | **Custom bignum** |

### Size Progression Graph

```
v0-vanilla    ████████████████████████████████████████████████ 70 KB
v1-portable   ████████████████████████████████████████████████ 70 KB
v2-opt1       █████████████████████ 30 KB
v6-opt5       ██████████████████ 26 KB
v7-opt6       ████████████████ 23 KB
v8-opt7       ███████████ 15 KB
v10-opt9      ████████ 11.5 KB  ⭐ Smallest (with external libs)
v14-opt12     ████████ 11.4 KB  ⭐ Smallest overall
v15-crypto    ██████████████ 20 KB 🔒 Self-contained
v16-crypto    ██████████████ 20 KB 🔒 Fully standalone
v12-static    ████████████████████████████████ 5.2 MB (static)
```

### Dependency Analysis

**Most versions (22):** Require libsodium + OpenSSL libcrypto
- Total footprint: ~11-70 KB binary + ~700 KB libraries

**v14-crypto (1):** Drops OpenSSL dependency
- Uses libsodium only
- Custom AES-128-CTR, SHA-256, HMAC-SHA256

**v15-v16 (2):** Zero crypto library dependencies ⭐
- Only requires standard libc
- 100% custom crypto implementations
- Total footprint: Just 20 KB
- Best for embedded systems

**v12-static (1):** Fully static build
- No runtime dependencies
- 5.2 MB (includes all libraries)
- Most portable

## Contributing

This is an educational project demonstrating:
- Minimal SSH protocol implementation
- Iterative size optimization
- Platform-independent embedded systems code
- Rigorous testing discipline

See **CLAUDE.md** for development workflow.

## Security Warning

⚠️ **This is an educational/experimental project.**

**DO NOT use in production environments.**

This server:
- Uses hardcoded credentials
- Has minimal security hardening
- Lacks DoS protection
- Is designed for size, not security

Intended for:
- Learning SSH protocol internals
- Microcontroller proof-of-concept
- Size optimization research

## License

TBD

## References

- RFC 4253: SSH Transport Layer Protocol
- RFC 4252: SSH Authentication Protocol
- RFC 4254: SSH Connection Protocol
- RFC 7748: Elliptic Curves (Curve25519)
- TweetNaCl: Compact crypto library
- libsodium: Modern crypto library

## Status

✅ **Project Complete** ✅

- [x] Phase 0: Project setup
- [x] Phase 1: v0-vanilla implementation (70 KB)
- [x] Phase 2: v1-portable implementation (70 KB)
- [x] Phase 3: Size optimizations (24 versions, 11-70 KB range)
- [x] Self-contained crypto implementations (v15-v16)

**Achievements:**
- 24 working SSH server versions
- 84% size reduction achieved
- Self-contained versions require only libc
- Comprehensive testing and analysis complete
- Multiple optimization strategies demonstrated

**Recommended versions:**
- **For embedded systems:** v15-crypto or v16-crypto-standalone (20 KB, no crypto deps)
- **For minimal size:** v14-opt12 (11.4 KB, requires libsodium + OpenSSL)
- **For maximum portability:** v12-static (5.2 MB, fully static)
