/*
 * nolibc.h - Minimal freestanding libc replacement for v23-nolibc
 *
 * Provides direct Linux x86-64 syscalls plus tiny implementations of the
 * libc functions the nano SSH server actually uses. Built with
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

/* ------------------------------------------------------------------ */
/* errno (simple global; not thread-safe but the server is single-thread) */
/* ------------------------------------------------------------------ */
extern int errno;
#define EINTR 4

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
#define SYS_open        2
#define SYS_close       3
#define SYS_mmap        9
#define SYS_socket      41
#define SYS_accept      43
#define SYS_bind        49
#define SYS_listen      50
#define SYS_setsockopt  54
#define SYS_exit_group  231
#define SYS_getrandom   318

/* ------------------------------------------------------------------ */
/* errno-translating wrapper: kernel returns -errno on failure.        */
/* ------------------------------------------------------------------ */
static inline long __sysret(long r) {
    if (r < 0 && r > -4096) {
        errno = (int)(-r);
        return -1;
    }
    return r;
}

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
#define O_RDONLY 0

static inline ssize_t read(int fd, void *buf, size_t n) {
    return __sysret(__syscall3(SYS_read, fd, buf, n));
}
static inline ssize_t write(int fd, const void *buf, size_t n) {
    return __sysret(__syscall3(SYS_write, fd, buf, n));
}
static inline int close(int fd) {
    return (int)__sysret(__syscall1(SYS_close, fd));
}
static inline int open(const char *path, int flags) {
    return (int)__sysret(__syscall3(SYS_open, path, flags, 0));
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

/* send/recv are just write/read for a connected TCP socket (flags=0). */
static inline ssize_t send(int fd, const void *buf, size_t n, int flags) {
    (void)flags;
    return write(fd, buf, n);
}
static inline ssize_t recv(int fd, void *buf, size_t n, int flags) {
    (void)flags;
    return read(fd, buf, n);
}

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
void  *memmove(void *dst, const void *src, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);

/* ------------------------------------------------------------------ */
/* Heap: simple arena. Allocations in the SSH server are short-lived   */
/* and freed in LIFO order within a single packet read. We use a       */
/* freelist-free bump allocator that resets when all blocks are freed. */
/* ------------------------------------------------------------------ */
void *malloc(size_t n);
void  free(void *p);

/* ------------------------------------------------------------------ */
/* Debug output: stripped to a no-op variadic stub (returns 0).        */
/* ------------------------------------------------------------------ */
#define stderr ((void *)0)
#define stdout ((void *)0)
static inline int fprintf(void *stream, const char *fmt, ...) {
    (void)stream; (void)fmt;
    return 0;
}

#endif /* NOLIBC_H */
