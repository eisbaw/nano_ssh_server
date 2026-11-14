/*
 * net_linux_x86_64.c - the platform layer with no C library at all: Linux
 * system calls on x86-64, issued directly.  This file, tiny.ld and
 * nanossh.c make the binary whose size this version is measured by.
 *
 * Unlike the core this file is as unportable as it gets (GNU C inline
 * assembly, one kernel, one processor); net_posix.c is the portable
 * equivalent.  It provides what nanossh.h asks for, the two C library
 * functions the core uses, and the program around them: listen on PORT,
 * serve one connection at a time, for ever.
 */
#include "nanossh.h"

#ifndef PORT
#define PORT 2222
#endif

enum {
	SYS_read = 0, SYS_write = 1, SYS_close = 3, SYS_socket = 41,
	SYS_accept = 43, SYS_bind = 49, SYS_listen = 50, SYS_setsockopt = 54,
	SYS_exit = 60, SYS_getrandom = 318
};

/* A system call with three arguments (the kernel ignores the registers of
 * arguments a call does not take) and one with five.  A negative result
 * is an error. */
static long sys3(long nr, long a, long b, long c)
{
	__asm__ volatile ("syscall" : "+a"(nr) : "D"(a), "S"(b), "d"(c)
			  : "rcx", "r11", "memory");
	return nr;
}

static long sys5(long nr, long a, long b, long c, long d, long e)
{
	register long r10 __asm__("r10") = d;
	register long r8 __asm__("r8") = e;

	__asm__ volatile ("syscall" : "+a"(nr)
			  : "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8)
			  : "rcx", "r11", "memory");
	return nr;
}

static int conn = -1, lsock;

int net_read(uint8_t *buf, unsigned n)
{
	return (int)sys3(SYS_read, conn, (long)buf, n);
}

int net_write(const uint8_t *buf, unsigned n)
{
	return (int)sys3(SYS_write, conn, (long)buf, n);
}

/* getrandom(2) fills requests of up to 256 bytes completely, or fails. */
void rng_bytes(uint8_t *buf, unsigned n)
{
	if (sys3(SYS_getrandom, (long)buf, n, 0) != (long)n)
		sys3(SYS_exit, 1, 0, 0);
}

void *memcpy(void *dst, const void *src, size_t n)
{
	void *d = dst;

	__asm__ volatile ("rep movsb" : "+D"(d), "+S"(src), "+c"(n)
			  : : "memory");
	return dst;
}

void *memset(void *dst, int c, size_t n)
{
	void *d = dst;

	__asm__ volatile ("rep stosb" : "+D"(d), "+c"(n) : "a"(c) : "memory");
	return dst;
}

/* The listening address, 0.0.0.0:PORT, and SO_REUSEADDR's 1: tiny.ld
 * finds both in the ELF header (in a field the kernel does not read, and
 * in one that has to hold a 1). */
extern const unsigned char hdr_sockaddr[16];
extern const int hdr_one;

/* The kernel enters at _start with the stack aligned to 16 bytes, where a
 * C function expects it 8 off, as after a call: one push, and then on
 * into main(), which tiny.ld places right behind. */
__asm__(".pushsection .text.entry,\"ax\"\n"
	".globl _start\n"
	"_start:\n\t"
	"push %rax\n\t"
	".popsection");

/* One connection after the other.  A connection that is given up - by
 * nanossh_restart(), from somewhere inside nanossh_serve() - gets here
 * again from the top, which is why closing the last one comes first.
 * (A failed accept() fails the first write, and so on.)
 * used, externally_visible, noinline: entered by a jump from assembly. */
__attribute__((used, externally_visible, noinline, noreturn))
void serve_loop(void)
{
	for (;;) {
		sys3(SYS_close, conn, 0, 0);
		conn = (int)sys3(SYS_accept, lsock, 0, 0);
		nanossh_serve();
	}
}

/* Nothing on the stack is needed again, so there is no unwinding: the
 * stack pointer goes back to the top of a stack of our own (8 off a
 * multiple of 16, as on entry to any function). */
static unsigned char stack[16384] __attribute__((aligned(16)));

__attribute__((noreturn))
void nanossh_restart(void)
{
	__asm__ volatile ("mov %0, %%esp\n\tjmp serve_loop"
			  : : "i"(stack + sizeof stack - 8) : "memory");
	__builtin_unreachable();
}

/* used, externally_visible: nothing in C refers to it */
__attribute__((used, externally_visible, noreturn))
int main(void)
{
	long fd = sys3(SYS_socket, 2 /* AF_INET */, 1 /* SOCK_STREAM */, 0);

	lsock = (int)fd;
	sys5(SYS_setsockopt, fd, 1 /* SOL_SOCKET */, 2 /* SO_REUSEADDR */,
	     (long)&hdr_one, sizeof hdr_one);
	/* socket()'s failure fails bind() too */
	if ((sys3(SYS_bind, fd, (long)hdr_sockaddr, sizeof hdr_sockaddr) |
	     sys3(SYS_listen, fd, 1, 0)) < 0)
		sys3(SYS_exit, 1, 0, 0);

	nanossh_init();
	serve_loop();
}
