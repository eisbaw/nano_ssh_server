#!/usr/bin/env python3
"""Pack the ELF header and the one program header into 0x68 bytes and drop
everything else (section header table, .shstrtab: sstrip, ELFkickers-style).

The kernel loads a static executable from e_entry and the program headers
alone, and of the ELF header it checks only e_ident[0..3] (magic),
EI_CLASS, e_type, e_machine, e_phentsize and e_phnum.  e_ident[7..15],
e_version, e_shoff, e_flags, e_ehsize and the e_sh* fields are never read,
so the 56-byte program header can start at e_phoff = 0x30, inside the
64-byte ELF header, with its first two words doubling as header fields:

    0x30  p_type   = e_flags                       -> 1 (PT_LOAD)
    0x34  p_flags  = e_ehsize | e_phentsize << 16  -> 7 (RWX; only the low
                                                      bits are looked at)
    0x38  p_offset = e_phnum | e_shentsize << 16 | e_shnum << 32
                     | e_shstrndx << 48            -> 1
    0x40  p_vaddr  = 0x400001, so file offset f maps to 0x400000 + f
                     exactly as with a conventional header
    0x48  p_paddr  (ignored)
    0x50  p_filesz   0x58  p_memsz   0x60  p_align

The image, which tiny.ld links to start at 0x400068, follows at file
offset 0x68 instead of 0x78.  This is metadata packing, NOT compression:
the bytes mapped into RAM are identical, and readelf still parses it.

Usage: sstrip.py <elf-file>   (modified in place)
"""
import struct
import sys

BASE = 0x400000          # tiny.ld's load address
HDR = 0x68               # packed header size = file offset of the image


def main(path):
    with open(path, "rb") as f:
        data = f.read()

    if data[:4] != b"\x7fELF" or data[4] != 2:
        sys.exit(f"{path}: not a 64-bit ELF")

    (e_phoff,) = struct.unpack_from("<Q", data, 0x20)
    (e_phentsize, e_phnum) = struct.unpack_from("<HH", data, 0x36)
    if e_phentsize != 56 or e_phnum != 1:
        sys.exit(f"{path}: expected exactly one program header")
    (p_type, _fl, p_offset, p_vaddr, _pa, p_filesz, p_memsz, p_align) = \
        struct.unpack_from("<IIQQQQQQ", data, e_phoff)
    if p_type != 1 or p_vaddr != BASE + HDR:
        sys.exit(f"{path}: expected one PT_LOAD at {BASE + HDR:#x}")

    img = data[p_offset:p_offset + p_filesz]
    # e_ident .. e_entry unchanged, e_phoff = 0x30, e_shoff = 0, then the
    # program header (values as in the table above).
    hdr = bytearray(data[:0x20])
    hdr += struct.pack("<QQ", 0x30, 0)
    hdr += struct.pack("<IIQQQQQQ", 1, 7 | 56 << 16, 1, BASE + 1, BASE + 1,
                       HDR - 1 + len(img), HDR - 1 + p_memsz, p_align)
    assert len(hdr) == HDR

    with open(path, "wb") as f:
        f.write(hdr + img)
    print(f"sstrip: {path}: {len(data)} -> {HDR + len(img)} bytes")


if __name__ == "__main__":
    main(sys.argv[1])
