#!/usr/bin/env python3
"""Differential test of v33-portable's core against independent references.

v33-portable/test_main.c includes the core (nanossh.c) and answers the
requests that v32-asm's test build answers, so the checks are those of
tests/test_asm.py - the generated SHA-256 constants, SHA-256, ChaCha20,
Poly1305, the modular arithmetic under all three moduli, P-256 scalar
multiplication, ECDSA, the mpint encoder, the key generator and the packet
layer back to back - plus two that only this version's ChaCha20 allows:
lengths that are not multiples of four, and a starting block of 1.

The point of the version is that the same C runs anywhere, so the test
build is made in several ways and each is put through all of it:

  default           the compiler's defaults, -O2
  --sanitize        -fsanitize=address,undefined (any report is fatal)
  --cc / --cflags   another compiler or mode, e.g. --cc clang, -m32
  --run             a command that runs the binary, e.g. a qemu-user
                    emulator for a cross-compiled big-endian build (see
                    v33-portable/cross.nix and `just test-portable-cross`)
"""

import argparse
import os
from pathlib import Path
import random
import shlex
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_asm
import test_crypto as ref

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "v33-portable"
STRICT = ["-std=c99", "-pedantic", "-Wall", "-Wextra", "-Werror"]


def build(cc, cflags, directory):
    exe = str(Path(directory) / "test_bin")
    subprocess.run(shlex.split(cc) + STRICT + shlex.split(cflags) +
                   [str(SRC / "test_main.c"), "-o", exe], check=True)
    return exe


def extra_chacha(argv, iters):
    """Any length, from block 0 and from block 1."""
    rng = random.Random(0x5eed33)
    t = test_asm.Target(argv)
    t.start()
    cases = 0
    for n in list(range(1, 70)) + [127, 129, 255, 4091] + \
            [rng.randrange(1, 4100) for _ in range(iters // 2)]:
        for cmd, ctr in ((b"C", 0), (b"c", 1)):
            key = rng.randbytes(32)
            seq = rng.choice([0, 3, 255, 256, rng.getrandbits(32)])
            data = rng.randbytes(n)
            guard = rng.randbytes(16)
            out = t.call(cmd + key + struct.pack("<II", seq, n) + data +
                         guard, n + 16)
            assert out[:n] == ref.chacha_ref(key, seq, ctr, data), \
                ("chacha", n, ctr)
            assert out[n:] == guard, ("chacha overrun", n)
            cases += 1
    t.stop()
    return cases


def check(label, cc, cflags, run, iters):
    with tempfile.TemporaryDirectory(prefix="nano-portable-") as directory:
        argv = shlex.split(run) + [build(cc, cflags, directory)]
        line = test_asm.run_checks(f"v33-portable ({label})", argv, iters)
        more = extra_chacha(argv, iters)
        print(f"{line}, ChaCha20 any length {more}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
    parser.add_argument("--cflags", default="-O2")
    parser.add_argument("--run", default="",
                        help="command prefix that runs the test binary")
    parser.add_argument("--label", default=None)
    parser.add_argument("--sanitize", action="store_true",
                        help="build with AddressSanitizer and UBSan")
    parser.add_argument("--iters", type=int, default=100)
    args = parser.parse_args()
    cflags = args.cflags
    label = args.label or f"{args.cc} {cflags}"
    if args.sanitize:
        cflags += " -g -fsanitize=address,undefined" \
                  " -fno-sanitize-recover=all"
        label = args.label or f"{args.cc} {args.cflags}, ASan + UBSan"
        os.environ.setdefault("ASAN_OPTIONS", "detect_leaks=1")
    check(label, args.cc, cflags, args.run, args.iters)
