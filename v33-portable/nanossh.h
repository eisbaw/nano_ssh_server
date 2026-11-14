/*
 * nanossh.h - Nano SSH Server, portable core (v33-portable).
 *
 * The core (nanossh.c) is ISO C99.  It uses <stdint.h>, <stddef.h>,
 * memcpy() and memset(), the three functions declared at "provided by the
 * platform" below, and setjmp()/longjmp() unless told otherwise.  It
 * makes no assumption about byte order, alignment or the signedness of
 * char, needs no integer wider than 32 bits and no floating point,
 * allocates nothing and keeps all its state in static storage: about
 * 10 KB at the default NANOSSH_MAXPKT, one connection at a time.
 *
 * To port it, write net_read(), net_write() and rng_bytes() for your
 * network stack and random number generator (net_posix.c does it for
 * sockets, net_linux_x86_64.c without a C library) and call the core like
 * this:
 *
 *      (bring the network up and start listening)
 *      nanossh_init();
 *      for (;;) {
 *              (wait for a TCP connection)
 *              nanossh_serve();
 *              (close the connection)
 *      }
 *
 * It is slow: all its arithmetic is done a byte at a time (see nanossh.c).
 * A login takes half a second on a PC; expect a minute on a processor a
 * hundred times slower.
 */
#ifndef NANOSSH_H
#define NANOSSH_H

/* A platform's own settings, from a file named on the command line
 * (-DNANOSSH_CONFIG_FILE='"file.h"'); read before anything else, so that
 * it can also say how the C library's headers are to be read. */
#ifdef NANOSSH_CONFIG_FILE
#include NANOSSH_CONFIG_FILE
#endif

#include <stddef.h>
#include <stdint.h>

/* ---- provided by the core ---- */

/* Once, before the first connection: generates the SHA-256 constants and
 * the host key (a new one at every start).  Needs rng_bytes(). */
void nanossh_init(void);

/* Serve the connection that net_read()/net_write() talk to, from the
 * version exchange to "Hello World"; returns when the client has closed
 * it or as soon as anything goes wrong.  The caller closes it. */
void nanossh_serve(void);

/* ---- provided by the platform ---- */

/* Read up to n bytes (n > 0) from the connection into buf, blocking until
 * at least one is there.  Returns how many were read; 0 or a negative
 * value means end of file or an error and ends nanossh_serve(). */
int net_read(uint8_t *buf, unsigned n);

/* Write up to n bytes (n > 0) of buf to the connection.  Returns how many
 * were written; 0 or a negative value ends nanossh_serve(). */
int net_write(const uint8_t *buf, unsigned n);

/* Fill buf with n bytes (at most 32) from a cryptographically secure
 * random number generator.  The host key and every signature depend on
 * it: a poor generator gives the host key away. */
void rng_bytes(uint8_t *buf, unsigned n);

/* ---- configuration (with -D, or in NANOSSH_CONFIG_FILE) ---- */

/* Largest packet_length accepted from the client.  The largest packet a
 * client has to send is its KEXINIT, about 1.5 KB from OpenSSH 9.9 (2000
 * works with it, 1024 does not).  The core's two buffers are this size
 * plus 32 and plus 869 bytes. */
#ifndef NANOSSH_MAXPKT
#define NANOSSH_MAXPKT 4092u
#endif

/* The one non-local exit: an I/O error or a malformed packet anywhere in
 * the handshake abandons nanossh_serve().  setjmp()/longjmp() by default;
 * a platform without <setjmp.h> defines all three itself. */
#ifndef NANOSSH_SETJMP
#include <setjmp.h>
#define NANOSSH_JMP_BUF    jmp_buf
#define NANOSSH_SETJMP(b)  setjmp(b)
#define NANOSSH_LONGJMP(b) longjmp(b, 1)
#endif

#endif /* NANOSSH_H */
