#!/usr/bin/env python3
"""Remove the ELF section header table (sstrip, ELFkickers-style).

Section headers describe the file for tools (objdump, gdb); the kernel
loads a binary purely from its program headers. Dropping the table and
the .shstrtab it references shrinks the file with zero effect on the
loaded image (this is metadata removal, NOT compression - the bytes
mapped into RAM are identical).

Usage: sstrip.py <elf-file>   (modified in place)
"""
import struct
import sys


def main(path):
    with open(path, "rb") as f:
        data = bytearray(f.read())

    if data[:4] != b"\x7fELF" or data[4] != 2:
        sys.exit(f"{path}: not a 64-bit ELF")

    (e_phoff,) = struct.unpack_from("<Q", data, 0x20)
    (e_phentsize, e_phnum) = struct.unpack_from("<HH", data, 0x36)

    # File must retain everything any program header references.
    end = e_phoff + e_phentsize * e_phnum
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        (p_offset, _va, _pa, p_filesz) = struct.unpack_from("<4Q", data, off + 8)
        end = max(end, p_offset + p_filesz)

    # Zero e_shoff, e_shentsize, e_shnum, e_shstrndx.
    struct.pack_into("<Q", data, 0x28, 0)
    struct.pack_into("<HHH", data, 0x3A, 0, 0, 0)

    before = len(data)
    with open(path, "wb") as f:
        f.write(data[:end])
    print(f"sstrip: {path}: {before} -> {end} bytes")


if __name__ == "__main__":
    main(sys.argv[1])
