# Product Requirements Document: Nano SSH Server

## Project Overview

Build the world's smallest SSH server that remains compatible with standard Linux SSH clients. The primary goal is minimal binary size while maintaining core SSH protocol functionality.

## Core Principles

1. **Minimalism First**: Fewer features are better
2. **Size Optimization**: Every byte of code matters
3. **Client Compatibility**: Must work with standard Linux ssh clients
4. **Experimental Approach**: Multiple versions for testing different approaches

## Technical Requirements

### Language & Tooling

- **Language**: Plain C
- **Compiler**: GCC with size optimization flags
- **Build System**: GNU Make
- **Package Management**: Nix (shell.nix)
- **Task Runner**: justfile

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
- Shell session spawning
- Basic terminal I/O

**MUST NOT HAVE:**
- SSH-1 protocol support
- Multiple cipher negotiations
- Compression
- X11 forwarding
- Port forwarding
- SFTP subsystem
- Agent forwarding
- Multiple channels

## Development Approach

### Experimental Versions

Create multiple experimental implementations:

1. **Version A**: Baseline implementation
2. **Version B**: Alternative crypto library
3. **Version C**: Different buffer management
4. **Version D**: Optimized protocol state machine

Each version stored in separate directory or branch for easy comparison and combination of best parts.

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
just build          # Build current version
just build-all      # Build all experimental versions
just size-report    # Compare binary sizes
just test           # Run local tests
just run            # Start server on port 2222
just connect        # Test connection with ssh client
just clean          # Clean build artifacts
```

## Testing Requirements

### Test Environment

- Use `nix-shell` for isolated testing
- SSH client from nixpkgs
- Local loopback testing (no network required)

### Test Cases

1. **Connection Test**: `ssh -p 2222 user@localhost` connects successfully
2. **Authentication Test**: Password authentication works
3. **Shell Test**: Can execute basic commands
4. **I/O Test**: Terminal input/output functions correctly
5. **Disconnect Test**: Clean session termination

### Success Criteria

- [ ] SSH client successfully connects
- [ ] User can authenticate
- [ ] Interactive shell session works
- [ ] Basic commands execute (ls, pwd, echo)
- [ ] Binary size < 100KB (stretch goal: < 50KB)

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

### Phase 1: Setup
- [ ] Nix environment configuration
- [ ] Build system setup
- [ ] Project structure

### Phase 2: Prototype
- [ ] Basic TCP server
- [ ] SSH protocol handshake
- [ ] Key exchange implementation

### Phase 3: Core Features
- [ ] Encryption/decryption
- [ ] Authentication
- [ ] Shell spawning

### Phase 4: Optimization
- [ ] Multiple experimental versions
- [ ] Size optimization
- [ ] Best practices combination

### Phase 5: Testing
- [ ] Real client testing
- [ ] Bug fixes
- [ ] Documentation

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

- Feature completeness
- Performance optimization (speed)
- Production readiness
- Multi-platform support (Linux only)
- IPv6 support (unless trivial)
- Configuration files (hardcoded is fine)
