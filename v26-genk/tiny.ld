/* Minimal layout: one PT_LOAD covering ELF header + phdrs + all sections,
 * packed back-to-back (no page-alignment padding between sections, no
 * NOTE/GNU_STACK/GNU_PROPERTY program headers). The kernel only needs the
 * one LOAD entry. Segment is RWX - fine for this size-golf experiment,
 * matches the norelro/no-libc choices documented in the Makefile. */
ENTRY(_start)
PHDRS { load PT_LOAD FLAGS(7) FILEHDR PHDRS; }
SECTIONS {
    . = 0x400000 + SIZEOF_HEADERS;
    .text   : { *(.text*) } :load
    .rodata : { *(.rodata*) }
    .data   : { *(.data.rel.ro*) *(.data*) }
    .bss    : { *(.bss*) *(COMMON) }
    /DISCARD/ : { *(.note*) *(.comment) *(.eh_frame*) }
}
