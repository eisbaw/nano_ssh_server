# Nano SSH Server

A minimal SSH-2.0 server intended for microcontrollers. Speaks enough of the
protocol to authenticate a user and emit a single message ("Hello World"), then
disconnects. Designed for size, not security.

The smallest working build is 24 KB (dynamic) or 53 KB (musl, fully static).

## Quick Start

```bash
nix-shell                       # enters dev environment
just build v21-static           # recommended: musl static, 53 KB
just run v21-static             # listens on 2222
ssh -p 2222 user@localhost      # password: password123
```

Outside Nix: `apt install gcc make just openssh-client sshpass libsodium-dev`.

## Versions

The repository keeps each optimization step as its own directory so the size
progression is reproducible. The table lists the production-ready builds, sorted
smallest first. Run `just size-report` to regenerate.

| Version     | Bytes   | Size   | Linkage                | Notes                                |
|-------------|---------|--------|------------------------|--------------------------------------|
| v17-from14  |  25,248 |  24 KB | dynamic, glibc         | Smallest build; needs libc at runtime|
| v20-opt     |  41,288 |  40 KB | dynamic, glibc         | Compiler + linker optimizations      |
| v19-donna   |  45,720 |  44 KB | dynamic, glibc         | Curve25519-donna implementation      |
| v21-static  |  54,344 |  53 KB | static, musl           | Recommended: portable, zero runtime deps |
| v17-static2 |  71,456 |  69 KB | static, musl           | v17-from14 sources, built static     |
| v0-vanilla  | 118,496 | 115 KB | dynamic, libsodium+SSL | Baseline reference, readable code    |

A musl-static build is roughly an order of magnitude smaller than the same code
linked against glibc-static. The glibc-static "comparison baseline" (formerly
v12-static, ~718 KB) was removed because it does not build on NixOS due to a
glibc ABI mismatch; see git history.

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
- Custom crypto: replace libsodium with in-tree Curve25519-donna + Ed25519
- libc: musl instead of glibc for static builds

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
├── v17-from14/        custom-crypto dynamic
├── v17-static2/       custom-crypto musl static
├── v19-donna/         Curve25519-donna
├── v20-opt/           compiler-optimized dynamic
├── v21-static/        recommended: musl static
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

Six production versions are validated end-to-end against a real OpenSSH client
on every commit: `v0-vanilla`, `v17-from14`, `v17-static2`, `v19-donna`,
`v20-opt`, `v21-static`. The intermediate `v8`–`v15` directories also build and
pass; they document the step-by-step size progression.

Versions whose optimization broke the server were removed (recoverable from git
history):
- `v2-opt1`–`v7-opt6`: aggressive `-Os -flto` builds that hung at the
  `SSH_MSG_NEWKEYS` exchange.
- `v10-opt9`, `v13-opt11`: custom linker scripts that omitted required ELF
  sections, producing binaries that segfaulted before `main`.
- `v12-static`, `v14-static`: glibc-static builds that do not link on NixOS.

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
