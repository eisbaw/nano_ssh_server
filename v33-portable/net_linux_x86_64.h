/*
 * net_linux_x86_64.h - configuration of the core for net_linux_x86_64.c,
 * which has no C library and therefore no <setjmp.h>.  It does not need
 * one: the program does nothing but serve connections, so giving one up
 * is starting over with the next - nanossh_restart() resets the stack
 * and re-enters the accept loop - and there is no state to save for that.
 * (Named on the command line: -DNANOSSH_CONFIG_FILE='"net_linux_x86_64.h"'.)
 */
#ifndef NET_LINUX_X86_64_H
#define NET_LINUX_X86_64_H

/* The core includes <string.h> for memcpy() and memset(), which
 * net_linux_x86_64.c defines.  Compilers that fortify by default (the Nix
 * wrapper, some distributions) would have that header call the C
 * library's checking variants instead, and there is no C library. */
#undef _FORTIFY_SOURCE

__attribute__((noreturn)) void nanossh_restart(void);

#define NANOSSH_JMP_BUF    char
#define NANOSSH_SETJMP(b)  ((void)(b), 0)
#define NANOSSH_LONGJMP(b) nanossh_restart()

#endif
