/*
 * net_posix.c - the platform layer for anything with BSD sockets and a C
 * library: Linux, the BSDs, macOS, and with small changes the sockets API
 * of lwIP or Zephyr.  With nanossh.c this is the whole server:
 *
 *      cc -O2 nanossh.c net_posix.c -o nano_ssh_server
 *
 * It listens on PORT (2222 unless -DPORT=n says otherwise) and serves one
 * connection at a time.
 */
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "nanossh.h"

#ifndef PORT
#define PORT 2222
#endif

static int conn = -1;

int net_read(uint8_t *buf, unsigned n)
{
	return (int)read(conn, buf, n);
}

int net_write(const uint8_t *buf, unsigned n)
{
	return (int)write(conn, buf, n);
}

/* The kernel's generator.  Without it there is no host key worth having,
 * so failure ends the program. */
void rng_bytes(uint8_t *buf, unsigned n)
{
	static int fd = -1;

	if (fd < 0)
		fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0 || read(fd, buf, n) != (ssize_t)n) {
		perror("/dev/urandom");
		exit(1);
	}
}

int main(void)
{
	struct sockaddr_in addr;
	int fd, one = 1;

	/* a client that disappears must not kill the server with SIGPIPE */
	signal(SIGPIPE, SIG_IGN);

	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(PORT);
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0 ||
	    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) < 0 ||
	    bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0 ||
	    listen(fd, 1) < 0) {
		perror("listen");
		return 1;
	}

	nanossh_init();
	for (;;) {
		conn = accept(fd, NULL, NULL);
		if (conn < 0)
			continue;
		nanossh_serve();
		close(conn);
	}
}
