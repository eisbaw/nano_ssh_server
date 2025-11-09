/* v22-compatible linker script - more aggressive but safe
 * Removes all unnecessary metadata and debug sections
 */

SECTIONS
{
  /DISCARD/ : {
    /* Discard all unnecessary sections */
    *(.comment)            /* Compiler comments */
    *(.note.GNU-stack)     /* Stack marking */
    *(.note.gnu.build-id)  /* Build ID */
    *(.eh_frame)           /* Exception handling frames */
    *(.eh_frame_hdr)       /* Exception frame header */
    *(.gnu.lto_*)          /* LTO metadata */
    *(.gnu_debuglink)      /* Debug info link */
  }
}

/* Insert before the default linker script (not replacing it) */
INSERT AFTER .text;
