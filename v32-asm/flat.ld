/* One flat output section at 0x400000: the file is the image, byte for
 * byte.  main.S writes the ELF header itself and addresses everything
 * behind the image (bss) through absolute symbols, so there is nothing
 * else to lay out. */
ENTRY(_start)
SECTIONS {
    . = 0x400000;
    .text : { *(.text) }
    /DISCARD/ : { *(*) }
}
