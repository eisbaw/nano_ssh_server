/*
 * nanossh.c - Nano SSH Server, portable core (v33-portable).
 *
 * The server of v31-trim and v32-asm - ecdh-sha2-nistp256,
 * ecdsa-sha2-nistp256, chacha20-poly1305@openssh.com, password
 * authentication, one session channel that says "Hello World" - in ISO C99
 * with no compiler extensions, no assembly and no assumptions about the
 * machine.  See nanossh.h for what a platform has to provide and
 * optimization_log.txt for the design, the sizes and the tests.
 *
 * Everything the protocol needs is built on one arithmetic routine,
 * fp_add(): 256-bit modular addition and subtraction a byte at a time.
 * Multiplication is 512 of those, P-256 is a few dozen multiplications
 * per scalar bit, Poly1305 runs on the same routine with a third modulus,
 * and the SHA-256 constants are extracted with it at startup.  That makes
 * the code small and slow: a login is about 21,000 multiplications, some
 * 700 million additions of two bytes - half a second on a PC.
 */
#include "nanossh.h"
#include <string.h>

#define MAXPKT NANOSSH_MAXPKT

/* ---- big-endian and little-endian 32-bit words, a byte at a time ---- */

static uint32_t get32(const uint8_t *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
	       (uint32_t)p[2] << 8 | p[3];
}

static void put32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)v;
}

static uint32_t get32le(const uint8_t *p)
{
	return (uint32_t)p[3] << 24 | (uint32_t)p[2] << 16 |
	       (uint32_t)p[1] << 8 | p[0];
}

/* ---- arithmetic modulo p, n and 2^130 - 5 ----
 * Numbers are 32 bytes, most significant first, and live in a file of 16
 * slots that the routines and the programs below address by number. */

#define FE 32
enum { D, X2, Y2, K, ZERO, KN, T0, X1, Y1, Z1, SX, SY, SZ, T1, T2, T3 };
/* D     the host key's secret scalar
 * X2 Y2 the point P a multiplication starts from, then its result;
 *       ECDSA's r and s; Poly1305's accumulator and key
 * K     the scalar: ephemeral secret, ECDSA nonce; Poly1305's block
 * ZERO  never written
 * KN    K + n, the scalar the ladder really uses
 * X1 Y1 Z1  the ladder's R (projective); X1 is ECDSA's message hash
 * SX SY SZ  the ladder's S = R + P
 * T0..T3    scratch; T0 is the inverse fp_inv() computes */
static uint8_t slot[16 * FE];
#define W(s) (slot + FE * (s))

static const uint8_t modulus[3 * FE] = {
	/* p = 2^256 - 2^224 + 2^192 + 2^96 - 1, the field of P-256 */
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	/* n, the order of its base point */
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xbc, 0xe6, 0xfa, 0xad, 0xa7, 0x17, 0x9e, 0x84,
	0xf3, 0xb9, 0xca, 0xc2, 0xfc, 0x63, 0x25, 0x51,
	/* 2^130 - 5, Poly1305's */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfb
};
#define PMOD modulus
#define NMOD (modulus + FE)
#define QMOD (modulus + 2 * FE)

static const uint8_t *fm;	/* the modulus in use */

/*
 * d = a + b - m (neg = 0) or d = a - b (neg = 0xff), plus m if that is
 * negative; mask = 0 takes b as zero.  For a and b below m the result is
 * a + b or a - b modulo m, below m; a may be anything below 2^256 if the
 * true sum stays below 2^256 + m.  d may be a or b.
 *
 * Both are one addition: a + (255 - m) + b + 1 is a + b - m + 2^256, and
 * a + (255 - b) + 1 is a - b + 2^256, a byte at a time with the carry in c
 * (at most 2).  The carry out of the top byte is the 2^256: it is missing
 * exactly when the true value is negative, and then m is added back -
 * always added, under a mask, so the work done does not depend on the
 * numbers.
 */
static void fp_add(uint8_t *d, const uint8_t *a, const uint8_t *b,
		   unsigned neg, unsigned mask)
{
	const uint8_t *m = fm;
	unsigned c = 1;
	int i;

	for (i = FE - 1; i >= 0; i--) {
		c += a[i] + ((b[i] & mask) ^ neg) + ((m[i] | neg) ^ 0xffu);
		d[i] = (uint8_t)c;
		c >>= 8;
	}
	mask = (c - 1) >> 8;		/* ones if there was no carry */
	c = 0;
	for (i = FE - 1; i >= 0; i--) {
		c += d[i] + (m[i] & mask);
		d[i] = (uint8_t)c;
		c >>= 8;
	}
}

/*
 * r = a b modulo m, by doubling and adding over the bits of b from the
 * top: r = 2r + (bit ? a : 0).  a must be below m; b may be anything.  r
 * must not be a or b.  The bit only ever becomes fp_add()'s mask.
 */
static void fp_mul(uint8_t *r, const uint8_t *a, const uint8_t *b)
{
	unsigned i;

	memset(r, 0, FE);
	for (i = 0; i < 8 * FE; i++) {
		fp_add(r, r, r, 0, 0xff);
		fp_add(r, r, a, 0, 0u - (b[i >> 3] >> (7 - (i & 7)) & 1));
	}
}

/*
 * The longer formulas are programs for a small interpreter rather than
 * sequences of calls: an operation costs two bytes, { a << 4 | d,
 * op << 4 | b }, for d = a + b, a - b or a b, or for fp_inv().  A zero
 * first byte ends a program (no instruction is D = D op b).
 */
#define ADD(d, a, b) (a) << 4 | (d), 0 << 4 | (b)
#define SUB(d, a, b) (a) << 4 | (d), 1 << 4 | (b)
#define MUL(d, a, b) (a) << 4 | (d), 2 << 4 | (b)
#define INV          0xff, 3 << 4
#define END          0

static void run(const uint8_t *ip);

/*
 * T0 = Z1^(m-2), which is 1/Z1 for a prime m (Fermat), by squaring and
 * multiplying over the bits of m - 2 from the top; Y2 is scratch.  m - 2
 * is m with its last byte lowered by 2 (the last bytes of p and n are at
 * least 2), and its top bit is set, which is the initial copy.  The
 * exponent is public, so choosing by its bits gives nothing away.
 */
static const uint8_t inv_prog[] = {
	MUL(Y2, T0, T0),		/* a clear bit: square */
	ADD(T0, Y2, ZERO),
	END,
	MUL(Y2, T0, T0),		/* a set bit: square and multiply */
	MUL(T0, Y2, Z1),
	END
};

static void fp_inv(void)
{
	unsigned i, e;

	memcpy(W(T0), W(Z1), FE);
	for (i = 1; i < 8 * FE; i++) {
		e = fm[i >> 3];
		if (i >= 8 * (FE - 1))
			e -= 2;
		run(inv_prog + (e << (i & 7) & 0x80 ? 5 : 0));
	}
}

static void run(const uint8_t *ip)
{
	for (; ip[0]; ip += 2) {
		uint8_t *d = W(ip[0] & 15);
		const uint8_t *a = W(ip[0] >> 4), *b = W(ip[1] & 15);
		unsigned op = ip[1] >> 4;

		if (op < 2)
			fp_add(d, a, b, (0u - op) & 0xff, 0xff);
		else if (op == 2)
			fp_mul(d, a, b);
		else
			fp_inv();
	}
}

/* ---- P-256 ---- */

static const uint8_t base_point[2 * FE] = {
	0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47,
	0xf8, 0xbc, 0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2,
	0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33, 0xa0,
	0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96,
	0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b,
	0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e, 0x16,
	0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31, 0x5e, 0xce,
	0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5
};

/* One step of the ladder in projective coordinates (x = X/Z, y = Y/Z):
 * R = 2R in place (dbl-1998-cmo-2 with a = -3; each coordinate is
 * overwritten after its last use), then S = R + P for the affine P
 * (add-1998-cmo-2 with Z2 = 1). */
static const uint8_t step_prog[] = {
	MUL(T0, X1, X1),		/* XX */
	MUL(T1, Z1, Z1),		/* ZZ */
	SUB(T1, T0, T1),
	ADD(T2, T1, T1),
	ADD(T2, T2, T1),		/* w = 3 (XX - ZZ) */
	MUL(T3, Y1, Z1),
	ADD(T3, T3, T3),		/* s = 2 Y Z */
	MUL(T1, T3, T3),		/* ss */
	MUL(Z1, T3, T1),		/* Z3 = s^3 */
	MUL(T1, Y1, T3),		/* R = Y s */
	MUL(SZ, T1, T1),		/* RR */
	MUL(T0, X1, T1),
	ADD(T0, T0, T0),		/* B = 2 X R */
	MUL(T1, T2, T2),
	SUB(T1, T1, T0),
	SUB(T1, T1, T0),		/* h = w^2 - 2B */
	MUL(X1, T1, T3),		/* X3 = h s */
	SUB(T0, T0, T1),
	MUL(T1, T2, T0),
	ADD(SZ, SZ, SZ),
	SUB(Y1, T1, SZ),		/* Y3 = w (B - h) - 2 RR */
	MUL(T0, Y2, Z1),
	SUB(T0, T0, Y1),		/* u = y2 Z1 - Y1 */
	MUL(T1, T0, T0),		/* uu */
	MUL(T2, X2, Z1),
	SUB(T2, T2, X1),		/* v = x2 Z1 - X1 */
	MUL(T3, T2, T2),		/* vv */
	MUL(SX, T2, T3),		/* vvv */
	MUL(SY, T3, X1),		/* R = vv X1 */
	MUL(T3, T1, Z1),
	SUB(T3, T3, SX),
	SUB(T3, T3, SY),
	SUB(T3, T3, SY),		/* A = uu Z1 - vvv - 2R */
	MUL(SZ, SX, Z1),		/* Z3 = vvv Z1 */
	MUL(T1, SX, Y1),		/* vvv Y1 */
	MUL(SX, T2, T3),		/* X3 = v A */
	SUB(SY, SY, T3),		/* R - A */
	MUL(T2, T0, SY),
	SUB(SY, T2, T1),		/* Y3 = u (R - A) - vvv Y1 */
	END
};

/* back to affine: x = X/Z, y = Y/Z, into the P slots */
static const uint8_t affine_prog[] = {
	INV,
	MUL(X2, X1, T0),
	MUL(Y2, Y1, T0),
	END
};

/*
 * ECDSA modulo n, with the nonce k in K, [k]G in X2 Y2, the message hash
 * z in X1 and the key in D: r = x mod n and s = (z + r d) / k, left in
 * X2 Y2.  x and z are only below 2^256 < 2n; they go in where fp_add()
 * and fp_mul() take such numbers.
 */
static const uint8_t ecdsa_prog[] = {
	ADD(Z1, K, ZERO),
	INV,				/* T0 = 1/k */
	MUL(T1, D, X2),			/* r d */
	ADD(T1, X1, T1),		/* z + r d */
	MUL(Y2, T0, T1),		/* s */
	ADD(X2, X2, ZERO),		/* r */
	END
};

/*
 * (X2, Y2) = [K] P for the point P = x || y at pt, by double-and-add on
 * K + n.  For K in [2^224, 2^256 - 2^224),
 * which is what keygen() draws, K + n has bit 256 set: R = P stands for
 * that bit, and for each of the 256 below it R = 2R, then R = R + P if
 * the bit is set - computed always and kept under a mask, so neither the
 * instructions executed nor the addresses touched depend on the scalar.
 *
 * The formulas fail only where the addition meets R = P, R = -P or the
 * point at infinity, that is where the bits of K + n read so far, m, have
 * 2m = 0 or +-1 modulo n.  With K + n in [2^256, 2n) m is never a
 * multiple of n and is (n +- 1)/2 only for the last step of K + n = 2n - 2
 * or 2n - 1, whose bit is clear, so that the failed sum is dropped.
 * (v31-trim's p256.c has the long form of this argument.)
 */
static void smult(const uint8_t *pt)
{
	unsigned i, j, keep;

	fm = W(ZERO);			/* a plain 256-bit addition */
	fp_add(W(KN), W(K), NMOD, 0, 0xff);
	fm = PMOD;
	memcpy(W(X1), pt, 2 * FE);
	memcpy(W(X2), pt, 2 * FE);
	memset(W(Z1), 0, FE);
	W(Z1)[FE - 1] = 1;
	for (i = 0; i < 8 * FE; i++) {
		keep = 0u - (W(KN)[i >> 3] >> (7 - (i & 7)) & 1);
		run(step_prog);
		for (j = 0; j < 3 * FE; j++)
			W(X1)[j] ^= (uint8_t)((W(X1)[j] ^ W(SX)[j]) & keep);
	}
	run(affine_prog);
}

/*
 * A key pair: a scalar in K and [K]G in X2 Y2.  The scalar is 32 random
 * bytes, drawn again while their first four are all zeros or all ones:
 * what is kept lies in [2^224, 2^256 - 2^224), below n, and is uniform
 * there - one part in 2^31 short of uniform over [1, n), and with no
 * fixed bits, which an ECDSA nonce must not have.
 */
static void keygen(void)
{
	do
		rng_bytes(W(K), FE);
	while ((uint32_t)(get32(W(K)) + 1) < 2);
	smult(base_point);
}

/* ---- SHA-256 (FIPS 180-4) ---- */

static uint32_t sha_k[64 + 8];	/* round constants, initial state */

/*
 * The round constants are the first 32 fractional bits of the cube roots
 * of the first 64 primes, the initial state those of the square roots of
 * the first 8.  They are computed here rather than stored (288 bytes),
 * with integers only and the field's arithmetic, a bit at a time from the
 * top, the way roots are extracted on paper.  With x the bits of the
 * root so far and c = prime * 8^k after k steps, the next bit is set if
 * (2x + 1)^3 <= c / 64; the roots are below 8, so 3 + 32 steps give
 * x = floor(root * 2^32).  To stay in integers the program keeps X = 4x:
 * X = 2X, then Z = X + 4, which is 4 (2x + 1), then the sign of c - Z^3,
 * then c = 8c; where the bit is set, X = Z.  For a square root the test
 * is (2x + 1)^2 <= c / 16 with c = prime * 4^k, the same Z.
 * Everything stays far below p, where reducing modulo p changes nothing,
 * except that a negative difference shows as a nonzero top byte.
 * SX is X, SY is c, T3 is 4.
 */
static const uint8_t root_prog[] = {
	ADD(SX, SX, SX),
	ADD(SZ, SX, T3),
	MUL(T1, SZ, SZ),
	MUL(T2, T1, SZ),
	SUB(T2, SY, T2),		/* c - Z^3 */
	ADD(SY, SY, SY),
	ADD(SY, SY, SY),
	ADD(SY, SY, SY),
	END,
	/* and for square roots, at root_prog + 17: */
	ADD(SX, SX, SX),
	ADD(SZ, SX, T3),
	MUL(T2, SZ, SZ),
	SUB(T2, SY, T2),		/* c - Z^2 */
	ADD(SY, SY, SY),
	ADD(SY, SY, SY),
	END,
	/* and to set the bit, at root_prog + 30: */
	ADD(SX, SZ, ZERO),
	END
};

static void sha_init(void)
{
	const uint8_t *prog = root_prog;
	unsigned q = 1, d, i, k;

	fm = PMOD;
	W(T3)[FE - 1] = 4;
	for (i = 0; i < 64 + 8; i++) {
		if (i == 64) {		/* now the square roots */
			q = 1;
			prog += 17;
		}
		do {			/* the next prime */
			q++;
			for (d = q - 1; q % d; d--)
				;
		} while (d > 1);
		memset(W(SX), 0, 2 * FE);
		W(SY)[FE - 2] = (uint8_t)(q >> 8);
		W(SY)[FE - 1] = (uint8_t)q;
		for (k = 0; k < 3 + 32; k++) {
			run(prog);
			if (!W(T2)[0])
				run(root_prog + 30);
		}
		/* X / 4: the fraction is bits 2..33 */
		sha_k[i] = get32(W(SX) + FE - 4) >> 2 |
			   (uint32_t)W(SX)[FE - 5] << 30;
	}
}

#define ROR(x, n) ((uint32_t)((x) >> (n) | (x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) ((((x) | (y)) & (z)) | ((x) & (y)))
/* the three rotations of each function as one chain:
 * ROR(x,a) ^ ROR(x,b) ^ ROR(x,c) = ROR(ROR(ROR(x, c-b) ^ x, b-a) ^ x, a) */
#define EP0(x) ROR(ROR(ROR(x, 9) ^ (x), 11) ^ (x), 2)
#define EP1(x) ROR(ROR(ROR(x, 14) ^ (x), 5) ^ (x), 6)
#define SIG0(x) (ROR(ROR(x, 11) ^ (x), 7) ^ ((x) >> 3))
#define SIG1(x) (ROR(ROR(x, 2) ^ (x), 17) ^ ((x) >> 10))

/*
 * out = SHA-256 of the len bytes at m, which are padded in place: every
 * message hashed here sits in a buffer with at least 72 bytes to spare
 * behind it, so there is no streaming state and no block buffer.  out may
 * be where the padding went.
 */
static void sha256(uint8_t *out, uint8_t *m, size_t len)
{
	uint8_t *end = m + ((len + 72) & ~(size_t)63);
	uint32_t st[8], w[64], v[64 + 8], *s, t1, t2;
	unsigned i;

	memset(m + len, 0, (size_t)(end - m) - len);
	m[len] = 0x80;
	put32(end - 4, (uint32_t)len << 3);
	memcpy(st, sha_k + 64, sizeof st);
	for (; m < end; m += 64) {
		for (i = 0; i < 16; i++)
			w[i] = get32(m + 4 * i);
		for (; i < 64; i++)
			w[i] = SIG1(w[i - 2]) + w[i - 7] + SIG0(w[i - 15]) +
			       w[i - 16];
		/* a..h are a window that slides down v: each round writes
		 * the new a below it and adds into d, the new e */
		s = v + 64;
		memcpy(s, st, sizeof st);
		for (i = 0; i < 64; i++, s--) {
			t1 = s[7] + EP1(s[4]) + CH(s[4], s[5], s[6]) +
			     sha_k[i] + w[i];
			t2 = EP0(s[0]) + MAJ(s[0], s[1], s[2]);
			s[3] += t1;
			s[-1] = t1 + t2;
		}
		for (i = 0; i < 8; i++)
			st[i] += v[i];
	}
	for (i = 0; i < 8; i++)
		put32(out + 4 * i, st[i]);
}

/* ---- chacha20-poly1305@openssh.com ----
 * A direction has a 64-byte key K_2 || K_1 and uses the packet's sequence
 * number as the nonce.  The packet length is encrypted with K_1, block 0;
 * the rest of the packet with K_2 from block 1; the Poly1305 key is the
 * first half of K_2's block 0, and the tag covers the encrypted length and
 * the encrypted rest. */

static uint32_t seq;	/* packets sent, which is also packets received
			 * when one is: the two sides take turns */

/* XOR the len bytes at buf with the keystream of key from block ctr. */
static void chacha(uint8_t *buf, size_t len, const uint8_t *key, uint32_t ctr)
{
	/* A quarter round is four steps a += b; d = rol(d ^ a, n) on the
	 * words a b c d, then c d a b, and so on.  Each entry packs the
	 * indices d:c:b:a of a column or diagonal; swapping its bytes
	 * swaps a/c and b/d.  The rotations 16, 12, 8, 7 are used up from
	 * the low end. */
	static const uint16_t qr[8] = {
		0xc840, 0xd951, 0xea62, 0xfb73,
		0xfa50, 0xcb61, 0xd872, 0xe943
	};
	uint32_t st[16], x[16], rot;
	uint8_t nonce[4];
	unsigned i, q, a;

	st[0] = 0x61707865;		/* "expand 32-byte k" */
	st[1] = 0x3320646e;
	st[2] = 0x79622d32;
	st[3] = 0x6b206574;
	for (i = 0; i < 8; i++)
		st[4 + i] = get32le(key + 4 * i);
	st[12] = ctr;
	st[13] = st[14] = 0;		/* 64-bit big-endian nonce */
	put32(nonce, seq);
	st[15] = get32le(nonce);
	while (len) {
		memcpy(x, st, sizeof x);
		for (i = 0; i < 80; i++) {
			q = qr[i & 7];
			for (rot = 0x07080c10; rot; rot >>= 8) {
				a = q & 15;
				x[a] += x[q >> 4 & 15];
				x[q >> 12] ^= x[a];
				x[q >> 12] = (uint32_t)(x[q >> 12] << (rot & 31) |
					     x[q >> 12] >> (32 - (rot & 31)));
				q = (q >> 8 | q << 8) & 0xffff;
			}
		}
		for (i = 0; i < 64 && len; i++, len--)
			*buf++ ^= (uint8_t)((x[i >> 2] + st[i >> 2]) >>
					    8 * (i & 3));
		st[12]++;
	}
}

/*
 * Write Poly1305(key, m) over the 16 bytes at tag and return 0 if that is
 * what was there, nonzero otherwise - without stopping at the first
 * difference.  h = (h + block) r modulo 2^130 - 5 for each 16-byte block
 * with a 1 above it, then tag = (h + s) mod 2^128.  Poly1305's numbers are
 * little-endian: byte i of one is byte FE-1-i of a slot.  X2 is h, Y2 is
 * r, K is the block.
 */
static unsigned poly1305(uint8_t *tag, const uint8_t *key, const uint8_t *m,
			 size_t len)
{
	uint8_t *h = W(X2), *r = W(Y2), *t = W(K);
	unsigned i, n, c = 0, diff = 0;

	fm = QMOD;
	memset(h, 0, 2 * FE);
	for (i = 0; i < 16; i++)
		r[FE - 1 - i] = key[i];
	for (i = 3; i < 16; i += 4) {	/* r &= 0x0ffffffc0ffffffc0ffffffc0fffffff */
		r[FE - 1 - i] &= 15;
		r[FE - 2 - i] &= 252;	/* (byte 16 is zero as it is) */
	}
	while (len) {
		n = len < 16 ? (unsigned)len : 16;
		memset(t, 0, FE);
		t[FE - 1 - n] = 1;
		for (i = 0; i < n; i++)
			t[FE - 1 - i] = *m++;
		fp_add(t, t, h, 0, 0xff);
		fp_mul(h, t, r);
		len -= n;
	}
	for (i = 0; i < 16; i++) {
		c += h[FE - 1 - i] + key[16 + i];
		diff |= tag[i] ^ (c & 0xff);
		tag[i] = (uint8_t)c;
		c >>= 8;
	}
	return diff;
}

/* ---- packets ---- */

static NANOSSH_JMP_BUF dead;

/* Give the connection up, from wherever that turns out to be necessary. */
static void die(void)
{
	NANOSSH_LONGJMP(dead);
}

/* Read (wr = 0) or write exactly n > 0 bytes. */
static void xio(int wr, uint8_t *b, size_t n)
{
	int r;

	do {
		r = wr ? net_write(b, (unsigned)n) : net_read(b, (unsigned)n);
		if (r <= 0)
			die();
		b += r;
		n -= (unsigned)r;
	} while (n);
}

/* Every packet is received into this buffer and sent from it.  A packet
 * is uint32 length, byte padding length, payload, padding, and once the
 * keys are in use a 16-byte tag; the length counts what is between it and
 * the tag.  The largest packet sent is a service request echoed, which
 * can come out 11 bytes longer than it came in. */
static uint8_t pkt[4 + MAXPKT + 12 + 16];
#define PAY (pkt + 5)

static uint8_t keys[4 * FE];	/* K_2, K_1 client to server; K_2, K_1 back */

/* Tag the packet (length field included) in place; see poly1305(). */
static unsigned tag(const uint8_t *key, uint32_t pktlen)
{
	uint8_t pk[32];

	memset(pk, 0, sizeof pk);
	chacha(pk, sizeof pk, key, 0);
	return poly1305(pkt + 4 + pktlen, pk, pkt, 4 + pktlen);
}

/* Send the packet whose payload, n bytes, is at PAY.  The length is the
 * smallest that leaves 4 bytes of padding and makes what is encrypted -
 * everything before the keys are in use, when nothing is, and all but the
 * length after - a multiple of 8.  The first three packets (KEXINIT, the
 * key exchange reply, NEWKEYS) go out in the clear, the rest encrypted. */
static void send_packet(size_t n)
{
	unsigned a = seq < 3 ? 4 : 0;
	uint32_t pktlen = (((uint32_t)n + 12 + a) & ~(uint32_t)7) - a;
	size_t total = 4 + pktlen;

	put32(pkt, pktlen);
	pkt[4] = (uint8_t)(pktlen - 1 - n);
	memset(PAY + n, 0, pktlen - 1 - n);
	if (!a) {
		chacha(pkt, 4, keys + 3 * FE, 0);
		chacha(pkt + 4, pktlen, keys + 2 * FE, 1);
		tag(keys + 2 * FE, pktlen);
		total += 16;
	}
	xio(1, pkt, total);
	seq++;
}

/* Receive a packet; returns the length of its payload, which is at PAY.
 * The client's fourth packet and all after it are encrypted. */
static size_t recv_packet(void)
{
	uint8_t len[4];
	uint32_t pktlen;
	unsigned enc = seq >= 3;

	xio(0, pkt, 4);
	memcpy(len, pkt, 4);
	if (enc)
		chacha(len, 4, keys + FE, 0);
	pktlen = get32(len);
	if (pktlen - 1 > MAXPKT - 1)
		die();
	xio(0, pkt + 4, pktlen + (enc ? 16 : 0));
	if (enc) {
		if (tag(keys, pktlen))
			die();
		chacha(pkt + 4, pktlen, keys, 1);
	}
	if (pkt[4] >= pktlen)
		die();
	return pktlen - 1 - pkt[4];
}

/* Write the 32-byte number at d as an mpint at b; returns its end.
 * Leading zero bytes go, but not the last byte, and a number with its top
 * bit set gets a zero byte in front. */
static uint8_t *put_mpint(uint8_t *b, const uint8_t *d)
{
	unsigned n = FE, z;

	while (n > 1 && !*d)
		d++, n--;
	z = *d >> 7;
	put32(b, n + z);
	b[4] = 0;
	memcpy(b + 4 + z, d, n);
	return b + 4 + z + n;
}

/* ---- the connection ---- */

/* Our version string as the exchange hash wants it, followed by the CR LF
 * that the version line wants. */
static const uint8_t version[4 + 15 + 2] = {
	0, 0, 0, 15, 'S', 'S', 'H', '-', '2', '.', '0', '-',
	'N', 'a', 'n', 'o', 'S', 'S', 'H', '\r', '\n'
};

/* Our KEXINIT behind its type byte and cookie: the name-lists for key
 * exchange, host key, cipher (each way), MAC (each way; empty, the cipher
 * authenticates), compression (each way), languages (each way; empty),
 * then no guessed packet and the reserved word. */
static const char kexinit[] =
	"\0\0\0\022ecdh-sha2-nistp256"
	"\0\0\0\023ecdsa-sha2-nistp256"
	"\0\0\0\035chacha20-poly1305@openssh.com"
	"\0\0\0\035chacha20-poly1305@openssh.com"
	"\0\0\0\0" "\0\0\0\0"
	"\0\0\0\004none" "\0\0\0\004none"
	"\0\0\0\0" "\0\0\0\0"
	"\0" "\0\0\0";		/* and the literal's own NUL */
#define KEXINIT_LEN (1 + 16 + sizeof kexinit)

/* The host key blob K_S = string "ecdsa-sha2-nistp256", string
 * "nistp256", string (04 || x || y), with its own length in front.  The
 * constant part is here; nanossh_init() adds x || y. */
static const uint8_t ks_head[4 + 23 + 12 + 5] = {
	0, 0, 0, 104,
	0, 0, 0, 19, 'e', 'c', 'd', 's', 'a', '-', 's', 'h', 'a', '2', '-',
	'n', 'i', 's', 't', 'p', '2', '5', '6',
	0, 0, 0, 8, 'n', 'i', 's', 't', 'p', '2', '5', '6',
	0, 0, 0, 65, 4
};
static uint8_t ks[sizeof ks_head + 2 * FE];

/* The one acceptable USERAUTH_REQUEST: user "user", service
 * "ssh-connection", method "password", no new password, "password123". */
static const uint8_t auth[55] = {
	50, 0, 0, 0, 4, 'u', 's', 'e', 'r',
	0, 0, 0, 14, 's', 's', 'h', '-', 'c', 'o', 'n', 'n', 'e', 'c', 't',
	'i', 'o', 'n',
	0, 0, 0, 8, 'p', 'a', 's', 's', 'w', 'o', 'r', 'd', 0,
	0, 0, 0, 11, 'p', 'a', 's', 's', 'w', 'o', 'r', 'd', '1', '2', '3'
};

/* What the session consists of: three messages to the client's channel,
 * each given here as its type, the number of bytes that follow the
 * channel number, and those bytes. */
static const uint8_t session[] = {
	91, 12,			/* CHANNEL_OPEN_CONFIRMATION: our channel 0, */
	0, 0, 0, 0, 0, 0, 0x80, 0, 0, 0, 0x40, 0,   /* window 32768, packets
						     * of up to 16384 */
	94, 17,			/* CHANNEL_DATA: a string */
	0, 0, 0, 13, 'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd',
	'\r', '\n',
	97, 0			/* CHANNEL_CLOSE */
};

/* The exchange hash covers V_C, V_S, I_C, I_S, K_S, Q_C, Q_S as strings
 * and K as an mpint.  They are collected here as they come up, each with
 * its length in front, so that the hash is one call; K and the hash H
 * stay at the end for the key derivation, which hashes K || H || 65 more
 * bytes, and SHA-256 wants up to 72 bytes for its padding. */
static uint8_t hb[(4 + 253) + sizeof version - 2 + (4 + MAXPKT) +
		  (4 + KEXINIT_LEN) + sizeof ks + 2 * (4 + 65) + (4 + 33) +
		  FE + (1 + FE) + 72];

static void serve(void)
{
	uint8_t *p, *q, *t, *h, *k, chan[4];
	const uint8_t *m;
	size_t n;
	unsigned i, diff;

	seq = 0;

	/* The ephemeral key pair, first: part of its public point is the
	 * cookie of our KEXINIT (random, ours, and public anyway). */
	keygen();

	/* Version exchange.  V_C is the client's line without its CR LF;
	 * the line is at most 255 bytes.  (A line with no CR in front of
	 * the LF, or of one byte, gives a V_C the client disagrees with.) */
	memcpy(pkt, version + 4, 17);
	xio(1, pkt, 17);
	p = hb + 4;
	do
		xio(0, p, 1);
	while (*p++ != '\n' && p < hb + 4 + 255);
	p -= 2;
	hb[3] = (uint8_t)(p - hb - 4);	/* the three bytes above stay 0 */
	memcpy(p, version, 19);
	p += 19;

	/* KEXINIT, the client's and then ours (each side sends its own
	 * without waiting for the other's). */
	n = recv_packet();
	put32(p, (uint32_t)n);
	memcpy(p + 4, PAY, n);
	p += 4 + n;
	q = PAY;
	*q++ = 20;
	memcpy(q, W(X2), 16);
	memcpy(q + 16, kexinit, sizeof kexinit);
	n = KEXINIT_LEN;
	put32(p, (uint32_t)n);
	memcpy(p + 4, PAY, n);
	p += 4 + n;
	send_packet(n);

	/* KEX_ECDH_INIT: byte 30, then the client's public point as string
	 * Q_C = 04 || x || y.  The transcript continues with K_S, Q_C and
	 * our Q_S; the shared secret K is the x of [k] Q_C.  The packet is
	 * taken for what it should be: a client that sends something else
	 * - another message, a point not on the curve - gets a hash it
	 * cannot verify, computed from a secret that is used only once. */
	recv_packet();
	t = p;
	memcpy(p, ks, sizeof ks);
	p += sizeof ks;
	memcpy(p, PAY + 1, 4 + 65);
	p += 4 + 65;
	memcpy(p, ks + sizeof ks_head - 5, 5);
	memcpy(p + 5, W(X2), 2 * FE);
	p += 4 + 65;
	smult(PAY + 6);
	h = put_mpint(p, W(X2));
	sha256(h, hb, (size_t)(h - hb));

	/* The signature over H: a nonce and its point, then SHA-256(H)
	 * where the ECDSA program wants it (the multiplication uses that
	 * slot, so not before). */
	keygen();
	sha256(W(X1), h, FE);
	fm = NMOD;
	run(ecdsa_prog);

	/* KEX_ECDH_REPLY: K_S, Q_S, and the signature blob, string
	 * "ecdsa-sha2-nistp256" then string (mpint r, mpint s), as a
	 * string.  The blob begins as K_S does - a length, the name, a
	 * length - so its first 31 bytes are copied from there, and the
	 * low bytes of the two lengths, which depend on r and s, are set
	 * afterwards (both are below 256). */
	q = PAY;
	*q++ = 31;
	memcpy(q, t, sizeof ks);
	q += sizeof ks;
	memcpy(q, t + sizeof ks + 4 + 65, 4 + 65);
	q += 4 + 65;
	t = q;
	memcpy(q, ks, 31);
	q = put_mpint(put_mpint(q + 31, W(X2)), W(Y2));
	t[3] = (uint8_t)(q - t - 4);
	t[30] = (uint8_t)(q - t - 31);
	send_packet((size_t)(q - PAY));

	/* NEWKEYS: the client's arrives as soon as it has our reply, and
	 * being a bare type byte it is sent straight back.  (From here on
	 * each packet is taken for what the protocol says comes next; a
	 * client that sends something else gets nowhere.) */
	recv_packet();
	send_packet(1);

	/* The keys (RFC 4253 7.2), 'C' for client to server and 'D' for
	 * server to client: K_2 = HASH(K || H || letter || session id) and
	 * K_1 = HASH(K || H || K_2).  The session id is H: there is no
	 * second key exchange.  The cipher needs nothing else. */
	n = (size_t)(h - p) + 2 * FE;
	for (k = keys; k < keys + sizeof keys; k += 2 * FE) {
		h[FE] = (uint8_t)(k == keys ? 'C' : 'D');
		memcpy(h + FE + 1, h, FE);
		sha256(k, p, n + 1);
		memcpy(h + FE, k, FE);
		sha256(k + FE, p, n);
	}

	/* SERVICE_REQUEST -> SERVICE_ACCEPT: the same payload, type + 1 */
	n = recv_packet();
	PAY[0]++;
	send_packet(n);

	/* USERAUTH_REQUEST: the payload is compared with the one that is
	 * acceptable as a whole, in constant time.  Anything else gets
	 * USERAUTH_FAILURE - methods that can continue: "password"; no
	 * partial success - whose body is part of that request too. */
	for (;;) {
		n = recv_packet();
		diff = (unsigned)(n ^ sizeof auth);
		for (i = 0; i < sizeof auth; i++)
			diff |= PAY[i] ^ auth[i];
		if (!diff)
			break;
		PAY[0] = 51;
		memcpy(PAY + 1, auth + 27, 13);
		send_packet(14);
	}
	PAY[0] = 52;			/* USERAUTH_SUCCESS */
	send_packet(1);

	/* CHANNEL_OPEN "session": byte 90, the string, then the client's
	 * channel number, which our messages have to carry: confirm, say
	 * hello, close. */
	recv_packet();
	memcpy(chan, PAY + 12, 4);
	for (m = session; m < session + sizeof session; m += 2 + m[1]) {
		PAY[0] = m[0];
		memcpy(PAY + 1, chan, 4);
		memcpy(PAY + 5, m + 2, m[1]);
		send_packet(5u + m[1]);
	}

	/* Then read and drop whatever the client still sends until it
	 * closes the connection: closing first could cost it the data. */
	while (net_read(pkt, sizeof pkt) > 0)
		;
}

/* ---- the interface (nanossh.h) ---- */

void nanossh_init(void)
{
	sha_init();
	keygen();
	memcpy(W(D), W(K), FE);
	memcpy(ks, ks_head, sizeof ks_head);
	memcpy(ks + sizeof ks_head, W(X2), 2 * FE);
}

void nanossh_serve(void)
{
	if (!NANOSSH_SETJMP(dead))
		serve();
}
