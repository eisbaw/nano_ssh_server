/* Minimal layout: one PT_LOAD covering ELF header + phdr + all sections,
 * packed back-to-back (no page-alignment padding between sections, no
 * NOTE/GNU_STACK/GNU_PROPERTY program headers). The kernel only needs the
 * one LOAD entry. Segment is RWX - fine for this size-golf experiment,
 * matches the norelro/no-libc choices documented in the Makefile.
 *
 * The segment deliberately has no FILEHDR/PHDRS: sstrip.py replaces the
 * linker's 0x78 bytes of headers with a 0x68-byte packed header (the
 * program header folded into the tail of the ELF header, see there), so the
 * image is linked to start at 0x400068 and lands at file offset 0x68 with
 * every byte at its link address.
 *
 * SUBALIGN(1) drops the per-input-section alignment padding between the
 * input sections. The explicit `.` start address on each output section
 * drops the padding before the section as well: without it ld aligns an
 * output section to its most-aligned input (32 bytes, the P-256 tables),
 * which cost 9 bytes between .text and .rodata - ALIGN(1) cannot lower it,
 * an explicit address skips the alignment. x86-64 has no alignment
 * requirement for code or for the byte arrays in .rodata; .bss is left
 * alone so the 64-bit tables there keep their natural alignment. */
ENTRY(_start)
PHDRS { load PT_LOAD FLAGS(7); }
SECTIONS {
    . = 0x400000 + 0x68;           /* HDR in sstrip.py */
    .text   . : SUBALIGN(1) { *(.text*) *(.rodata*) } :load
    .data   . : SUBALIGN(1) { *(.data.rel.ro*) *(.data*) }
    .bss    : { *(.bss*) *(COMMON) }
    /DISCARD/ : { *(.note*) *(.comment) *(.eh_frame*) }
}
