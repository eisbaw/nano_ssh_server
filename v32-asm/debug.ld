/* Debugging aid only: the same code at the same addresses in an ordinary
 * ELF file with symbols and a real .bss, for gdb and valgrind.  Not what
 * ships; see flat.ld. */
ENTRY(_start)
PHDRS { load PT_LOAD FLAGS(7); }
SECTIONS {
    . = 0x400000;
    .text : { *(.text) } :load
    .bss (NOLOAD) : { BYTE(0); . = ALIGN(0x10000); } :load
    /DISCARD/ : { *(.note*) *(.comment) }
}
