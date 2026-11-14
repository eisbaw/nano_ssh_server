/*
 * test_main.c - command loop for tests/test_portable.py.
 *
 * Not part of the server: this includes nanossh.c, so that its static
 * routines can be called one at a time, and speaks the request protocol
 * of v32-asm/test_main.S on standard input and output - one command byte
 * and its fields in, the result out - so that the same Python references
 * check both versions.  Plain hosted C: it builds for any target that
 * has a C library, which is how the core is run on big-endian and 32-bit
 * processors (under qemu) and under the sanitizers.
 *
 * net_read() and net_write() are standard input and output here, so the
 * packet layer can be driven back to back; a routine that gives the
 * connection up ends the program, as does the end of the input.
 */
#include <stdio.h>
#include <stdlib.h>

#include "nanossh.c"

int net_read(uint8_t *buf, unsigned n)
{
	return (int)fread(buf, 1, n, stdin);
}

int net_write(const uint8_t *buf, unsigned n)
{
	n = (unsigned)fwrite(buf, 1, n, stdout);
	fflush(stdout);
	return (int)n;
}

void rng_bytes(uint8_t *buf, unsigned n)
{
	static FILE *f;

	if (!f)
		f = fopen("/dev/urandom", "rb");
	if (!f || fread(buf, 1, n, f) != n)
		exit(2);
}

static uint8_t in[8192], out[8192];

static void rd(uint8_t *b, size_t n)
{
	if (n && fread(b, 1, n, stdin) != n)
		exit(0);
}

static void wr(const uint8_t *b, size_t n)
{
	fwrite(b, 1, n, stdout);
	fflush(stdout);
}

/* the requests carry their 32-bit fields little-endian */
static uint32_t rd32(void)
{
	uint8_t b[4];

	rd(b, 4);
	return get32le(b);
}

static void put32le(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

int main(void)
{
	uint32_t n, s;
	unsigned i, op;
	int c;

	if (NANOSSH_SETJMP(dead))
		return 0;
	sha_init();
	while ((c = getchar()) != EOF)
		switch (c) {
		case 'T':	/* -> K[64], H0[8] as generated */
			for (i = 0; i < 72; i++)
				put32le(out + 4 * i, sha_k[i]);
			wr(out, 288);
			break;
		case 'S':	/* u32 n, n bytes -> SHA-256 */
			n = rd32();
			if (n > sizeof in - 72)
				return 3;
			rd(in, n);
			sha256(out, in, n);
			wr(out, 32);
			break;
		case 'C':	/* key 32, u32 seq, u32 n, n + 16 bytes -> the
				 * same, the first n XORed with the keystream
				 * from block 0 */
		case 'c':	/* the same from block 1 */
			rd(out, 32);
			seq = rd32();
			n = rd32();
			if (n > sizeof in - 16)
				return 3;
			rd(in, n + 16);
			chacha(in, n, out, c == 'c');
			wr(in, n + 16);
			break;
		case 'P':	/* key 32, u32 n, n bytes, tag 16 -> the tag
				 * computed, and 1 if it is the one given */
			rd(out, 32);
			n = rd32();
			if (n > sizeof in - 17)
				return 3;
			rd(in, n + 16);
			in[n + 16] = !poly1305(in + n, out, in, n);
			wr(in + n, 17);
			break;
		case 'F':	/* op, modulus, a 32, b 32 -> result 32; op 0
				 * add, 1 sub, 2 mul, 3 add into a, 4 sub into
				 * b, 5 double in place */
			rd(in, 2);
			op = in[0];
			fm = modulus + FE * in[1];
			rd(W(X1), 2 * FE);
			if (op == 0)
				fp_add(W(T1), W(X1), W(Y1), 0, 0xff);
			else if (op == 1)
				fp_add(W(T1), W(X1), W(Y1), 0xff, 0xff);
			else if (op == 2)
				fp_mul(W(T1), W(X1), W(Y1));
			else if (op == 3)
				fp_add(W(X1), W(X1), W(Y1), 0, 0xff);
			else if (op == 4)
				fp_add(W(Y1), W(X1), W(Y1), 0xff, 0xff);
			else
				fp_add(W(X1), W(X1), W(X1), 0, 0xff);
			wr(op < 3 ? W(T1) : op == 4 ? W(Y1) : W(X1), FE);
			break;
		case 'I':	/* modulus, a 32 -> a^(m-2) */
			rd(in, 1);
			fm = modulus + FE * in[0];
			rd(W(Z1), FE);
			fp_inv();
			wr(W(T0), FE);
			break;
		case 'M':	/* k 32, point 64 -> [k] point */
			rd(W(K), FE);
			rd(in, 2 * FE);
			smult(in);
			wr(W(X2), 2 * FE);
			break;
		case 'E':	/* d 32, k 32, z 32 -> r || s, as serve() signs */
			rd(W(D), FE);
			rd(W(K), FE);
			smult(base_point);
			rd(W(X1), FE);
			fm = NMOD;
			run(ecdsa_prog);
			wr(W(X2), 2 * FE);
			break;
		case 'N':	/* number 32 -> u32 length of its mpint, and 40
				 * bytes holding it */
			rd(in, FE);
			put32le(out, (uint32_t)(put_mpint(out + 4, in) -
						(out + 4)));
			wr(out, 44);
			break;
		case 'K':	/* -> the scalar keygen() drew, and its point */
			keygen();
			wr(W(K), FE);
			wr(W(X2), 2 * FE);
			break;
		case 'A':	/* key 64 (both directions), u32 seq, u32 n,
				 * n bytes -> the packet send_packet() writes;
				 * then, given that packet back, u32 n and the
				 * payload recv_packet() finds */
			rd(keys, 2 * FE);
			memcpy(keys + 2 * FE, keys, 2 * FE);
			s = rd32();
			n = rd32();
			if (n > MAXPKT - 1)
				return 3;
			rd(PAY, n);
			seq = s;
			send_packet(n);
			seq = s;
			n = (uint32_t)recv_packet();
			put32le(out, n);
			wr(out, 4);
			wr(PAY, n);
			break;
		default:
			return 4;
		}
	return 0;
}
