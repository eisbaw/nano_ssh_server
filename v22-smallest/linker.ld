/* Minimal linker script for nano_ssh_server
 * Optimizes section layout and removes unnecessary sections
 */

OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS
{
  /* Program headers */
  . = 0x400000 + SIZEOF_HEADERS;

  /* Text segment - executable code */
  .text : {
    *(.text.startup)
    *(.text.hot)
    *(.text .text.*)
  }

  /* Read-only data */
  .rodata : {
    *(.rodata .rodata.*)
  }

  /* Data that has been relocated */
  .data.rel.ro : {
    *(.data.rel.ro.local* .gnu.linkonce.d.rel.ro.local.*)
    *(.data.rel.ro .data.rel.ro.* .gnu.linkonce.d.rel.ro.*)
  }

  /* Dynamic linking info */
  .dynamic : { *(.dynamic) }
  .dynsym : { *(.dynsym) }
  .dynstr : { *(.dynstr) }
  .rela.dyn : { *(.rela.dyn) }
  .rela.plt : { *(.rela.plt) }

  /* GOT and PLT */
  .got : { *(.got) }
  .got.plt : { *(.got.plt) }
  .plt : { *(.plt) }

  /* Initialized data */
  .data : {
    *(.data .data.*)
  }

  /* Uninitialized data */
  .bss : {
    *(.bss .bss.*)
    *(COMMON)
  }

  /* Discard unnecessary sections */
  /DISCARD/ : {
    *(.note.GNU-stack)
    *(.gnu_debuglink)
    *(.gnu.lto_*)
    *(.comment)
    *(.eh_frame)
    *(.eh_frame_hdr)
    *(.note.gnu.build-id)
  }
}
