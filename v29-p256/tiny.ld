/* Minimal layout: one PT_LOAD covering ELF header + phdrs + all sections,
 * packed back-to-back (no page-alignment padding between sections, no
 * NOTE/GNU_STACK/GNU_PROPERTY program headers). The kernel only needs the
 * one LOAD entry. Segment is RWX - fine for this size-golf experiment,
 * matches the norelro/no-libc choices documented in the Makefile.
 *
 * SUBALIGN(1) drops the per-input-section alignment padding between the
 * output sections (~38 bytes here); x86-64 has no alignment requirement for
 * code or for the byte arrays in .rodata, and .bss is left alone so the
 * 64-bit tables there keep their natural alignment. */
ENTRY(_start)
PHDRS { load PT_LOAD FLAGS(7) FILEHDR PHDRS; }
SECTIONS {
    . = 0x400000 + SIZEOF_HEADERS;
    .text   : SUBALIGN(1) { *(.text*) *(.rodata*) } :load
    .data   : SUBALIGN(1) { *(.data.rel.ro*) *(.data*) }
    .bss    : { *(.bss*) *(COMMON) }
    /DISCARD/ : { *(.note*) *(.comment) *(.eh_frame*) }
}
