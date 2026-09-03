/* Minimal layout: one PT_LOAD covering the ELF header and all sections,
 * packed back-to-back (no page-alignment padding, no NOTE/GNU_STACK/
 * GNU_PROPERTY program headers).  The segment is RWX - fine for this
 * size-golf experiment, matches the norelro/no-libc choices documented in
 * the Makefile.  SUBALIGN(1) drops the per-input-section alignment padding,
 * the explicit `.` start address on each output section the alignment of
 * the section itself (ld would otherwise align it to its most-aligned
 * input, 32 bytes for the P-256 tables); x86-64 has no alignment
 * requirement for code or byte arrays, and .bss is left alone so the
 * 64-bit tables there keep their natural alignment.
 *
 * The 0x68-byte packed ELF header is written here as data, so that the
 * linker fills in the entry point and the sizes and objcopy -O binary
 * emits the finished file.  e_phoff = 0x30: the program header starts
 * inside the ELF header, its first words doubling as e_flags, e_ehsize/
 * e_phentsize and e_phnum - fields the kernel either ignores or wants to
 * see exactly these values in.  Of the ELF header the kernel checks only
 * the magic, EI_CLASS, e_type, e_machine, e_phentsize and e_phnum; of the
 * program header p_type, p_flags, p_offset, p_vaddr, p_filesz and p_memsz.
 * p_vaddr = 0x400001 with p_offset = 1 maps file offset f to 0x400000 + f.
 * Everything the kernel never reads holds data the program does use
 * (hdr_* below): metadata packing, not compression - the bytes mapped
 * into memory are the file's bytes. */
ENTRY(_start)
PHDRS { load PT_LOAD FLAGS(7); }
SECTIONS {
    . = 0x400000;
    .ehdr . : SUBALIGN(1) {
        BYTE(0x7f) BYTE(0x45) BYTE(0x4c) BYTE(0x46)     /* 00 magic         */
        BYTE(2)                                         /* 04 64-bit        */
        /* 05..0f EI_DATA, EI_VERSION, OSABI and the padding, none of which
         * the kernel reads: the 11-byte ECDSA program of p256.c */
        *(.hdr.ecdsa)
        ASSERT(. == 0x10, "p256_ecdsa is not 11 bytes"); /* . is section-
                                                           relative here */
        SHORT(2) SHORT(0x3e)                            /* 10 EXEC, x86-64  */
        LONG(0xffffffff)                                /* 14 e_version:free*/
        QUAD(_start)                                    /* 18 e_entry       */
        QUAD(0x30)                                      /* 20 e_phoff       */
        /* 22..2d: the CHANNEL_OPEN_CONFIRMATION tail, sender channel 0,
         * window 32768, max packet 16384 - its six leading zeros are the
         * top of e_phoff, the rest is e_shoff, which is never read */
        hdr_win = . - 6;
        BYTE(0x80) BYTE(0) BYTE(0) BYTE(0) BYTE(0x40) BYTE(0)
        SHORT(0xffff)                                   /* 2e free          */
        hdr_one = .;                                    /* setsockopt's 1   */
        LONG(1)                                         /* 30 p_type LOAD   */
        SHORT(0xffff) SHORT(56)                         /* 34 p_flags RWX,
                                                              e_phentsize   */
        SHORT(1) SHORT(0) SHORT(0) SHORT(0)             /* 38 e_phnum = 1 =
                                                              p_offset      */
        QUAD(0x400001)                                  /* 40 p_vaddr       */
        /* 48 p_paddr, unused by the kernel: INADDR_ANY:PORT for bind(),
         * which reads 16 bytes but ignores the 8 of sin_zero (p_filesz) */
        hdr_sockaddr = .;
        SHORT(2) SHORT((PORT >> 8) | ((PORT & 0xff) << 8)) LONG(0)
        QUAD(_img_end - 0x400001)                       /* 50 p_filesz      */
        LONG(_bss_end - 0x400001) BYTE(0)               /* 58 p_memsz, low 5
                                                              bytes         */
        /* 5d: the version string vs (main.c, 21 bytes): its three leading
         * zeros complete p_memsz, the next eight are p_align, which the
         * kernel ignores for an executable, and only the last ten are
         * image bytes */
        *(.hdr.vs)
    } :load
    /* _start (.text.entry) falls through into main (.text.startup.main);
     * KEEP: nothing references main, so --gc-sections would drop it */
    .text   . : SUBALIGN(1) { *(.text.entry) KEEP(*(.text.startup*))
                              *(.text*) *(.rodata*) }
    .data   . : SUBALIGN(1) { *(.data.rel.ro*) *(.data*) }
    _img_end = .;
    .bss    : { *(.bss*) *(COMMON) }
    _bss_end = .;
    /DISCARD/ : { *(.note*) *(.comment) *(.eh_frame*) }
}
