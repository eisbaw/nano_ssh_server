# Nano SSH Server

A minimal SSH-2.0 server intended for microcontrollers. Speaks enough of the
protocol to authenticate a user and emit a single message ("Hello World"), then
disconnects. Designed for size, not security.

The smallest working build is **4,095 bytes** (`v30-chacha`, fully static,
zero runtime dependencies) or 20 KB (`v23-scratch`, dynamic).

## Quick Start

```bash
nix-shell                       # enters dev environment
just build v30-chacha           # recommended/smallest: under 4 KiB, fully static
just run v30-chacha             # listens on 2222
ssh -p 2222 user@localhost      # password: password123
```

Outside Nix: `apt install gcc make just openssh-client sshpass libsodium-dev`.

## Versions

The repository keeps each optimization step as its own directory so the size
progression is reproducible. The table lists the production-ready builds, sorted
smallest first. Run `just size-report` to regenerate.

| Version      | Bytes   | Size   | Linkage                | Notes                                |
|--------------|---------|--------|------------------------|--------------------------------------|
| v30-chacha   |   4,095 | 4.0 KB | static, no libc        | Recommended/smallest: shared ChaCha20 add/XOR/rotate step |
| v29-p256     |   4,118 | 4.0 KB | static, no libc        | P-256 key exchange and host key on one modular multiplier |
| v28-chapoly  |   7,975 | 7.8 KB | static, no libc        | chacha20-poly1305 on the Ed25519 field arithmetic |
| v27-onecurve |   9,946 | 9.7 KB | static, no libc        | One Curve25519 implementation for KEX and signing |
| v26-genk     |  12,074 |  12 KB | static, no libc        | v25-pack + generated SHA/Ed25519 constants + ELF golf |
| v25-pack     |  14,576 |  14 KB | static, no libc        | v23-min + computed AES S-box + packed exchange hash |
| v23-min      |  14,800 |  14 KB | static, no libc        | from-scratch main + freestanding syscalls |
| v23-scratch  |  20,688 |  20 KB | dynamic, glibc         | Smallest dynamic: from-scratch 378-line main |
| v22-c25519   |  25,272 |  25 KB | dynamic, glibc         | c25519 ladder, libc only             |
| v17-from14   |  25,248 |  24 KB | dynamic, glibc+libsodium | Smaller on disk, but needs libsodium at runtime |
| v20-opt      |  41,288 |  40 KB | dynamic, glibc         | Compiler + linker optimizations      |
| v19-donna    |  45,720 |  44 KB | dynamic, glibc         | Curve25519-donna implementation      |
| v22-static   |  51,008 |  50 KB | static, musl           | c25519 ladder, zero runtime deps     |
| v21-static   |  54,344 |  53 KB | static, musl           | Curve25519-donna, zero runtime deps  |
| v17-static2  |  71,456 |  69 KB | static, musl           | v17-from14 sources, built static     |
| v0-vanilla   | 118,496 | 115 KB | dynamic, libsodium+SSL | Baseline reference, readable code    |

Exact byte counts vary a little with the toolchain. `v29-p256`'s 4,118,
`v28-chapoly`'s 7,975, `v27-onecurve`'s 9,946 and `v26-genk`'s 12,074 were
measured with the same gcc 13.3 / binutils 2.42, so the like-for-like
savings are 3,857 bytes (−48.4%), 1,971 bytes (−19.8%) and 2,128 bytes
(−17.6%). (The freestanding builds are `-nostdlib`,
so musl-gcc and plain gcc emit identical bytes.) `v25-pack` rebuilt with that
toolchain is 13,928 bytes. See each version's `optimization_log.txt` for the
step-by-step breakdown.

`v30-chacha` retains `v29-p256`'s curve, hash, protocol and build flags. It
packs the eight ChaCha20 quarter-round operand sets into 16 bytes and uses
one add/XOR/rotate body with rotation counts 16, 12, 8 and 7. Swapping the
packed operand bytes between steps exchanges a/c and b/d. An increment-to-zero
quarter-round counter and unsigned word indexing save another ten bytes.
The result is **4,095 bytes versus 4,118** with the same gcc 13.3 / binutils
2.42: **23 bytes (0.56%) smaller**, with all 20 cipher rounds retained and
no compression. The cipher core does more loop/operand-decoding work; the
P-256 handshake still dominates connection time. `just test-chacha` checks
both cores against a known vector and an independent Python reference,
including unaligned buffers and partial-word/block boundaries. See
`v30-chacha/optimization_log.txt` for the measurements.

`v29-p256` moves the key exchange and the host key from Curve25519/Ed25519
to NIST P-256 (ecdh-sha2-nistp256 + ecdsa-sha2-nistp256). The point is not
the curve but what it removes: `v28-chapoly` still needed two curves (the
same group in Montgomery and Edwards coordinates, with a mapping and a
square root between them), a dedicated 2^255−19 field with its own carry
folding, and two hashes, because the Ed25519 challenge is SHA-512 while the
rest of the handshake is SHA-256. On P-256 the key exchange and the
signature run in one coordinate system, and ECDSA hashes with SHA-256, so the
SHA-512 core, the Curve25519 field code, the square root and the curve
mapping all go. What is left of the public-key crypto is one generic
shift-and-add modular multiplier (`fp.c`, big-endian, any 256-bit modulus,
so every wire value is used in place) and one short-Weierstrass
double-and-add step written as a 41-instruction program over a flat file of
field elements; the same multiplier does the group-order arithmetic of
ECDSA and, as in `v28`, Poly1305. The SHA-256 constants are generated at
startup in double precision (the cube root as the fixed point of
x = sqrt(q/x)), every hash is taken in one shot over an assembled buffer
and padded in place, the KEX reply is built in place, a failed read or a
malformed packet abandons the connection by re-entering the accept loop on
a private stack (so no I/O call returns a result), and the linker script
writes the 0x68-byte ELF header itself with the program header packed into
its tail and the fields the kernel never reads holding data the program
uses (the ECDSA program, the version string, the listening address, the
channel window). The cost is speed: every field multiplication is
bit-serial, so a handshake takes ~0.7 s on a desktop against ~0.07 s for
`v28` (and a proportionally long time on a microcontroller). See
`v29-p256/optimization_log.txt` for the step-by-step breakdown.

`v28-chapoly` changes the packet cipher from aes128-ctr + hmac-sha2-256 to
chacha20-poly1305@openssh.com. The point is not ChaCha20 itself but Poly1305:
its tag is `h = (h + c) · r mod 2^130−5`, which is exactly the generic
modular arithmetic `fprime.c` already links for the Ed25519 scalar field, so
the AEAD costs a ChaCha20 core plus three bytes of modulus, and the AES S-box,
key schedule, rounds, CTR and HMAC wrapper all go. The same "run it on what
is already linked" argument then moves the SHA constant generator onto
`fprime_mul` (under a power-of-two modulus that never reduces), the field
normalization onto `fprime.c`'s conditional subtract, and the field inverse
and square root onto one square-and-multiply loop; the remaining steps are
protocol golf (wire words as one `bswap` + unaligned access, the userauth
request matched as a whole, packets received in place, a global socket).
The cost is a ~0.5 s startup (the constants are generated with bit-serial
multiplies) and a slower, bit-serial Poly1305.

`v27-onecurve` starts from the observation that `v26-genk` carried two full
Curve25519 implementations: the c25519 Montgomery ladder for the key exchange
and the twisted-Edwards group law for the host-key signature. They are the
same group in different coordinates, so the ladder is redundant — map the
peer's `u` coordinate onto the Edwards curve, reuse the scalar multiplier, and
map back. Once that mapping exists a square root is linked anyway, which lets
the field inverse and the square root share one exponentiation chain, and the
same "there is only one of these" argument then removes the dedicated doubling
formula (the addition law is complete on this curve), the SHA-512 key
expansion (a random scalar is indistinguishable to a verifier), and the
streaming SHA-512 path (one 96-byte input remains).

The smallest builds share one from-scratch `main` (single hardcoded algorithm
path, no debug output, fixed-size buffers, no malloc):
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
| Key exchange   | Curve25519; NIST P-256 (`v29-p256`, `v30-chacha`) |
| Host key       | Ed25519; ECDSA P-256 (`v29-p256`, `v30-chacha`) |
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
- AEAD on the scalar field (`v28-chapoly`): chacha20-poly1305@openssh.com
  replaces aes128-ctr + hmac-sha2-256, with Poly1305 running on the generic
  `fprime` modular arithmetic already linked for Ed25519 (a second modulus,
  no dedicated 130-bit code); the SHA constant generator, field
  normalization, scalar reduction and field inverse are likewise re-expressed
  on routines already present, and the wire code uses `bswap` + unaligned
  access, whole-message userauth matching and in-place packet receive
- One curve, one multiplier, one hash (`v29-p256`): NIST P-256 for both the
  key exchange and the signature, so the whole handshake runs on one generic
  shift-and-add modular multiplier (three moduli: the field prime, the group
  order, Poly1305's 2^130−5) and needs no SHA-512; the double-and-add step,
  the affine conversion and ECDSA are 2-byte-per-instruction programs over a
  flat file of field elements; SHA-256 constants from a double-precision
  recurrence; one-shot hashing over assembled buffers; the KEX reply built in
  place; `-fno-pie` (the code had been position-independent for nothing);
  the ELF header written by the linker script with the program header
  packed into its tail and program data in the fields the kernel never
  reads (`objcopy -O binary` emits the file: no post-processing script);
  errors abandon the connection by re-entering the accept loop on a
  private stack instead of returning through every caller
- One curve implementation (`v27-onecurve`): X25519 runs on the Edwards group
  law linked for Ed25519, so the Montgomery ladder disappears; the field
  inverse is `exp2523(x)^8 · x^3` so one exponentiation chain serves both the
  inverse and the square root; doubling uses the (complete) addition law, and
  the addition law itself is an 18-instruction program over a flat file of
  field elements at 3 bytes per operation instead of 18 straight-line calls
- Generated constants (`v26-genk`): the SHA-512 K table and initial state are
  computed at startup from integer cube/square roots of the first 80 primes
  (their FIPS 180-4 definition), and the SHA-256 K table and initial state are
  read off as the top 32 bits of the same values — ~1 KB of constant tables
  becomes ~460 bytes of generator code. The Ed25519 base/neutral points are
  likewise built at startup (only the base x coordinate remains stored).
- ELF golf (`v26-genk`): `-Oz`, compile+link in one gcc invocation so `-Oz`
  reaches the LTO codegen, `-fcf-protection=none`, `-Wl,-n`, a minimal linker
  script (single RWX `PT_LOAD` covering headers + packed sections), and
  `sstrip.py` to drop the section header table. Metadata removal only — the
  loaded image is unchanged, unlike UPX-style compression which is banned.

## Testing

```bash
just test-all-sshpass           # runs each production version end-to-end
just test v0-vanilla            # unit + connection tests for one version
just test-chacha                # independent cipher vectors and boundary tests
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
├── v25-pack/          v23-min + computed S-box + packed hash
├── v26-genk/          v25-pack + generated constants + ELF golf
├── v27-onecurve/      one Curve25519 implementation for KEX and signing
├── v28-chapoly/       chacha20-poly1305 on the Ed25519 field arithmetic
├── v29-p256/          P-256 on one modular multiplier
├── v30-chacha/        recommended/smallest: shared ChaCha20 round step
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

Sixteen production versions are validated end-to-end against a real OpenSSH
client on every commit: `v0-vanilla`, `v17-from14`, `v17-static2`,
`v19-donna`, `v20-opt`, `v21-static`, `v22-c25519`, `v22-static`,
`v23-scratch`, `v23-min`, `v25-pack`, `v26-genk`, `v27-onecurve`,
`v28-chapoly`, `v29-p256`, `v30-chacha`.
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
Two size optimizations also weaken the crypto in ways that matter only if you
ignore that advice: the AES S-box is computed with data-dependent operations
(`v25-pack`), and `v27-onecurve`/`v28-chapoly` accept an X25519 peer point
without checking that it is on the curve — a twist point yields a wrong shared
secret and the handshake fails at the first MAC check, rather than being
rejected outright. `v28-chapoly` also offers only chacha20-poly1305@openssh.com
with empty MAC name-lists (valid for an AEAD cipher; OpenSSH accepts it, a
stricter client may not) and accepts the userauth request only in the exact
form OpenSSH sends it.
`v29-p256` has its own set of trade-offs, and one property that is *stronger*
than its predecessors: an independent review of the version found that the
packet-length guard shared by every version since `v23-scratch` (`pktlen + 20 >
max` evaluated in 32 bits) wraps for lengths near 2^32 and lets an
unauthenticated client overrun a stack buffer; `v29-p256` compares in 64 bits
(the older versions are kept as they were, as documented size steps, and are
not to be exposed to a network either). What it does not do:
- The peer's P-256 point is used without a range or on-curve check. An
  invalid point yields a wrong shared secret and the handshake fails at the
  first MAC; the arithmetic has no data-dependent memory access or loop
  bound, so nothing worse happens, and the scalar it meets is ephemeral.
- Scalars (host key, ephemeral key, ECDSA nonce) are drawn uniformly over
  [2^224, n) — one part in 2^32 short of uniform over [1, n), with no fixed
  bits. The review caught an earlier draft that clamped the top two bits of
  every nonce, a textbook lattice-attack weakness; the ladder now takes
  `k + n` instead, which is why the lower bound exists.
- The modular reduction, the ladder's conditional move and the Poly1305 tag
  check are branch-free, but `fp_mul` selects its addend by a `cmov` on a
  *pointer*, so the address of the following load depends on a secret bit
  (a cache-timing channel), and every field operation is bit-serial.
- No `exit-status` is sent (the client reports 255 after printing "Hello
  World"), the client's `first_kex_packet_follows` guess and any `IGNORE` /
  `DEBUG` messages during the handshake are not handled, and the two
  post-NEWKEYS sequence numbers are constants — none of which OpenSSH
  exercises.

`v30-chacha` inherits all of `v29-p256`'s security and interoperability
limitations; its changes only restructure the ChaCha20 computation.

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
