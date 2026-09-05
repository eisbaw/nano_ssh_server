#!/usr/bin/env python3
"""Check the compact ChaCha20 core against an independent scalar reference."""

import argparse
import ctypes
import os
from pathlib import Path
import random
import shlex
import struct
import subprocess
import tempfile


MASK = (1 << 32) - 1
ROOT = Path(__file__).resolve().parents[1]


def rotl(x, n):
    return ((x << n) | (x >> (32 - n))) & MASK


def block(key, seq, counter):
    # Original ChaCha layout: 64-bit counter, 64-bit nonce. OpenSSH uses
    # the packet sequence number in big-endian order as its nonce.
    state = list(struct.unpack("<4I", b"expand 32-byte k"))
    state += list(struct.unpack("<8I", key))
    state += [counter, 0] + list(struct.unpack("<2I", struct.pack(">Q", seq)))
    x = state.copy()

    def quarter(a, b, c, d):
        x[a] = (x[a] + x[b]) & MASK
        x[d] = rotl(x[d] ^ x[a], 16)
        x[c] = (x[c] + x[d]) & MASK
        x[b] = rotl(x[b] ^ x[c], 12)
        x[a] = (x[a] + x[b]) & MASK
        x[d] = rotl(x[d] ^ x[a], 8)
        x[c] = (x[c] + x[d]) & MASK
        x[b] = rotl(x[b] ^ x[c], 7)

    for _ in range(10):
        quarter(0, 4, 8, 12)
        quarter(1, 5, 9, 13)
        quarter(2, 6, 10, 14)
        quarter(3, 7, 11, 15)
        quarter(0, 5, 10, 15)
        quarter(1, 6, 11, 12)
        quarter(2, 7, 8, 13)
        quarter(3, 4, 9, 14)
    return struct.pack("<16I", *((a + b) & MASK for a, b in zip(x, state)))


def reference(key, seq, counter, data, length):
    result = bytearray(data)
    for offset in range(0, length, 64):
        stream = block(key, seq, counter)
        # Both server versions XOR complete words. An authenticated odd
        # packet length can touch at most three bytes of the tag behind it.
        count = (min(64, length - offset) + 3) & ~3
        for i in range(count):
            result[offset + i] ^= stream[i]
        counter = (counter + 1) & MASK
    return bytes(result)


def check(version, cc):
    with tempfile.TemporaryDirectory(prefix="nano-chacha-") as directory:
        shim = Path(directory) / "probe.c"
        library = Path(directory) / "probe.so"
        shim.write_text(
            '#include "chapoly.h"\n'
            'void probe(const uint8_t *k, uint32_t s, uint32_t c, '
            'uint8_t *b, size_t n) { chacha_xor(k, s, c, b, n); }\n'
        )
        subprocess.run(
            shlex.split(cc) + ["-std=c11", "-Oz", "-shared", "-fPIC",
                              "-Wall", "-Wextra", "-Wno-unused-function",
                              "-I", str(ROOT / version), str(shim),
                              "-o", str(library)], check=True,
        )
        lib = ctypes.CDLL(str(library))
        pointer = ctypes.POINTER(ctypes.c_uint8)
        lib.probe.argtypes = [pointer, ctypes.c_uint32, ctypes.c_uint32,
                              pointer, ctypes.c_size_t]
        lib.probe.restype = None
        rng = random.Random(0x434841434841)

        # Fixed all-zero ChaCha20 block vector also checks the Python oracle.
        expected = bytes.fromhex(
            "76b8e0ada0f13d90405d6ae55386bd28bdd219b8a08ded1aa836efcc8b770dc7da"
            "41597c5157488d7724e03fb8d84a376a43b8f41518a11cc387b669b2ee6586"
        )
        assert block(bytes(32), 0, 0) == expected
        cases = [(bytes(32), 0, 0, 64, 0, bytes(64))]
        lengths = [0, 1, 2, 3, 4, 5, 7, 8, 15, 16, 31, 32,
                   63, 64, 65, 127, 128, 129, 255, 256, 257, 4092]
        for n in lengths:
            for alignment in range(4):
                for counter in [0, 1, MASK - 1]:
                    cases.append((rng.randbytes(32), rng.getrandbits(32),
                                  counter, n, alignment, rng.randbytes(n)))
        for _ in range(100):
            n = rng.randrange(4093)
            cases.append((rng.randbytes(32), rng.getrandbits(32),
                          rng.getrandbits(32), n, rng.randrange(4), rng.randbytes(n)))

        for key, seq, counter, n, alignment, data in cases:
            prefix = b"\xa5" * (16 + alignment)
            tail = b"\x5a" * (16 + (-n % 4))
            raw = prefix + data + tail
            buf = (ctypes.c_uint8 * len(raw)).from_buffer_copy(raw)
            k = (ctypes.c_uint8 * 32).from_buffer_copy(key)
            out = ctypes.cast(ctypes.byref(buf, len(prefix)), pointer)
            lib.probe(k, seq, counter, out, n)
            expected = prefix + reference(key, seq, counter, data + tail, n)
            assert bytes(buf) == expected, (version, n, alignment, seq, counter)
            assert bytes(k) == key
        print(f"{version}: {len(cases)} ChaCha20 cases PASS "
              "(known vector, random inputs, word/block boundaries, guards)")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("versions", nargs="*", default=["v29-p256", "v30-chacha"])
    parser.add_argument("--cc", default=os.environ.get("CC", "gcc"))
    args = parser.parse_args()
    for version in args.versions:
        check(version, args.cc)
