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
├── shell.nix           # Nix development environment
├── justfile            # Task automation
├── Makefile           # Top-level build orchestration
├── PRD.md             # Product Requirements Document
├── TODO.md            # Task breakdown and tracking
├── CLAUDE.md          # Development guidelines
├── README.md          # This file
├── docs/              # Implementation guides and RFCs
│   ├── RFC4253_Transport.md
│   ├── RFC4252_Authentication.md
│   ├── RFC4254_Connection.md
│   ├── IMPLEMENTATION_GUIDE.md
│   ├── CRYPTO_NOTES.md
│   ├── TESTING.md
│   └── PORTING.md
├── v0-vanilla/        # Phase 1: Working implementation (correctness first)
├── v1-portable/       # Phase 2: Platform-abstracted version
├── v2-opt1/           # Phase 3: Compiler optimizations
├── v3-opt2/           # Phase 3: Minimal crypto library
├── v4-opt3/           # Phase 3: Static buffers
├── v5-opt4/           # Phase 3: State machine optimization
├── v6-opt5/           # Phase 3: Aggressive optimization
└── tests/             # Test scripts
```

## Development Phases

### Phase 0: Project Setup ✅
- [x] Nix environment
- [x] Build system (Makefile + justfile)
- [x] Directory structure
- [x] Basic compilation test

### Phase 1: v0-vanilla (In Progress)
**Goal:** Working SSH server, correctness first

Implements:
- TCP server (POSIX sockets)
- SSH-2.0 version exchange
- Binary packet protocol
- Curve25519 key exchange
- ChaCha20-Poly1305 encryption
- Password authentication
- Session channel
- "Hello World" response

**Success:** `ssh -p 2222 user@localhost` receives "Hello World"

### Phase 2: v1-portable
**Goal:** Platform-independent code

- Network abstraction layer (net_posix.c, net_lwip.c)
- Remove platform-specific code from core
- Prepare for microcontroller deployment

### Phase 3: Optimizations (v2-v6)
**Goal:** Progressively smaller binaries

Each version is an independent optimization experiment:
- v2-opt1: Compiler flags only
- v3-opt2: Minimal crypto library
- v4-opt3: Static buffer management
- v5-opt4: State machine optimization
- v6-opt5: Aggressive size reduction

**Target:** < 32KB binary (stretch goal)

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

Uses **libsodium** (TweetNaCl-based) for minimal size:
- ChaCha20-Poly1305: AEAD cipher
- Curve25519: Key exchange
- Ed25519: Host key signatures
- SHA-256: Exchange hash

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

## Size Comparison (TODO)

| Version | Size | Description |
|---------|------|-------------|
| v0-vanilla | TBD | Working implementation |
| v1-portable | TBD | Platform abstraction |
| v2-opt1 | TBD | Compiler optimizations |
| v3-opt2 | TBD | Minimal crypto |
| v4-opt3 | TBD | Static buffers |
| v5-opt4 | TBD | State machine |
| v6-opt5 | TBD | Aggressive (goal: <32KB) |

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

🚧 **Work in Progress** 🚧

- [x] Phase 0: Project setup
- [ ] Phase 1: v0-vanilla implementation
- [ ] Phase 2: v1-portable implementation
- [ ] Phase 3: Size optimizations
- [ ] Phase 4: Documentation and polish

Current focus: Implementing Phase 1 (v0-vanilla)
