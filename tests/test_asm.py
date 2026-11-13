#!/usr/bin/env python3
"""Differential test of v32-asm's routines against independent references.

v32-asm is one assembly file with its own register conventions, so it
cannot be loaded as a library the way tests/test_crypto.py loads the C
versions.  Instead main.S is assembled with -DTEST, which swaps the
listening socket for the command loop in v32-asm/test_main.S: one request
on standard input, the routine called as the server calls it, the result
on standard output.  The instructions under test are the server's own.

Checked here: the generated SHA-256 constants (all 64 K and the 8 initial
state words); SHA-256 (every length 0..199 and random lengths); ChaCha20
from block 0 (lengths that are multiples of four, as the server uses it,
with a guard behind the buffer); Poly1305 (RFC 8439 vector, random and
extreme keys and messages, and the equal / one-bit-off result of the
compare-as-you-write path); modular add, subtract, multiply (operands
aliased as the programs alias them; an unreduced multiplier) and inverse
under the P-256 field prime and group order, and add/multiply under
2^130-5; P-256 scalar multiplication over the ladder's range and edges;
ECDSA signatures (formula and verification); the mpint encoder; the key
generator's scalar range; and the packet layer back to back - what
send_packet() writes is parsed and decrypted here, then fed to
recv_packet(), intact and with single bits flipped.
"""

import argparse
import hashlib
import os
from pathlib import Path
import random
import select
import shlex
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_crypto as ref     # the references shared with the C versions

ROOT = Path(__file__).resolve().parents[1]
P, N, PP = ref.P, ref.N, ref.PP
MODS = (P, N, PP)

# FIPS 180-4, for the generated tables
K256 = """
428a2f98 71374491 b5c0fbcf e9b5dba5 3956c25b 59f111f1 923f82a4 ab1c5ed5
d807aa98 12835b01 243185be 550c7dc3 72be5d74 80deb1fe 9bdc06a7 c19bf174
e49b69c1 efbe4786 0fc19dc6 240ca1cc 2de92c6f 4a7484aa 5cb0a9dc 76f988da
983e5152 a831c66d b00327c8 bf597fc7 c6e00bf3 d5a79147 06ca6351 14292967
27b70a85 2e1b2138 4d2c6dfc 53380d13 650a7354 766a0abb 81c2c92e 92722c85
a2bfe8a1 a81a664b c24b8b70 c76c51a3 d192e819 d6990624 f40e3585 106aa070
19a4c116 1e376c08 2748774c 34b0bcb5 391c0cb3 4ed8aa4a 5b9cca4f 682e6ff3
748f82ee 78a5636f 84c87814 8cc70208 90befffa a4506ceb bef9a3f7 c67178f2
""".split()
H256 = "6a09e667 bb67ae85 3c6ef372 a54ff53a 510e527f 9b05688c 1f83d9ab " \
       "5be0cd19".split()


class Target:
    """The -DTEST build of a version, restarted when it exits."""

    def __init__(self, exe):
        self.exe = exe
        self.p = None

    def start(self):
        self.stop()
        self.p = subprocess.Popen([self.exe], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, bufsize=0)

    def stop(self):
        if self.p:
            self.p.stdin.close()
            self.p.stdout.close()
            self.p.wait()
            self.p = None

    def send(self, data):
        self.p.stdin.write(data)

    def recv(self, n, timeout=120):
        out = b""
        while len(out) < n:
            if not select.select([self.p.stdout], [], [], timeout)[0]:
                self.p.kill()
                raise TimeoutError(f"no answer: {len(out)} of {n} bytes")
            chunk = self.p.stdout.read(n - len(out))
            if not chunk:
                raise EOFError(f"target exited after {len(out)} of {n} bytes")
            out += chunk
        return out

    def call(self, request, n):
        self.send(request)
        return self.recv(n)


def i2b(x):
    return x.to_bytes(32, "big")


def b2i(b):
    return int.from_bytes(b, "big")


def mpint_ref(x):
    b = x.to_bytes(32, "big").lstrip(b"\0") or b"\0"
    if b[0] & 0x80:
        b = b"\0" + b
    return struct.pack(">I", len(b)) + b


def wire_len(n, encrypted):
    """Bytes send_packet() writes for an n-byte payload."""
    if encrypted:
        return 4 + ((n + 12) & ~7) + 16
    return 4 + ((n + 16) & ~7) - 4


def open_packet(key, seq, wire):
    """Parse and decrypt one chacha20-poly1305@openssh.com packet with the
    references; returns the payload."""
    k2, k1 = key[:32], key[32:]
    length = ref.chacha_ref(k1, seq, 0, wire[:4])
    pktlen = struct.unpack(">I", length)[0]
    assert len(wire) == 4 + pktlen + 16, "packet length"
    polykey = ref.chacha_ref(k2, seq, 0, bytes(32))
    assert wire[-16:] == ref.poly_ref(polykey, wire[:-16]), "packet tag"
    body = ref.chacha_ref(k2, seq, 1, wire[4:-16])
    pad = body[0]
    assert 4 <= pad < 12 and (pktlen % 8) == 0, "packet padding"
    assert body[len(body) - pad:] == bytes(pad), "padding bytes"
    return body[1:len(body) - pad]


def build(version, cc, ld, directory):
    src = ROOT / version
    obj = str(Path(directory) / "test.o")
    exe = str(Path(directory) / "test_bin")
    subprocess.run(shlex.split(cc) + ["-c", "-DTEST", str(src / "main.S"),
                                      "-o", obj], check=True)
    subprocess.run(shlex.split(ld) + ["-T", str(src / "flat.ld"),
                                      "--oformat", "binary", "-o", exe, obj],
                   check=True)
    os.chmod(exe, 0o755)
    return exe


def check(version, cc, ld, iters):
    rng = random.Random(0x5eed32)
    with tempfile.TemporaryDirectory(prefix="nano-asm-") as directory:
        t = Target(build(version, cc, ld, directory))
        t.start()

        tab = t.call(b"T", 288)
        words = struct.unpack("<72I", tab)
        assert ["%08x" % w for w in words[:64]] == K256, "SHA-256 K table"
        assert ["%08x" % w for w in words[64:]] == H256, "SHA-256 H0"

        lens = list(range(200)) + [rng.randrange(6000) for _ in range(iters)]
        for n in lens:
            m = rng.randbytes(n)
            out = t.call(b"S" + struct.pack("<I", n) + m, 32)
            assert out == hashlib.sha256(m).digest(), ("sha256", n)

        ccases = 0
        for n in [4, 8, 12, 32, 60, 64, 68, 124, 128, 132, 4092, 4160] + \
                 [rng.randrange(1, 1040) * 4 for _ in range(iters // 2)]:
            key = rng.randbytes(32)
            seq = rng.choice([3, 4, rng.getrandbits(32)])
            data = rng.randbytes(n)
            guard = rng.randbytes(16)
            out = t.call(b"C" + key + struct.pack("<II", seq, n) + data +
                         guard, n + 16)
            assert out[:n] == ref.chacha_ref(key, seq, 0, data), ("chacha", n)
            assert out[n:] == guard, ("chacha overrun", n)
            ccases += 1

        key = bytes.fromhex("85d6be7857556d337f4452fe42d506a8"
                            "0103808afb0db2fd4abff6af4149f51b")
        msg = b"Cryptographic Forum Research Group"
        pcases = [(key, msg)]
        for n in list(range(1, 70)) + \
                [rng.randrange(1, 4113) for _ in range(iters)]:
            k = rng.randbytes(32)
            if rng.random() < 0.2:
                k = bytes([rng.choice([0, 0xff])]) * 32
            m = rng.randbytes(n) if rng.random() < 0.8 else b"\xff" * n
            pcases.append((k, m))
        for k, m in pcases:
            want = ref.poly_ref(k, m)
            req = b"P" + k + struct.pack("<I", len(m)) + m
            out = t.call(req + bytes(16), 17)
            assert out[:16] == want, ("poly1305", len(m), k.hex())
            assert out[16] == (want == bytes(16)), "poly1305 compare"
            assert t.call(req + want, 17) == want + b"\1", "poly1305 equal"
            bit = rng.randrange(128)
            bad = bytearray(want)
            bad[bit // 8] ^= 1 << (bit % 8)
            assert t.call(req + bytes(bad), 17) == want + b"\0", \
                "poly1305 differs"

        fcases = 0

        def fp(op, mi, a, b):
            return b2i(t.call(b"F" + bytes([op, mi]) + i2b(a) + i2b(b), 32))

        for mi, m in enumerate(MODS):
            vals = [0, 1, 2, m - 1, m - 2, m // 2, (m + 1) // 2,
                    (1 << 255) % m, rng.randrange(m)] + \
                   [rng.randrange(m) for _ in range(iters // 4)]
            for _ in range(iters * 3):
                a = rng.choice(vals)
                b = rng.choice(vals)
                assert fp(0, mi, a, b) == (a + b) % m, ("add", mi, a, b)
                assert fp(3, mi, a, b) == (a + b) % m, ("add d=a", mi, a, b)
                assert fp(5, mi, a, b) == 2 * a % m, ("double", mi, a)
                assert fp(1, mi, a, b) == (a - b) % m, ("sub", mi, a, b)
                assert fp(4, mi, a, b) == (a - b) % m, ("sub d=b", mi, a, b)
                assert fp(2, mi, a, b) == a * b % m, ("mul", mi, a, b)
                bb = rng.getrandbits(256)       # multiplier: any 256 bits
                assert fp(2, mi, a, bb) == a * bb % m, ("mul wide", mi, a, bb)
                fcases += 7
            if m == PP:
                continue
            # what the ECDSA program relies on: an unreduced first operand
            for _ in range(iters):
                a = rng.choice([rng.getrandbits(256), (1 << 256) - 1, m,
                                m + 1, (1 << 256) - rng.randrange(1 << 32)])
                b = rng.choice(vals)
                got = fp(0, mi, a, b)
                assert got % m == (a + b) % m and got < 1 << 256, \
                    ("add unreduced", mi, a, b)
                fcases += 1
            for a in [1, 2, m - 1, m - 2] + \
                     [rng.randrange(1, m) for _ in range(iters // 4)]:
                got = b2i(t.call(b"I" + bytes([mi]) + i2b(a), 32))
                assert got == pow(a, m - 2, m), ("inverse", mi, a)
                fcases += 1

        def scalar():
            while True:
                k = rng.getrandbits(256)
                if k < N and k >> 224:
                    return k

        ks = [scalar() for _ in range(max(8, iters // 20))]
        ks += [1 << 224, (1 << 224) + 1, (1 << 256) - (1 << 224) - 1,
               N - 1, N - 2, N - 3, (N - 1) // 2, (N + 1) // 2]
        pts = [ref.G, ref.pmul(scalar(), ref.G)]
        for k in ks:
            pt = rng.choice(pts)
            out = t.call(b"M" + i2b(k) + i2b(pt[0]) + i2b(pt[1]), 64)
            assert (b2i(out[:32]), b2i(out[32:])) == ref.pmul(k, pt), \
                ("p256 [k]P", hex(k))

        sigs = max(4, iters // 40)
        zs = [rng.getrandbits(256) for _ in range(sigs)]
        zs[:3] = [(1 << 256) - 1, N, N + 1]     # unreduced hashes
        for z in zs:
            d, k = scalar(), scalar()
            out = t.call(b"E" + i2b(d) + i2b(k) + i2b(z), 64)
            r, s = b2i(out[:32]), b2i(out[32:])
            assert r == ref.pmul(k, ref.G)[0] % N, "ecdsa r"
            assert s == pow(k, N - 2, N) * (z + r * d) % N, "ecdsa s"
            w = pow(s, N - 2, N)
            x = ref.padd(ref.pmul(z * w % N, ref.G),
                         ref.pmul(r * w % N, ref.pmul(d, ref.G)))
            assert x[0] % N == r, "ecdsa verify"
            # the next multiplication must find the Z slot zero again
            out = t.call(b"M" + i2b(k) + i2b(ref.G[0]) + i2b(ref.G[1]), 64)
            assert (b2i(out[:32]), b2i(out[32:])) == ref.pmul(k, ref.G)

        mcases = 0
        for z in range(33):
            for top in (0x01, 0x7f, 0x80, 0xff):
                for _ in range(max(1, iters // 25)):
                    body = bytes([top]) + rng.randbytes(31 - z) if z < 32 \
                        else b""
                    x = b2i(bytes(z) + body)
                    out = t.call(b"N" + i2b(x), 44)
                    want = mpint_ref(x)
                    assert struct.unpack("<I", out[:4])[0] == len(want)
                    assert out[4:4 + len(want)] == want, ("mpint", hex(x))
                    mcases += 1

        for _ in range(4):
            out = t.call(b"K", 96)
            k = b2i(out[:32])
            assert 1 << 224 <= k < (1 << 256) - (1 << 224), "keygen range"
            assert (b2i(out[32:64]), b2i(out[64:])) == ref.pmul(k, ref.G)

        acases = 0
        for n in [1, 2, 3, 4, 5, 11, 12, 13, 17, 55, 165, 300, 4000] + \
                 [rng.randrange(1, 4000) for _ in range(iters // 4)]:
            key = rng.randbytes(64)
            seq = rng.choice([3, 4, 5, 255, 256, rng.getrandbits(32)])
            payload = rng.randbytes(n)
            req = b"A" + key + struct.pack("<II", seq, n) + payload
            wire = t.call(req, wire_len(n, True))
            assert open_packet(key, seq, wire) == payload, ("send", n)
            back = t.call(wire, 4 + n)
            assert back == struct.pack("<I", n) + payload, ("recv", n)
            # one flipped bit anywhere (length, payload, tag) is refused:
            # the target gives up on the connection, which here ends it
            bit = rng.randrange(len(wire) * 8)
            bad = bytearray(wire)
            bad[bit // 8] ^= 1 << (bit % 8)
            t.send(req)
            t.recv(len(wire))
            t.send(bytes(bad))
            try:
                t.recv(1)
                raise AssertionError(("tampered packet accepted", n, bit))
            except (EOFError, BrokenPipeError):
                pass
            t.start()
            acases += 1
        # before NEWKEYS (sequence numbers 0..2) packets are in the clear
        for n in [1, 17, 165, 300]:
            payload = rng.randbytes(n)
            seq = rng.randrange(3)
            req = b"A" + bytes(64) + struct.pack("<II", seq, n) + payload
            wire = t.call(req, wire_len(n, False))
            pktlen, pad = struct.unpack(">IB", wire[:5])
            assert pktlen == len(wire) - 4 and len(wire) % 8 == 0
            assert 4 <= pad and wire[5:5 + n] == payload
            assert wire[5 + n:] == bytes(pad), "plain padding"
            assert t.call(wire, 4 + n) == struct.pack("<I", n) + payload

        t.stop()
        print(f"{version}: PASS - SHA-256 tables, SHA-256 {len(lens)}, "
              f"ChaCha20 {ccases}, Poly1305 {len(pcases)}, field {fcases}, "
              f"P-256 {len(ks)} multiplications + {sigs} signatures, "
              f"mpint {mcases}, packets {acases} + 4")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("versions", nargs="*", default=["v32-asm"])
    parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
    parser.add_argument("--ld", default=os.environ.get("LD", "ld"))
    parser.add_argument("--iters", type=int, default=100)
    args = parser.parse_args()
    for version in args.versions:
        check(version, args.cc, args.ld, args.iters)
