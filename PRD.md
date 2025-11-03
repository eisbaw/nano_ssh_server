# Product Requirements Document: Nano SSH Server

## Project Overview

Build the world's smallest SSH server that remains compatible with standard Linux SSH clients, targeting **microcontroller platforms**. The primary goal is minimal binary size while maintaining core SSH protocol functionality.

This is an iterative project: start with a working vanilla implementation, verify compatibility with real SSH clients, then progressively optimize for size and portability across multiple independent versions.

## Core Principles

1. **Minimalism First**: Fewer features are better
2. **Size Optimization**: Every byte of code matters (critical for microcontrollers)
3. **Client Compatibility**: Must work with standard Linux ssh clients
4. **Testing First**: Testing with real SSH clients is paramount - iterate until it works
5. **Iterative Development**: Vanilla → Working → Portable → Optimized
6. **Platform Agnostic**: Design for microcontrollers through network abstraction layer
7. **Independent Versions**: Each optimization is a separate, independently compiled version

## Technical Requirements

### Language & Tooling

- **Language**: Plain C (C99 or C11 for portability)
- **Compiler**: GCC with size optimization flags
- **Build System**: GNU Make
- **Package Management**: Nix (shell.nix)
- **Task Runner**: justfile

### Platform Architecture

#### Target Platforms
- **Primary Development**: Linux (x86_64) for testing
- **Target Deployment**: Microcontrollers (ARM Cortex-M, ESP32, etc.)
- **Design Goal**: Zero platform-specific code in core logic

#### Network Abstraction Layer

**Critical for microcontroller portability:**

```c
// Abstract network interface - implement per platform
typedef struct {
    int (*init)(void);
    int (*accept)(void* ctx);
    ssize_t (*read)(void* ctx, uint8_t* buf, size_t len);
    ssize_t (*write)(void* ctx, const uint8_t* buf, size_t len);
    void (*close)(void* ctx);
} net_ops_t;
```

**Implementations:**
- `net_posix.c` - POSIX sockets (Linux, testing)
- `net_lwip.c` - lwIP stack (microcontrollers)
- `net_custom.c` - Bare metal implementations

### Cryptography

- **Encryption Schemes**: ONE only
- **Selection Criteria**: Smallest object code size
- **Candidates**:
  - ChaCha20-Poly1305 (modern, potentially smaller)
  - AES-128-CTR (widely supported)
  - Decision based on compiled size benchmarks

### Network

- **Port**: Must be > 1024 (no root required)
- **Default Port**: 2222 (configurable)
- **Protocol**: SSH-2.0 only (no SSH-1)

### Authentication

- **Methods**: Password authentication only (simplest)
- **Future**: Public key authentication in separate experimental version

### Features (Minimal Set)

**MUST HAVE:**
- SSH protocol handshake (SSH-2.0)
- Key exchange (single algorithm)
- Session encryption (single cipher)
- Authentication (password only)
- Session data channel
- Simple response (e.g., "Hello World" message)

**MUST NOT HAVE:**
- SSH-1 protocol support
- Multiple cipher negotiations
- Compression
- X11 forwarding
- Port forwarding
- SFTP subsystem
- Agent forwarding
- Multiple channels
- Shell/PTY spawning (not needed for microcontrollers)
- Complex terminal emulation

**Rationale:** Microcontrollers don't run bash/shell. The server simply needs to establish an encrypted SSH session and send a simple payload ("Hello World"). This proves the protocol works while minimizing code size.

## Development Approach

### Phase-Based Iterative Development

**Phase 1: Vanilla Implementation (v0-vanilla/)**
- Unoptimized, straightforward C implementation
- Focus: correctness and readability, not size
- Use standard libraries freely
- Goal: Working SSH server that passes all tests with real clients
- **DO NOT optimize until this works perfectly**

**Phase 2: Portable Implementation (v1-portable/)**
- Refactor to use network abstraction layer
- Remove platform-specific dependencies
- Maintain compatibility with real SSH clients
- Goal: Same functionality, runs on multiple platforms
- Test on Linux, prepare for microcontroller deployment

**Phase 3: Size-Optimized Versions (v2-opt1/, v3-opt2/, ...)**
Each subsequent version is an independent experiment in size reduction:

1. **v2-opt1**: Compiler optimization flags only
   - `-Os -flto -ffunction-sections -fdata-sections`
   - Link-time optimization
   - Symbol stripping

2. **v3-opt2**: Minimal crypto library
   - Replace standard crypto with TweetNaCl or custom
   - Benchmark binary size difference

3. **v4-opt3**: Manual buffer management
   - Replace malloc/free with static buffers
   - Optimize for minimal RAM usage

4. **v5-opt4**: Hand-optimized protocol state machine
   - Minimize branching
   - Reduce code paths

5. **v6-opt5**: Aggressive code golf
   - Combine all successful optimizations
   - Consider assembly for critical paths

**Critical Rule:** Each version is completely independent and compiles standalone. Can cherry-pick successful optimizations into new versions.

### Size Optimization Strategies

- Strip symbols (`strip -s`)
- Compiler flags: `-Os -ffunction-sections -fdata-sections`
- Linker flags: `-Wl,--gc-sections`
- Minimal libc usage
- Static linking evaluation
- Manual syscalls where beneficial
- Avoid standard crypto libraries if smaller alternatives exist

## Build Environment

### Nix Environment (shell.nix)

Required packages:
- gcc
- gnumake
- openssh (client for testing)
- just
- pkg-config
- Minimal crypto library (libsodium or TweetNaCl for size)

### Justfile Commands

```
# Build commands
just build <version>      # Build specific version (e.g., v0-vanilla)
just build-all           # Build all versions
just clean              # Clean all build artifacts
just clean <version>    # Clean specific version

# Testing commands
just test <version>     # Run tests for specific version
just test-all          # Test all built versions
just size-report       # Compare binary sizes across versions

# Development commands
just run <version>     # Start server on port 2222
just connect          # Connect with ssh client in another terminal
just debug <version>  # Run with gdb
just valgrind <version>  # Check for memory leaks
```

## Testing Requirements

**CRITICAL:** Testing with real SSH clients is paramount. Do not proceed to optimization until the implementation passes ALL tests.

### Test Environment

- Use `nix-shell` for isolated, reproducible testing
- Real SSH client from nixpkgs (openssh)
- Local loopback testing (127.0.0.1:2222)
- Automated test scripts

### Test Cases

**Must pass with standard `ssh` client:**

1. **Connection Test**: `ssh -p 2222 user@localhost` establishes connection
2. **Protocol Version**: Server responds with SSH-2.0 banner
3. **Key Exchange**: Completes key exchange successfully
4. **Authentication Test**: Password authentication succeeds
5. **Data Channel**: Receives "Hello World" message
6. **Disconnect Test**: Clean session termination (no crashes)

### Test Automation

```bash
# Automated test suite
just test-vanilla       # Test v0-vanilla
just test-portable      # Test v1-portable
just test-all          # Test all versions
just test-version v3-opt2  # Test specific version
```

### Success Criteria (Per Version)

- [ ] SSH client connects successfully
- [ ] User can authenticate with password
- [ ] Server sends "Hello World" response
- [ ] Connection closes cleanly
- [ ] No memory leaks (valgrind clean)
- [ ] No crashes under normal operation
- [ ] Binary size recorded and compared

**Binary Size Targets:**
- v0-vanilla: No size constraint (get it working!)
- v1-portable: < 200KB (with abstraction overhead)
- v2-opt1: < 100KB
- v3+: Progressively smaller, stretch goal < 32KB

## Security Considerations

**In Scope:**
- Basic protocol security
- Correct crypto implementation
- Memory safety (basic)

**Out of Scope (for minimal version):**
- DoS protection
- Rate limiting
- Advanced security hardening
- Privilege separation
- Chroot jails

**Warning**: This is an educational/experimental project. Not suitable for production use.

## Documentation Requirements

- README.md with build instructions
- Code comments for protocol implementation
- Size comparison table between versions
- Lessons learned document

## Timeline & Milestones

### Phase 0: Project Setup
- [ ] Nix environment (shell.nix)
- [ ] Build system (Makefile, justfile)
- [ ] Directory structure for multiple versions
- [ ] Test automation scripts

### Phase 1: Vanilla Implementation (v0-vanilla/)
- [ ] Network layer (POSIX sockets first)
- [ ] SSH protocol handshake (version exchange)
- [ ] Key exchange algorithm implementation
- [ ] Packet encryption/decryption
- [ ] Password authentication
- [ ] Session channel establishment
- [ ] Send "Hello World" response
- [ ] **MILESTONE: Works with real SSH client**

### Phase 2: Portable Implementation (v1-portable/)
- [ ] Design network abstraction layer
- [ ] Implement net_posix.c
- [ ] Refactor v0 to use abstraction
- [ ] Remove platform-specific code
- [ ] Test on Linux
- [ ] **MILESTONE: Platform-independent codebase**

### Phase 3: Iterative Optimization (v2+)
- [ ] v2-opt1: Compiler optimizations
- [ ] v3-opt2: Minimal crypto library
- [ ] v4-opt3: Static buffer management
- [ ] v5-opt4: State machine optimization
- [ ] v6-opt5: Aggressive size reduction
- [ ] **MILESTONE: Sub-32KB binary**

### Phase 4: Documentation & Analysis
- [ ] Size comparison table
- [ ] Lessons learned document
- [ ] Porting guide for new platforms
- [ ] Performance benchmarks

## Success Metrics

1. **Primary**: Binary size in bytes (smaller is better)
2. **Secondary**: Lines of code (fewer is better)
3. **Compatibility**: Works with openssh-client
4. **Simplicity**: Code readability and maintainability

## References

- RFC 4253: SSH Protocol Architecture
- RFC 4252: SSH Authentication Protocol
- RFC 4254: SSH Connection Protocol
- RFC 7748: Elliptic Curves for Security (for key exchange)
- TweetNaCl: Compact crypto library for reference

## Non-Goals

- Feature completeness (deliberately minimal)
- Performance optimization for speed (size is priority)
- Production readiness or security hardening
- Configuration files (hardcoded credentials acceptable)
- IPv6 support (unless trivial)
- Multiple simultaneous connections
- Graphical interfaces or management tools
- Full terminal emulation
- Shell execution (just "Hello World" payload)

## Project Structure

```
nano_ssh_server/
├── shell.nix           # Nix development environment
├── justfile            # Task automation
├── Makefile           # Top-level build orchestration
├── PRD.md             # This document
├── README.md          # Project overview
├── docs/              # Additional documentation
│   ├── TESTING.md
│   └── PORTING.md
├── v0-vanilla/        # Phase 1: Unoptimized working version
│   ├── Makefile
│   ├── main.c
│   ├── ssh_proto.c
│   ├── crypto.c
│   └── network.c
├── v1-portable/       # Phase 2: Platform-abstracted version
│   ├── Makefile
│   ├── main.c
│   ├── ssh_proto.c
│   ├── crypto.c
│   ├── network.h      # Abstraction interface
│   ├── net_posix.c    # POSIX implementation
│   └── net_lwip.c     # lwIP implementation (future)
├── v2-opt1/           # Phase 3: Compiler optimizations
├── v3-opt2/           # Phase 3: Minimal crypto
├── v4-opt3/           # Phase 3: Static buffers
├── v5-opt4/           # Phase 3: State machine optimization
├── v6-opt5/           # Phase 3: Aggressive optimization
└── tests/             # Shared test scripts
    ├── test_connect.sh
    ├── test_auth.sh
    └── test_session.sh
```

Each version directory is completely independent and self-contained.
