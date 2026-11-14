# Nano SSH Server

A minimal SSH-2.0 server intended for microcontrollers. Speaks enough of the
protocol to authenticate a user and emit a single message ("Hello World"), then
disconnects. Designed for size, not security.

The smallest working build is **2,393 bytes** (`v32-asm`, fully static,
zero runtime dependencies, hand-written x86-64 assembly); the smallest in C
is 3,426 bytes (`v31-trim`), the smallest dynamic one 20 KB (`v23-scratch`).
The one to port is `v33-portable`: the same server with its core in strict
ISO C99 behind a three-function platform interface, 4,124 bytes on x86-64
and tested on big-endian and 32-bit processors.

## Quick Start

```bash
nix-shell                       # enters dev environment
just build v32-asm              # smallest: 2,393 bytes, fully static
just run v32-asm                # listens on 2222
ssh -p 2222 user@localhost      # password: password123
```

For another processor or operating system, start from `v33-portable`:
`cc -O2 nanossh.c net_posix.c` is the whole server on anything with
sockets, and `nanossh.h` says what a platform without them has to provide.

Outside Nix: `apt install gcc make just openssh-client sshpass libsodium-dev`.

## Versions

The repository keeps each optimization step as its own directory so the size
progression is reproducible. The table lists the production-ready builds, sorted
smallest first. Run `just size-report` to regenerate.

| Version      | Bytes   | Size   | Linkage                | Notes                                |
|--------------|---------|--------|------------------------|--------------------------------------|
| v32-asm      |   2,393 | 2.3 KB | static, no libc        | Smallest: v31-trim's server in hand-written x86-64 assembly |
| v31-trim     |   3,426 | 3.3 KB | static, no libc        | Smallest in C: shorter channel phase, string-instruction copies, data in place |
| v30-chacha   |   4,095 | 4.0 KB | static, no libc        | Shared ChaCha20 add/XOR/rotate step |
| v29-p256     |   4,118 | 4.0 KB | static, no libc        | P-256 key exchange and host key on one modular multiplier |
| v33-portable |   4,124 | 4.0 KB | static, no libc        | Portable: the same server with an ISO C99 core, no assembly or extensions in it |
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
step-by-step breakdown. In `nix-shell` the compiler wrapper adds hardening
flags; the one that the freestanding C versions do not override,
`-fzero-call-used-regs=used-gpr`, makes `v29-p256`, `v30-chacha` and
`v31-trim` 4,368, 4,345 and 3,618 bytes there (for `v31-trim` that flag
was measured to be the whole difference). `v32-asm` has no compiler in its
build and is 2,393 bytes with either toolchain, byte for byte, and
`v33-portable` overrides the flag: 4,124 bytes with either, byte for byte
(so in `nix-shell` it is smaller than `v30-chacha` and `v29-p256`).

`v33-portable` is the server of `v31-trim` and `v32-asm` again - same key
exchange, host key, cipher, hash, ladder and interpreter - with the
opposite goal: a core (`nanossh.c`, about 900 lines) that is strict ISO
C99 with no assembly, no compiler extensions, no floating point and no
assumptions about byte order, alignment or word size, so that it runs
unchanged on a microcontroller. A platform provides three functions -
read, write, random bytes - and calls `nanossh_init()` once and
`nanossh_serve()` per connection; `net_posix.c` does that for anything
with sockets, `net_linux_x86_64.c` without a C library for the measured
binary. It costs 698 bytes against `v31-trim` (**4,124 versus 3,426**):
copies, carries and word accesses that C cannot express as compactly, and
SHA-256 constants extracted with the field arithmetic instead of the FPU.
The scalar's bits only ever become masks here, where `v31-trim` and
`v32-asm` rely on conditional moves (42 bytes, measured), and the
userauth comparison is constant-time and checks the length. The same unit
tests as `v32-asm` pass natively, under AddressSanitizer and UBSan, with
Clang, and cross-compiled for big-endian s390x and ppc64 and 32-bit ARMv7
and i686 under qemu, where the server also completes real OpenSSH logins.
The core alone is 3.7 KB of Thumb-2 code and constants and uses about
10 KB of RAM. A login takes as long as with `v31-trim` (0.55 s here) -
which on a slow microcontroller means minutes. See
`v33-portable/optimization_log.txt`.

`v32-asm` is `v31-trim`'s server - the same key exchange, host key, cipher
and hash, the same field-operation interpreter running the same ladder
program - written directly in x86-64 assembly: **2,393 bytes versus 3,426**
(1,033 bytes, 30.2%; against the 3,618 that `v31-trim` is in `nix-shell`,
33.9%). Nothing is compressed and no algorithm changed; the bytes come
from what a compiler cannot do with this program. One base register
reaches the field elements and another the packet buffer and connection
state with one-byte displacements, where GCC loaded a 32-bit address each
time. Registers follow the data instead of the ABI: a pointer left behind
by one `rep movsb` is where the next copy starts, and a register that
already points into the image's 64 KiB is re-aimed by its low 16 bits. A
256-bit add or subtract is a 14-byte `adc`/`sbb` loop (C has no carry
flag), a modular add or subtract three such passes with one fix-up test
for both, and the three bit counters of ladder, multiplier and inversion
count up to 256 in byte registers that are therefore zero again when next
needed. The SHA-256 constants come from the x87 (`fdivr`/`fsqrt`/`fscale`
at two bytes each), the three moduli are generated from 16 stored bytes
instead of 96, the message schedule is stored downwards so the sliding
window of working variables overwrites each word right after its round,
and Poly1305's final addition and constant-time tag compare share one
loop. One sequence number serves both directions and also says whether
the keys are in use; one routine produces the length keystream, the
Poly1305 key and the payload keystream in a single ChaCha20 pass. The ELF
header's unread fields hold two interpreter programs, the dispatch table,
the channel confirmation, the listening address and the version string.
A few more protocol checks went (see Security). `just test-asm` runs the
routines - assembled from the same file with a command loop in place of
the socket - against Python references, `just test-probes` throws
malformed input at one server process and requires a real login after
each probe, and valgrind reports no errors. A login takes as long as with
`v31-trim` (0.57 s against 0.56 s here). See
`v32-asm/optimization_log.txt`.

`v31-trim` keeps `v30-chacha`'s algorithms (ecdh-sha2-nistp256,
ecdsa-sha2-nistp256, chacha20-poly1305@openssh.com, all 20 ChaCha20 rounds)
and takes **669 bytes (16.3%)** off it in 51 measured steps: **3,426 bytes
versus 4,095** with the same gcc 13.3 / binutils 2.42. The largest single
step is the channel phase: "Hello World" and `CHANNEL_CLOSE` go out right
after the open confirmation (RFC 4254 lets a channel close without EOF)
instead of after a loop answering the client's pty/shell/exec requests.
The rest is golf on what was already there: copies, the SHA-256 message
load, Poly1305's byte reversal and the mpint encoder as x86 string
instructions (`rep movsb` returning the end pointer, `repe scasb`,
`lodsl`/`bswap`/`stosl`); Poly1305's block step as a two-instruction
program for the existing P-256 interpreter; the multiplier's and the
ladder's bit tests as one `bt`; the KEX reply's constant head overlaid on
the last 45 bytes of the image (the linker script puts the reply buffer
there, so the rest of it is bss); the KEXINIT name-lists stored in wire
form; and compiler flags from a sweep that only kept a flag if the binary
still served a real OpenSSH login (it caught `-fcall-saved-rdi`, which
miscompiles a tail call). `just test-crypto` checks the P-256 versions'
SHA-256, ChaCha20, Poly1305, field arithmetic, scalar multiplication and
ECDSA against independent Python references. See
`v31-trim/optimization_log.txt` for every step and its size.

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
| Key exchange   | Curve25519; NIST P-256 (`v29-p256` and later) |
| Host key       | Ed25519; ECDSA P-256 (`v29-p256` and later) |
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
- String instructions and data in place (`v31-trim`): copies as inline
  `rep movsb` that hand back the end pointer (so consecutive copies chain),
  `repe scasb` to strip an mpint's leading zeros, `lodsl`/`bswap`/`stosl`
  for SHA-256's big-endian words and a `lodsb`/`std`/`stosb`/`cld` loop for
  Poly1305's byte reversal; the KEX reply's constant head overlaid on the
  last bytes of the image; the channel phase cut to open confirmation,
  data and close; compiler flags from a sweep that keeps a flag only if the
  binary still serves a real login
- Hand-written assembly (`v32-asm`): base registers with one-byte
  displacements for the element file and the packet buffer; carry-flag
  arithmetic (`adc`/`sbb` loops, `rcl`/`rcr` to park a carry); byte
  counters that wrap to zero; register values carried between routines
  instead of reloaded (`mov cl, n` where `rcx` is known to be zero, 16-bit
  re-aiming of pointers); x87 for the SHA-256 constants; moduli generated
  at startup; one keystream pass per packet direction; data in every
  unread ELF header field; a unit-test build of the same source
  (`-DTEST`) so the golfed routines stay checked against references
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
just test-crypto                # P-256 versions' crypto core vs Python references
just test-asm                   # v32-asm's routines vs Python references
just test-portable              # v33-portable's core vs the same references, also under ASan + UBSan
just test-portable-cross        # ... and on big-endian and 32-bit targets under qemu (large download)
just test-probes v32-asm        # malformed input against one server process
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
├── v30-chacha/        shared ChaCha20 round step
├── v31-trim/          smallest in C: v30 trimmed by 669 bytes
├── v32-asm/           smallest: v31-trim's server in x86-64 assembly
├── v33-portable/      the same server with a portable ISO C99 core
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

Nineteen production versions are validated end-to-end against a real
OpenSSH client on every commit: `v0-vanilla`, `v17-from14`, `v17-static2`,
`v19-donna`, `v20-opt`, `v21-static`, `v22-c25519`, `v22-static`,
`v23-scratch`, `v23-min`, `v25-pack`, `v26-genk`, `v27-onecurve`,
`v28-chapoly`, `v29-p256`, `v30-chacha`, `v31-trim`, `v32-asm`,
`v33-portable`.
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

`v31-trim` inherits them too and is stricter about what it accepts: it
sends "Hello World" and closes the channel without answering the client's
channel requests (they arrive after its close), understands only a
`session` channel open, requires the version line to end in CR LF and to
fit in 252 bytes, takes the client's first packet as its KEXINIT without
checking the type and the next as KEX_ECDH_INIT by its length alone (a
wrong one fails the client's signature check). Its crypto is
unchanged; the Poly1305 check still compares all 16 bytes without an
early exit.

`v32-asm` inherits all of that and checks less still: the client's
NEWKEYS, SERVICE_REQUEST and CHANNEL_OPEN are taken by their position in
the exchange, not by their type byte, and KEX_ECDH_INIT not even by its
length (a client that sends something else ends up with a transcript,
keys or a channel that do not match, and gives up); the userauth request
is matched against the 55 expected bytes without comparing the payload
length, so trailing bytes are accepted (the bytes compared are always the
client's own or this connection's KEX reply, never another connection's);
padding is zero bytes rather than random ones; the KEXINIT cookie is 16
bytes of the ephemeral public key rather than separate random bytes; and
what the client sends after the channel is closed is read and dropped
undecrypted. Its field inversion skips the multiplication for zero bits
of the exponent, which is the public modulus minus two; everything that
handles a secret is as (non-)constant-time as in `v29-p256`. The length
guard, the tag check before decryption and the constant-time tag compare
are covered by `just test-asm` and `just test-probes`.

`v33-portable` follows `v32-asm`'s protocol - packets taken by position,
zero padding, the cookie from the ephemeral key, a fresh host key at every
start, the client's last bytes dropped undecrypted - with three
differences: the userauth request is compared in constant time and by
length too; the scalar and the multiplier's bits select by mask, not by
conditional move or branch; and the version line may be 255 bytes. The
client's public point is still used as it comes (not checked to be on the
curve; the secret it meets is used for that one connection only), the
timing of the mpint encoding still depends on leading zero bytes, and
nothing is wiped when a connection ends. Its core is covered by
`just test-portable` (also under ASan and UBSan) and both builds by
`just test-probes`.

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
