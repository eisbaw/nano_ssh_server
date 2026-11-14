/* tiny.ld - the ELF file around nanossh.c + net_linux_x86_64.c.
 *
 * One loadable segment holding everything, written out as it is: the ELF
 * header and the program header are data in this script, so the linker
 * only fills in the entry point and the sizes, and objcopy -O binary gives
 * the finished file.  No section headers, no alignment padding (x86-64
 * needs none for code or bytes; .bss keeps its own).
 *
 * The program header starts inside the ELF header (e_phoff = 0x30): its
 * first words double as e_flags, e_ehsize/e_phentsize and e_phnum, fields
 * the kernel ignores or wants exactly these values in.  The kernel checks
 * the magic, EI_CLASS, e_type, e_machine, e_phentsize and e_phnum, and of
 * the program header p_type, p_flags, p_offset, p_vaddr, p_filesz and
 * p_memsz; p_vaddr = 0x400001 with p_offset = 1 maps file offset f to
 * 0x400000 + f.  Fields it never reads hold data the program uses:
 * hdr_sockaddr, and four objects of the two C files, picked up by the
 * section names -fdata-sections gives them (the C says nothing about
 * it; if one changes size an ASSERT here stops the build).  That is
 * packing, not compression: the file's bytes are the bytes in memory. */
ENTRY(_start)
PHDRS { load PT_LOAD FLAGS(7); }
SECTIONS {
    . = 0x400000;
    .ehdr . : SUBALIGN(1) {
        BYTE(0x7f) BYTE(0x45) BYTE(0x4c) BYTE(0x46)     /* 00 magic         */
        BYTE(2)                                         /* 04 64-bit        */
        /* 05..0f: the rest of e_ident, unread */
        *(.rodata.inv_prog)
        ASSERT(. == 0x0f, "inv_prog is not 10 bytes");  /* . is relative to
                                                           the section here */
        BYTE(0)
        SHORT(2) SHORT(0x3e)                            /* 10 EXEC, x86-64  */
        *(.data.conn)                                   /* 14 e_version,
                                                              unread        */
        ASSERT(. == 0x18, "conn is not 4 bytes");
        QUAD(_start)                                    /* 18 e_entry       */
        QUAD(0x30)                                      /* 20 e_phoff       */
        *(.rodata.affine_prog)                          /* 28 e_shoff,
                                                              unread        */
        ASSERT(. == 0x2f, "affine_prog is not 7 bytes");
        BYTE(0)
        hdr_one = .;                                    /* setsockopt's 1   */
        LONG(1)                                         /* 30 p_type LOAD   */
        SHORT(7) SHORT(56)                              /* 34 p_flags RWX,
                                                              e_phentsize   */
        SHORT(1) SHORT(0) SHORT(0) SHORT(0)             /* 38 e_phnum = 1 =
                                                              p_offset      */
        QUAD(0x400001)                                  /* 40 p_vaddr       */
        /* 48 p_paddr, unread: 0.0.0.0:PORT for bind(), which reads 16
         * bytes but ignores the last 8 (sin_zero; here p_filesz) */
        hdr_sockaddr = .;
        SHORT(2) SHORT((PORT >> 8) | ((PORT & 0xff) << 8)) LONG(0)
        QUAD(_img_end - 0x400001)                       /* 50 p_filesz      */
        LONG(_bss_end - 0x400001) BYTE(0)               /* 58 p_memsz, its
                                                              low 5 bytes   */
        /* 5d: the core's version string, 0 0 0 15 "SSH-2.0-NanoSSH" CR LF:
         * its three zeros complete p_memsz, the next eight bytes are
         * p_align, which the kernel ignores for an executable, and the
         * last ten lie behind the header */
        *(.rodata.version)
        ASSERT(. == 0x72, "version is not 21 bytes");
    } :load
    /* _start (.text.entry) falls through into main() */
    .text   . : SUBALIGN(1) { *(.text.entry) KEEP(*(.text.main))
                              KEEP(*(.text.startup*)) *(.text*) *(.rodata*) }
    .data   . : SUBALIGN(1) { *(.data*) }
    _img_end = .;
    .bss    . : { *(.bss*) *(COMMON) }
    _bss_end = .;
    /DISCARD/ : { *(.note*) *(.comment) *(.eh_frame*) }
}
