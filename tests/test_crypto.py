#!/usr/bin/env python3
"""Differential test of the P-256 versions' crypto core against independent
Python references.

For each version, sha256.h, chapoly.h, fp.c and p256.c are built into a
shared library and compared with: hashlib SHA-256 (every length 0..199,
all padding boundaries, and random lengths); a scalar ChaCha20; Poly1305
from RFC 8439 (its test vector, random and extreme keys/messages, and - from
v31-trim on - the verify-as-you-write result for equal and one-bit-off
tags); big-integer arithmetic for fp_add/fp_sub/fp_mul/fp_inv under the
P-256 field prime and group order (edge values, and fp_mul's unreduced
second operand); affine P-256 scalar multiplication (random and edge
scalars of the ladder's range); and ECDSA, whose signatures must match
the formula and verify.

The Poly1305 program of v31-trim lives in the ELF header, where the byte
behind it ends it; the library links such a byte behind it too.
"""

import argparse
import ctypes
import hashlib
import os
from pathlib import Path
import random
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

P = 0xffffffff00000001000000000000000000000000ffffffffffffffffffffffff
N = 0xffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551
B = 0x5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b
GX = 0x6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296
GY = 0x4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5
PP = (1 << 130) - 5
M32 = 0xffffffff

PROBE = r'''
#include "sha256.h"
#include "chapoly.h"
#include "fp.h"
#include "p256.h"
void t_init(void) { sha_gentables(); fp_one[FP_SIZE - 1] = 1; }
void t_sha(uint8_t *out, uint8_t *m, size_t n) { sha256(out, m, n); }
void t_chacha(const uint8_t *k, uint32_t s, uint32_t c, uint8_t *b, size_t n)
{
#ifdef CHACHA_STATE
	static uint32_t st[16];		/* as in the server: bss, key words set */
	memcpy(st + 4, k, 32);
	chacha_xor(st, b, s, c, n);
#else
	chacha_xor(k, s, c, b, n);
#endif
}
long t_poly(uint8_t *tag, const uint8_t *k, const uint8_t *m, size_t n)
{
#ifdef POLY_COMPARE
	return (long)poly1305(tag, k, m, n);
#else
	poly1305(tag, k, m, n);
	return 0;
#endif
}
void t_fp(int op, const uint8_t *m, uint8_t *d, const uint8_t *a,
	  const uint8_t *b)
{
	fp_m = m;
	if (op == 0) fp_add(d, a, b);
	else if (op == 1) fp_sub(d, a, b);
	else if (op == 2) fp_mul(d, a, b);
	else fp_inv(d, a);
}
void t_smult(const uint8_t *k, const uint8_t *pt, uint8_t *out)
{
	memcpy(P256_K, k, 32);
	p256_smult(pt);
	memcpy(out, p256_w, 64);
}
void t_ecdsa(const uint8_t *d, const uint8_t *z, const uint8_t *k,
	     uint8_t *out)
{
	memcpy(P256_D, d, 32);
	memcpy(P256_Z, z, 32);
	memcpy(P256_K, k, 32);
#ifdef ECDSA_SIGN
	ecdsa_sign();
#else
	p256_smult(p256_g);
	p256_run(p256_ecdsa);
#endif
	memcpy(out, p256_w, 64);
}
void t_hostkey(const uint8_t *k, uint8_t *d_out)
{
	memcpy(P256_K, k, 32);
	p256_run(p256_hostkey);
	memcpy(d_out, P256_D, 32);
}
'''


def i2b(x):
    return x.to_bytes(32, "big")


def b2i(b):
    return int.from_bytes(b, "big")


def cbuf(b, extra=0):
    return ctypes.create_string_buffer(bytes(b) + bytes(extra), len(b) + extra)


# ---- references ----------------------------------------------------------

def rotl(x, n):
    return ((x << n) | (x >> (32 - n))) & M32


def chacha_block(key, seq, ctr):
    st = list(struct.unpack("<4I", b"expand 32-byte k"))
    st += list(struct.unpack("<8I", key))
    st += [ctr, 0] + list(struct.unpack("<2I", struct.pack(">Q", seq)))
    x = st[:]

    def q(a, b, c, d):
        x[a] = (x[a] + x[b]) & M32; x[d] = rotl(x[d] ^ x[a], 16)
        x[c] = (x[c] + x[d]) & M32; x[b] = rotl(x[b] ^ x[c], 12)
        x[a] = (x[a] + x[b]) & M32; x[d] = rotl(x[d] ^ x[a], 8)
        x[c] = (x[c] + x[d]) & M32; x[b] = rotl(x[b] ^ x[c], 7)

    for _ in range(10):
        q(0, 4, 8, 12); q(1, 5, 9, 13); q(2, 6, 10, 14); q(3, 7, 11, 15)
        q(0, 5, 10, 15); q(1, 6, 11, 12); q(2, 7, 8, 13); q(3, 4, 9, 14)
    return struct.pack("<16I", *((a + b) & M32 for a, b in zip(x, st)))


def chacha_ref(key, seq, ctr, data):
    out = bytearray(data)
    for off in range(0, len(data), 64):
        ks = chacha_block(key, seq, ctr)
        for i in range(min(64, len(data) - off)):
            out[off + i] ^= ks[i]
        ctr = (ctr + 1) & M32
    return bytes(out)


def poly_ref(key, msg):
    r = int.from_bytes(key[:16], "little") & 0x0ffffffc0ffffffc0ffffffc0fffffff
    s = int.from_bytes(key[16:], "little")
    h = 0
    for i in range(0, len(msg), 16):
        h = (h + int.from_bytes(msg[i:i + 16] + b"\x01", "little")) * r % PP
    return ((h + s) & ((1 << 128) - 1)).to_bytes(16, "little")


def padd(p1, p2):
    if p1 is None:
        return p2
    if p2 is None:
        return p1
    (x1, y1), (x2, y2) = p1, p2
    if x1 == x2:
        if (y1 + y2) % P == 0:
            return None
        lam = (3 * x1 * x1 - 3) * pow(2 * y1, P - 2, P) % P
    else:
        lam = (y2 - y1) * pow(x2 - x1, P - 2, P) % P
    x3 = (lam * lam - x1 - x2) % P
    return (x3, (lam * (x1 - x3) - y1) % P)


def pmul(k, pt):
    r = None
    while k:
        if k & 1:
            r = padd(r, pt)
        pt = padd(pt, pt)
        k >>= 1
    return r


G = (GX, GY)
assert (GY * GY - GX ** 3 + 3 * GX - B) % P == 0


# ---- the version under test ---------------------------------------------

def build(version, cc, directory):
    src = ROOT / version
    chapoly = (src / "chapoly.h").read_text()
    p256c = (src / "p256.c").read_text()
    (Path(directory) / "probe.c").write_text(PROBE)
    srcs = [str(Path(directory) / "probe.c"), str(src / "fp.c"),
            str(src / "p256.c")]
    if ".hdr.poly" in p256c:
        end = "0" if "#define OP_END 0\n" in p256c else "0xff"
        (Path(directory) / "zend.c").write_text(
            '__attribute__((section(".hdr.poly"), aligned(1))) '
            'const unsigned char poly_end = %s;\n' % end)
        srcs.append(str(Path(directory) / "zend.c"))
    defs = []
    if "ecdsa_sign" in (src / "p256.h").read_text():
        defs.append("-DECDSA_SIGN")
    if "unsigned poly1305" in chapoly:
        defs.append("-DPOLY_COMPARE")
    if "chacha_xor(uint32_t *st, uint8_t *buf" in chapoly:
        defs.append("-DCHACHA_STATE")
    lib = str(Path(directory) / "probe.so")
    subprocess.run(shlex.split(cc) + ["-std=c11", "-O1", "-shared", "-fPIC",
                                      "-w", "-I", str(src)] + defs + srcs +
                   ["-o", lib], check=True)
    return ctypes.CDLL(lib), "-DPOLY_COMPARE" in defs


def check(version, cc, iters):
    rng = random.Random(0x5eed31)
    with tempfile.TemporaryDirectory(prefix="nano-crypto-") as directory:
        lib, compare = build(version, cc, directory)
        lib.t_init()

        lens = list(range(200)) + [rng.randrange(4000) for _ in range(iters)]
        for n in lens:
            m = rng.randbytes(n)
            out = ctypes.create_string_buffer(32)
            lib.t_sha(out, cbuf(m, 80), n)      # padded in place: 72 free
            assert out.raw == hashlib.sha256(m).digest(), ("sha256", n)

        ccases = 0
        for n in [0, 4, 8, 12, 32, 60, 64, 68, 124, 128, 132, 4092] + \
                 [rng.randrange(1, 1023) * 4 for _ in range(iters // 4)]:
            key = rng.randbytes(32)
            seq = rng.getrandbits(32)
            ctr = rng.choice([0, 1, rng.getrandbits(32)])
            data = rng.randbytes(n)
            b = cbuf(data, 16)
            lib.t_chacha(cbuf(key), seq, ctr, b, n)
            assert b.raw[:n] == chacha_ref(key, seq, ctr, data), ("chacha", n)
            assert b.raw[n:] == bytes(16), ("chacha overrun", n)
            ccases += 1

        key = bytes.fromhex("85d6be7857556d337f4452fe42d506a8"
                            "0103808afb0db2fd4abff6af4149f51b")
        msg = b"Cryptographic Forum Research Group"
        assert poly_ref(key, msg).hex() == "a8061dc1305136c6c22b8baf0c0127a9"
        pcases = [(key, msg)]
        for n in list(range(70)) + [rng.randrange(4097) for _ in range(iters)]:
            k = rng.randbytes(32)
            if rng.random() < 0.2:
                k = bytes([rng.choice([0, 0xff])]) * 32
            m = rng.randbytes(n) if rng.random() < 0.8 else b"\xff" * n
            pcases.append((k, m))
        for k, m in pcases:
            tag = ctypes.create_string_buffer(16)
            mb = cbuf(m, 64)
            lib.t_poly(tag, cbuf(k), mb, len(m))
            want = poly_ref(k, m)
            assert tag.raw == want, ("poly1305", len(m), k.hex())
            if compare:
                same = ctypes.create_string_buffer(want, 16)
                assert lib.t_poly(same, cbuf(k), mb, len(m)) == 0
                bit = rng.randrange(128)
                bad = bytearray(want)
                bad[bit // 8] ^= 1 << (bit % 8)
                badb = ctypes.create_string_buffer(bytes(bad), 16)
                assert lib.t_poly(badb, cbuf(k), mb, len(m)) != 0
                assert badb.raw == want

        fcases = 0
        for m in (P, N):
            mb = cbuf(i2b(m))
            vals = [0, 1, 2, m - 1, m - 2, m // 2, (m + 1) // 2, 1 << 255,
                    rng.randrange(m)] + [rng.randrange(m) for _ in range(iters // 4)]
            for _ in range(iters * 3):
                a = rng.choice(vals) % m
                b = rng.choice(vals) % m
                for op, want in ((0, (a + b) % m), (1, (a - b) % m),
                                 (2, a * b % m)):
                    d = ctypes.create_string_buffer(32)
                    lib.t_fp(op, mb, d, cbuf(i2b(a)), cbuf(i2b(b)))
                    assert b2i(d.raw) == want, ("fp", op, hex(m), hex(a), hex(b))
                bb = rng.getrandbits(256)      # fp_mul: any 256-bit b
                d = ctypes.create_string_buffer(32)
                lib.t_fp(2, mb, d, cbuf(i2b(a)), cbuf(i2b(bb)))
                assert b2i(d.raw) == a * bb % m, ("fp mul", hex(a), hex(bb))
                fcases += 4
            for _ in range(iters // 4):
                a = rng.randrange(1, m)
                d = ctypes.create_string_buffer(32)
                lib.t_fp(3, mb, d, cbuf(i2b(a)), cbuf(i2b(0)))
                assert b2i(d.raw) == pow(a, m - 2, m), ("fp inv", hex(a))
                fcases += 1

        def scalar():
            while True:
                k = rng.getrandbits(256)
                if k < N and k >> 224:
                    return k
        ks = [scalar() for _ in range(max(8, iters // 20))]
        ks += [1 << 224, (1 << 224) + 1, (1 << 256) - (1 << 224) - 1,
               N - 1, N - 2, N - 3, (N - 1) // 2, (N + 1) // 2]
        pts = [G, pmul(scalar(), G)]
        for k in ks:
            pt = rng.choice(pts)
            out = ctypes.create_string_buffer(64)
            lib.t_smult(cbuf(i2b(k)), cbuf(i2b(pt[0]) + i2b(pt[1])), out)
            assert (b2i(out.raw[:32]), b2i(out.raw[32:])) == pmul(k, pt), \
                ("p256 [k]P", hex(k))
        sigs = max(4, iters // 40)
        for _ in range(sigs):
            d, z, k = scalar(), rng.getrandbits(256), scalar()
            dd = ctypes.create_string_buffer(32)
            lib.t_hostkey(cbuf(i2b(d)), dd)
            assert b2i(dd.raw) == d % N, "host key program"
            out = ctypes.create_string_buffer(64)
            lib.t_ecdsa(cbuf(i2b(d)), cbuf(i2b(z)), cbuf(i2b(k)), out)
            r, s = b2i(out.raw[:32]), b2i(out.raw[32:])
            assert r == pmul(k, G)[0] % N, "ecdsa r"
            assert s == pow(k, N - 2, N) * (z + r * d) % N, "ecdsa s"
            w = pow(s, N - 2, N)
            x = padd(pmul(z * w % N, G), pmul(r * w % N, pmul(d, G)))
            assert x[0] % N == r, "ecdsa verify"

        print(f"{version}: PASS - SHA-256 {len(lens)}, ChaCha20 {ccases}, "
              f"Poly1305 {len(pcases)}, field {fcases}, "
              f"P-256 {len(ks)} multiplications + {sigs} signatures")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("versions", nargs="*",
                        default=["v29-p256", "v30-chacha", "v31-trim"])
    parser.add_argument("--cc", default=os.environ.get("CC", "gcc"))
    parser.add_argument("--iters", type=int, default=100)
    args = parser.parse_args()
    for version in args.versions:
        check(version, args.cc, args.iters)
