# Nano SSH Server

A minimal SSH-2.0 server intended for microcontrollers. Speaks enough of the
protocol to authenticate a user and emit a single message ("Hello World"), then
disconnects. Designed for size, not security.

The smallest working build is **14,576 bytes** (`v25-pack`, fully static, zero
runtime dependencies) or 20 KB (`v23-scratch`, dynamic).

## Quick Start

```bash
nix-shell                       # enters dev environment
just build v25-pack             # recommended/smallest: 14.6 KB, fully static
just run v25-pack               # listens on 2222
ssh -p 2222 user@localhost      # password: password123
```

Outside Nix: `apt install gcc make just openssh-client sshpass libsodium-dev`.

## Versions

The repository keeps each optimization step as its own directory so the size
progression is reproducible. The table lists the production-ready builds, sorted
smallest first. Run `just size-report` to regenerate.

| Version     | Bytes   | Size   | Linkage                | Notes                                |
|-------------|---------|--------|------------------------|--------------------------------------|
| v25-pack    |  14,576 |  14 KB | static, no libc        | Recommended/smallest: v23-min + computed AES S-box + packed exchange hash |
| v23-min     |  14,800 |  14 KB | static, no libc        | from-scratch main + freestanding syscalls |
| v23-scratch |  20,688 |  20 KB | dynamic, glibc         | Smallest dynamic: from-scratch 378-line main |
| v22-c25519  |  25,272 |  25 KB | dynamic, glibc         | c25519 ladder, libc only             |
| v17-from14  |  25,248 |  24 KB | dynamic, glibc+libsodium | Smaller on disk, but needs libsodium at runtime |
| v20-opt     |  41,288 |  40 KB | dynamic, glibc         | Compiler + linker optimizations      |
| v19-donna   |  45,720 |  44 KB | dynamic, glibc         | Curve25519-donna implementation      |
| v22-static  |  51,008 |  50 KB | static, musl           | c25519 ladder, zero runtime deps     |
| v21-static  |  54,344 |  53 KB | static, musl           | Curve25519-donna, zero runtime deps  |
| v17-static2 |  71,456 |  69 KB | static, musl           | v17-from14 sources, built static     |
| v0-vanilla  | 118,496 | 115 KB | dynamic, libsodium+SSL | Baseline reference, readable code    |

The two smallest builds share one from-scratch `main` (378 lines, single
hardcoded algorithm path, no debug output, fixed-size buffers, no malloc):
- `v23-scratch` links glibc dynamically (20 KB).
- `v23-min` drops libc entirely — direct x86-64 Linux syscalls, a custom
  `_start`, and hand-rolled mem/str routines (`nolibc.h`/`nolibc.c`) — and links
  `-nostdlib -ffreestanding -static` for a 14 KB binary with zero runtime
  dependencies. In static builds libc dominates the binary, so removing it is the
  single biggest win (musl static of the same code is 51 KB). `-Wl,-z,noseparate-code`
  removes another ~4 KB of segment-alignment padding.

`v22-c25519`/`v22-static` replace `v19-donna`/`v21-static`'s 20 KB
`curve25519-donna-c64` X25519 with the small public-domain c25519 Montgomery
ladder (Daniel Beer), which reuses the f25519 field arithmetic already linked
for Ed25519. Note `v17-from14` is smaller on disk only because it dynamically
links libsodium — it is not self-contained.

The glibc-static "comparison baseline" (formerly v12-static, ~718 KB) was removed
because it does not build on NixOS due to a glibc ABI mismatch; see git history.
Executable compression (UPX) is deliberately not used: it only shrinks the
on-disk file while leaving the runtime RAM footprint unchanged.

## Protocol

| Feature        | Implementation                          |
|----------------|-----------------------------------------|
| Protocol       | SSH-2.0                                 |
| Key exchange   | Curve25519                              |
| Host key       | Ed25519                                 |
| Cipher         | AES-128-CTR (vanilla) or ChaCha20-Poly1305 |
| MAC            | HMAC-SHA256                             |
| Authentication | Password (hardcoded `user`/`password123`)|
| Channels       | Single session, no PTY                  |
| Unsupported    | Compression, pubkey auth, SFTP, X11, multiplexing |

## Optimization Techniques

- Compiler: `-Os -flto -ffunction-sections -fdata-sections -fno-unwind-tables
  -fno-asynchronous-unwind-tables -fno-stack-protector -fvisibility=hidden`
- Linker: `--gc-sections --strip-all --as-needed --hash-style=gnu --build-id=none`
- Protocol minimization: single cipher suite, no algorithm negotiation
- Custom crypto: in-tree public-domain c25519 (Montgomery-ladder X25519 + Ed25519)
- libc: musl instead of glibc for static builds; or drop libc entirely
  (`-nostdlib -ffreestanding`, direct syscalls) for the smallest static build
- `-Wl,-z,noseparate-code` to remove segment-alignment padding (~4 KB on small binaries)
- A tight from-scratch `main` (no debug, no malloc, fixed buffers)
- Code-size packing: compute the AES S-box in GF(2^8) instead of a 256-byte
  lookup table (trades table for ~70 bytes of code), and factor repeated
  exchange-hash field hashing into one helper (`v25-pack`). Note the computed
  S-box is not constant-time (data-dependent); a non-issue for this educational
  server but do not carry the pattern into production crypto.

## Testing

```bash
just test-all-sshpass           # runs each production version end-to-end
just test v0-vanilla            # unit + connection tests for one version
just size-report                # table of all built binary sizes
just valgrind v0-vanilla        # memory leak check
```

End-to-end testing uses `sshpass` to drive a real OpenSSH client against the
server on port 2222 and asserts that "Hello World" appears in the output. A
version that does not produce that string is considered broken.

## Layout

```
nano_ssh_server/
├── v0-vanilla/        reference implementation
├── v17-from14/        custom-crypto dynamic (still links libsodium)
├── v17-static2/       custom-crypto musl static
├── v19-donna/         Curve25519-donna
├── v20-opt/           compiler-optimized dynamic
├── v21-static/        Curve25519-donna musl static
├── v22-c25519/        c25519 ladder, dynamic, libc-only
├── v22-static/        c25519 ladder, musl static
├── v23-scratch/       smallest dynamic: from-scratch 378-line main
├── v23-min/           scratch main + freestanding, no libc
├── v25-pack/          recommended/smallest: v23-min + computed S-box + packed hash
├── v23-*/             other size experiments (debug-strip, chacha, nolibc, etc.)
├── v{8,9,11..15}-*/   intermediate optimization steps (all working)
├── docs/              RFC summaries and implementation notes
├── tests/             test scripts driven by justfile recipes
├── shell.nix          Nix dev environment (gcc, musl-gcc, sshpass, valgrind)
├── justfile           task automation entrypoint
├── PRD.md             product requirements
├── CLAUDE.md          development workflow
└── TODO.md            task tracking
```

## Status

Eleven production versions are validated end-to-end against a real OpenSSH client
on every commit: `v0-vanilla`, `v17-from14`, `v17-static2`, `v19-donna`,
`v20-opt`, `v21-static`, `v22-c25519`, `v22-static`, `v23-scratch`, `v23-min`,
`v25-pack`.
The intermediate `v8`–`v15` and the other `v23-*` size experiments
(debug-strip, chacha20-poly1305, nolibc, musl-static debug-strip, sstrip) also
build and pass; they document the step-by-step size progression.

Versions whose optimization broke the server were removed (recoverable from git
history):
- `v2-opt1`–`v7-opt6`: aggressive `-Os -flto` builds that hung at the
  `SSH_MSG_NEWKEYS` exchange.
- `v10-opt9`, `v13-opt11`: custom linker scripts that omitted required ELF
  sections, producing binaries that segfaulted before `main`.
- `v12-static`, `v14-static`: glibc-static builds that do not link on NixOS.
- `v23-clang`, `v23-lld-icf`: needed clang/lld outside the Nix dev shell;
  `v23-clang` was also larger than gcc `-Os` (clang `-Oz` miscompiled the inline
  crypto under LTO).

The `vbash-ssh-server/` directory is an unrelated shell-script proof of
concept.

## Security

This is an educational project. The server uses hardcoded credentials, runs
single-threaded, has no DoS protection, and is not hardened. Do not deploy.
Use it to study the SSH protocol, experiment with size optimization, or
prototype on a microcontroller.

## References

- RFC 4253 — SSH Transport Layer Protocol
- RFC 4252 — SSH Authentication Protocol
- RFC 4254 — SSH Connection Protocol
- RFC 7748 — Elliptic Curves for Security (Curve25519)
- libsodium — modern cryptography library
- TweetNaCl — compact cryptography library

## License

MIT License - See [LICENSE](LICENSE) file
