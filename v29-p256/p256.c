/* p256.c - see p256.h. */
#include "p256.h"
#include "fp.h"

/* aligned(1): GCC would otherwise align these 32-byte-and-larger arrays
 * to 32 bytes, and the .rodata output section with them, which pads the
 * file after .text. */
#define BYTES __attribute__((aligned(1)))

/* p || n: an instruction picks its modulus as an offset into this */
const uint8_t BYTES p256_pn[64] = {
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xbc, 0xe6, 0xfa, 0xad, 0xa7, 0x17, 0x9e, 0x84,
	0xf3, 0xb9, 0xca, 0xc2, 0xfc, 0x63, 0x25, 0x51
};
const uint8_t BYTES p256_g[64] = {
	0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47,
	0xf8, 0xbc, 0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2,
	0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33, 0xa0,
	0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96,
	0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b,
	0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e, 0x16,
	0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31, 0x5e, 0xce,
	0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5
};

/*
 * One step of the double-and-add ladder as a program over a flat file of
 * 32-byte field elements, run by the interpreter run(): each field
 * operation costs two bytes instead of a ~17-byte call sequence (the
 * v27-onecurve idea, now for short Weierstrass projective coordinates).
 * The step computes R = 2R (dbl-1998-cmo-2, a = -3) in place - each
 * coordinate of R is overwritten only after its last use - and S = R + P
 * (add-1998-cmo-2 with Z2 = 1, P affine); the caller then keeps R or S
 * according to the scalar bit.  Two more programs on the same file
 * convert to affine and compute the ECDSA signature.
 *
 * Slots: 0-1 P (x,y)  2-4 R (X,Y,Z)  5-9 scratch  10-12 S  13-15 the
 * scalar k, the ECDSA secret d and hash z (p256.h), which no program
 * writes.  The S slots are scratch too until they are written, near the
 * end of the addition.
 * Instruction: { dst << 4 | src1, op << 6 | N | src2 }, op 0 = add, 1 =
 * sub (the op is fp_addsub()'s neg flag), 2 = mul, 3 = inv of src1 - the
 * sign bit says multiplier; N (0x20) is the offset of n behind p in
 * p256_pn, so it selects the modulus (13 slots, so four bits index one).
 * A first byte of 0xf0 or more ends a program: slot 15 (z) is never a
 * destination.  Every mul writes a slot distinct from its sources
 * (fp_mul() zeroes the destination first); add and sub may write a
 * source.
 */
#define OP_ADD(d, a, b)  (uint8_t)((d) << 4 | (a)), (uint8_t)(0u << 6 | (b))
#define OP_SUB(d, a, b)  (uint8_t)((d) << 4 | (a)), (uint8_t)(1u << 6 | (b))
#define OP_MUL(d, a, b)  (uint8_t)((d) << 4 | (a)), (uint8_t)(2u << 6 | (b))
#define OP_INV(d, a, b)  (uint8_t)((d) << 4 | (a)), (uint8_t)(3u << 6 | (b))
#define N 0x20
#define OP_END 0xf0

enum { X2, Y2, X1, Y1, Z1, T0, T1, T2, T3, T4, SX, SY, SZ, D, K, Z,
       NSLOTS };

uint8_t p256_w[NSLOTS][FP_SIZE];		/* the element file (bss) */
#define w  p256_w
#define wb ((uint8_t *)w)

static const uint8_t BYTES p256_step[] = {
	/* R = 2R */
	OP_MUL(T0, X1, X1),		/* XX                               */
	OP_MUL(T1, Z1, Z1),		/* ZZ                               */
	OP_SUB(T1, T0, T1),
	OP_ADD(T2, T1, T1),
	OP_ADD(T2, T2, T1),		/* w = 3(X^2 - Z^2) = 3X^2 + aZ^2   */
	OP_MUL(T3, Y1, Z1),
	OP_ADD(T3, T3, T3),		/* s = 2YZ                          */
	OP_MUL(T1, T3, T3),		/* ss                               */
	OP_MUL(Z1, T3, T1),		/* Z3 = s^3                         */
	OP_MUL(T1, Y1, T3),		/* R = Ys                           */
	OP_MUL(T4, T1, T1),		/* RR                               */
	OP_MUL(T0, X1, T1),
	OP_ADD(T0, T0, T0),		/* B = 2XR (a multiply costs no
					   more than an add here, so not
					   (X+R)^2 - X^2 - R^2)             */
	OP_MUL(T1, T2, T2),
	OP_SUB(T1, T1, T0),
	OP_SUB(T1, T1, T0),		/* h = w^2 - 2B                     */
	OP_MUL(X1, T1, T3),		/* X3 = hs                          */
	OP_SUB(T0, T0, T1),
	OP_MUL(T1, T2, T0),
	OP_ADD(T4, T4, T4),
	OP_SUB(Y1, T1, T4),		/* Y3 = w(B-h) - 2R^2               */
	/* S = R + P */
	OP_MUL(T0, Y2, Z1),
	OP_SUB(T0, T0, Y1),		/* u = y2 Z1 - Y1                   */
	OP_MUL(T1, T0, T0),		/* uu                               */
	OP_MUL(T2, X2, Z1),
	OP_SUB(T2, T2, X1),		/* v = x2 Z1 - X1                   */
	OP_MUL(T3, T2, T2),		/* vv                               */
	OP_MUL(T4, T2, T3),		/* vvv                              */
	OP_MUL(SY, T3, X1),		/* R = v^2 X1                       */
	OP_MUL(T3, T1, Z1),
	OP_SUB(T3, T3, T4),
	OP_SUB(T3, T3, SY),
	OP_SUB(T3, T3, SY),		/* A = u^2 Z1 - v^3 - 2R            */
	OP_MUL(SX, T2, T3),		/* X3 = vA                          */
	OP_SUB(SY, SY, T3),		/* R - A                            */
	OP_MUL(T1, T0, SY),		/* u(R - A)                         */
	OP_MUL(SY, T4, Y1),
	OP_SUB(SY, T1, SY),		/* Y3 = u(R-A) - v^3 Y1             */
	OP_MUL(SZ, T4, Z1),		/* Z3 = v^3 Z1                      */
	OP_END
};

/* affine: x = X/Z, y = Y/Z into the P slots, which hold [k]P from then
 * on; the inverse goes to a dead scratch slot.  Z - Z is p, which
 * fp_addsub() reduces to 0: the Z slot is left all zero (as .bss starts)
 * for the next multiplication, which only has to set its low byte. */
static const uint8_t BYTES p256_affine[] = {
	OP_INV(T0, Z1, 0),
	OP_MUL(X2, X1, T0),
	OP_MUL(Y2, Y1, T0),
	OP_SUB(Z1, Z1, Z1),
	OP_END
};

/*
 * ECDSA (k = P256_K), run mod n with [k]G in the P slots: r = x mod n,
 * s = k^-1 (z + r d).  x < p < 2n, so adding the zero p256_affine left in
 * the Z slot reduces it; r d takes the unreduced x, as fp_mul()'s second
 * operand may be any 256-bit value, and z is not reduced either:
 * fp_addsub() leaves z + r d below 2^256 and congruent mod n, which is
 * all the final multiply needs.
 */
static const uint8_t BYTES p256_ecdsa[] = {
	OP_MUL(T1, D, X2 | N),		/* r d                              */
	OP_ADD(T1, T1, Z | N),		/* z + r d                          */
	OP_INV(T2, K, N),		/* k^-1                             */
	OP_MUL(Y2, T2, T1 | N),		/* s                                */
	OP_ADD(X2, X2, Z1 | N),		/* r = x mod n                      */
	OP_END
};

/* Run the program at ip on the element file. */
static void run(const uint8_t *ip)
{
	for (; *ip < OP_END; ip += 2) {
		/* dst << 4 is already in place: one shift more scales it to
		 * a 32-byte slot offset */
		uint8_t *d = wb + ((ip[0] & 0xf0) << 1);
		const uint8_t *s1 = w[ip[0] & 15], *s2 = w[ip[1] & 15];

		fp_m = p256_pn + (ip[1] & N);
		if ((int8_t)ip[1] < 0) {
			if (ip[1] & 0x40)
				fp_inv(d, s1);
			else
				fp_mul(d, s1, s2);
		} else	/* fp_addsub's neg is 0 (add) or -1 (sub) */
			fp_addsub(d, s1, s2, -(ip[1] >> 6));
	}
}

/*
 * Double-and-add on K = k + n, which for k >= 2^224 > 2^256 - n has bit
 * 256 set: R = P stands for that bit, then for bits 255..0 of K: R = 2R,
 * R += P if the bit is set (a cmov over the three coordinates).  The
 * addition formula fails only for 2R = O or +-P, i.e. for a prefix m of K
 * (bits 256..j+1) with 2m = 0, n-1 or n+1 mod n, and the doubling only
 * for R = O.  With K in [2^256, 2n) the prefixes at bit 255 lie in
 * [1, 2^255), at bit 1 in [2^254, 2^255) and at bit 0 in [2^255, n),
 * so they are never a multiple of n, and the only prefix equal to
 * (n +- 1)/2 is (n-1)/2 at bit 1 for K in {2n-2, 2n-1} - where bit 1 is
 * clear, so the failed sum is discarded.  The ladder is exception-free.
 * Adding n instead of clamping keeps the scalar itself free of fixed bits
 * (see rand_scalar() in main.c).  K is formed with the modular adder under
 * a zero modulus, which makes it a plain adder wrapping at 2^256.
 */
void p256_smult(uint8_t *out, const uint8_t *P)
{
	uint8_t k[FP_SIZE];
	uint8_t b = 0;
	int i;

	fp_m = fp_zero;
	fp_add(k, w[K], p256_n);

	/* P, R = P are the first four slots: P twice; Z = 1 is zero but
	 * for its low byte (see p256_affine) */
	for (i = 0; i < X1 * FP_SIZE + 64; i++)
		wb[i] = P[i & 63];
	w[Z1][FP_SIZE - 1] = 1;

	/* bits 255..0; the byte-sized counter ends the loop by wrapping */
	do {
		/* bit b of K, big-endian, spread to a byte: shift it up to
		 * the sign and let an arithmetic shift smear it down */
		const uint8_t mask = (int8_t)(k[b >> 3] << (b & 7)) >> 7;
		unsigned j;

		run(p256_step);
		/* R = bit ? S : R; the three coordinates are contiguous */
		for (j = 0; j < 3 * FP_SIZE; j++)
			wb[X1 * FP_SIZE + j] ^= mask &
				(wb[X1 * FP_SIZE + j] ^ wb[SX * FP_SIZE + j]);
	} while (++b);

	run(p256_affine);
	*out = 4;
	memcpy(out + 1, w, 64);
}

/* r || s at p256_w[0]: [k]G, then p256_ecdsa on it (see p256.h) */
void ecdsa_sign(void)
{
	/* the 65-byte wire-form copy goes to the dead S slots (10-12) */
	p256_smult(w[SX], p256_g);
	run(p256_ecdsa);
}
