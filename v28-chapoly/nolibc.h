/*
 * nolibc.h - Minimal freestanding libc replacement for v23-nolibc
 *
 * Provides direct Linux x86-64 syscalls plus tiny implementations of the
 * libc functions the nano SSH server actually uses (no errno, no heap, no
 * stdio - none of them are reachable any more). Built with
 * -nostdlib -ffreestanding -static so that musl libc (~33 KB) is dropped
 * entirely; only the ~18 KB of app+crypto .text remains.
 *
 * Target: x86-64 Linux only.
 */
#ifndef NOLIBC_H
#define NOLIBC_H

#include <stdint.h>   /* freestanding: just integer typedefs, no code */
#include <stddef.h>   /* freestanding: size_t, NULL */

/* ------------------------------------------------------------------ */
/* Basic types normally from sys/types.h                              */
/* ------------------------------------------------------------------ */
typedef long          ssize_t;
typedef unsigned int  socklen_t;

/* Unaligned, aliasing-safe 32/64-bit words: x86-64 loads and stores these
 * directly, so big-endian wire fields and hash words become one bswap and
 * one access instead of four or eight shifted byte moves. */
typedef uint32_t __attribute__((aligned(1), may_alias)) u32a;
typedef uint64_t __attribute__((aligned(1), may_alias)) u64a;

/* ------------------------------------------------------------------ */
/* Raw syscall (x86-64 System V): syscall number in rax, args in       */
/* rdi, rsi, rdx, r10, r8, r9. Return in rax.                          */
/* ------------------------------------------------------------------ */
static inline long __syscall6(long n, long a1, long a2, long a3,
                              long a4, long a5, long a6) {
    long ret;
    register long r10 __asm__("r10") = a4;
    register long r8  __asm__("r8")  = a5;
    register long r9  __asm__("r9")  = a6;
    __asm__ volatile ("syscall"
                      : "=a"(ret)
                      : "a"(n), "D"(a1), "S"(a2), "d"(a3),
                        "r"(r10), "r"(r8), "r"(r9)
                      : "rcx", "r11", "memory");
    return ret;
}
#define __syscall0(n)                  __syscall6((n),0,0,0,0,0,0)
#define __syscall1(n,a)                __syscall6((n),(long)(a),0,0,0,0,0)
#define __syscall2(n,a,b)              __syscall6((n),(long)(a),(long)(b),0,0,0,0)
#define __syscall3(n,a,b,c)            __syscall6((n),(long)(a),(long)(b),(long)(c),0,0,0)
#define __syscall4(n,a,b,c,d)          __syscall6((n),(long)(a),(long)(b),(long)(c),(long)(d),0,0)
#define __syscall5(n,a,b,c,d,e)        __syscall6((n),(long)(a),(long)(b),(long)(c),(long)(d),(long)(e),0)

/* x86-64 syscall numbers */
#define SYS_read        0
#define SYS_write       1
#define SYS_close       3
#define SYS_socket      41
#define SYS_accept      43
#define SYS_bind        49
#define SYS_listen      50
#define SYS_setsockopt  54
#define SYS_exit_group  231
#define SYS_getrandom   318

/* The kernel returns -errno on failure, which is already negative, and
 * every caller here only tests the sign of the result. Passing it through
 * unchanged removes the errno translation from all ten syscall wrappers;
 * there is no errno in this build. */
#define __sysret(r) (r)

/* ------------------------------------------------------------------ */
/* exit                                                                */
/* ------------------------------------------------------------------ */
static inline void _exit_group(int code) {
    __syscall1(SYS_exit_group, code);
    __builtin_unreachable();
}

/* ------------------------------------------------------------------ */
/* File / fd I/O                                                       */
/* ------------------------------------------------------------------ */
static inline int close(int fd) {
    return (int)__sysret(__syscall1(SYS_close, fd));
}

/* ------------------------------------------------------------------ */
/* Sockets                                                             */
/* ------------------------------------------------------------------ */
#define AF_INET        2
#define SOCK_STREAM    1
#define SOL_SOCKET     1
#define SO_REUSEADDR   2
#define INADDR_ANY     ((uint32_t)0x00000000)

struct sockaddr {
    uint16_t sa_family;
    char     sa_data[14];
};

struct in_addr {
    uint32_t s_addr;
};

struct sockaddr_in {
    uint16_t       sin_family;
    uint16_t       sin_port;    /* network byte order */
    struct in_addr sin_addr;
    uint8_t        sin_zero[8];
};

static inline int socket(int domain, int type, int protocol) {
    return (int)__sysret(__syscall3(SYS_socket, domain, type, protocol));
}
static inline int bind(int fd, const struct sockaddr *addr, socklen_t len) {
    return (int)__sysret(__syscall3(SYS_bind, fd, addr, len));
}
static inline int listen(int fd, int backlog) {
    return (int)__sysret(__syscall2(SYS_listen, fd, backlog));
}
static inline int accept(int fd, struct sockaddr *addr, socklen_t *len) {
    return (int)__sysret(__syscall3(SYS_accept, fd, addr, len));
}
static inline int setsockopt(int fd, int level, int optname,
                             const void *optval, socklen_t optlen) {
    return (int)__sysret(__syscall5(SYS_setsockopt, fd, level, optname,
                                    optval, optlen));
}

/* There is no send()/recv() wrapper: for a connected TCP socket with no
 * flags they are read(2)/write(2), which main.c issues directly. */

/* host-to-network short (x86-64 is little-endian) */
static inline uint16_t htons(uint16_t x) {
    return (uint16_t)((x << 8) | (x >> 8));
}

/* ------------------------------------------------------------------ */
/* Memory / string functions (freestanding builtins may emit calls to */
/* these, so provide real definitions in nolibc.c).                    */
/* ------------------------------------------------------------------ */
void  *memcpy(void *dst, const void *src, size_t n);
void  *memset(void *dst, int c, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);

#endif /* NOLIBC_H */
