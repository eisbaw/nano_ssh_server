/* p256.c - see p256.h. */
#include "p256.h"
#include "fp.h"

const uint8_t p256_p[32] = {
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};
const uint8_t p256_n[32] = {
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xbc, 0xe6, 0xfa, 0xad, 0xa7, 0x17, 0x9e, 0x84,
	0xf3, 0xb9, 0xca, 0xc2, 0xfc, 0x63, 0x25, 0x51
};
const uint8_t p256_g[64] = {
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
 * 32-byte field elements, run by the interpreter in p256_smult(): each
 * field operation costs three bytes instead of a ~17-byte call sequence
 * (the v27-onecurve idea, now for short Weierstrass projective
 * coordinates).  The step computes D = 2R (dbl-1998-cmo-2, a = -3) and
 * S = D + P (add-1998-cmo-2 with Z2 = 1, P affine); the caller then keeps
 * D or S according to the scalar bit.
 *
 * Slots: 0-2 R (X,Y,Z)  3-4 P (x,y)  5-16 scratch  17-19 D  20-22 S.
 * Instruction: { op << 5 | dst, src1, src2 }, op 0 = sub, 1 = add, 2 = mul.
 * Every mul writes a slot distinct from its sources (fp_mul() zeroes the
 * destination first).
 */
#define OP_SUB(d, a, b)  (uint8_t)(0u << 5 | (d)), (a), (b)
#define OP_ADD(d, a, b)  (uint8_t)(1u << 5 | (d)), (a), (b)
#define OP_MUL(d, a, b)  (uint8_t)(2u << 5 | (d)), (a), (b)

enum { X1, Y1, Z1, X2, Y2, XX, ZZ, T, W, S, SS, R, RR, B, B2, H, T2,
       DX, DY, DZ, SX, SY, SZ, NSLOTS };
/* the addition half reuses the scratch slots under other names */
enum { U = XX, UU = ZZ, V = T, VV = W, VVV = S, A = SS, T3 = R, T4 = RR,
       Q = B };

static const uint8_t p256_step[] = {
	/* D = 2R */
	OP_MUL(XX, X1, X1),
	OP_MUL(ZZ, Z1, Z1),
	OP_SUB(T,  XX, ZZ),
	OP_ADD(W,  T,  T),
	OP_ADD(W,  W,  T),		/* w = 3(X^2 - Z^2) = 3X^2 + aZ^2   */
	OP_MUL(S,  Y1, Z1),
	OP_ADD(S,  S,  S),		/* s = 2YZ                          */
	OP_MUL(SS, S,  S),
	OP_MUL(DZ, S,  SS),		/* Z3 = s^3                         */
	OP_MUL(R,  Y1, S),		/* R = Ys                           */
	OP_MUL(RR, R,  R),
	OP_ADD(B,  X1, R),
	OP_MUL(B2, B,  B),
	OP_SUB(B2, B2, XX),
	OP_SUB(B2, B2, RR),		/* B = (X+R)^2 - X^2 - R^2 = 2XR    */
	OP_MUL(H,  W,  W),
	OP_SUB(H,  H,  B2),
	OP_SUB(H,  H,  B2),		/* h = w^2 - 2B                     */
	OP_MUL(DX, H,  S),		/* X3 = hs                          */
	OP_SUB(T,  B2, H),
	OP_MUL(T2, W,  T),
	OP_ADD(RR, RR, RR),
	OP_SUB(DY, T2, RR),		/* Y3 = w(B-h) - 2R^2               */
	/* S = D + P */
	OP_MUL(U,  Y2, DZ),
	OP_SUB(U,  U,  DY),		/* u = y2 Z1 - Y1                   */
	OP_MUL(UU, U,  U),
	OP_MUL(V,  X2, DZ),
	OP_SUB(V,  V,  DX),		/* v = x2 Z1 - X1                   */
	OP_MUL(VV, V,  V),
	OP_MUL(VVV, V, VV),
	OP_MUL(T3, VV, DX),		/* R = v^2 X1                       */
	OP_MUL(A,  UU, DZ),
	OP_SUB(A,  A,  VVV),
	OP_SUB(A,  A,  T3),
	OP_SUB(A,  A,  T3),		/* A = u^2 Z1 - v^3 - 2R            */
	OP_MUL(SX, V,  A),		/* X3 = vA                          */
	OP_SUB(T4, T3, A),
	OP_MUL(Q,  U,  T4),
	OP_MUL(T4, VVV, DY),
	OP_SUB(SY, Q,  T4),		/* Y3 = u(R-A) - v^3 Y1             */
	OP_MUL(SZ, VVV, DZ)		/* Z3 = v^3 Z1                      */
};

/*
 * R = P, then for bits 254..0: R = 2R, R += P if the bit is set (a cmov
 * over the three coordinates).  With bit 255 set and bit 254 clear the
 * prefix m of the scalar processed so far satisfies 1 <= m < 2^255 and
 * m != (n +/- 1)/2, so 2mP is never P, -P or the point at infinity and the
 * affine addition formula never hits its exceptional cases; P itself is a
 * point of prime order n, so 2R never meets the doubling's exception (Y = 0)
 * either.  No point at infinity is ever represented.
 */
void p256_smult(uint8_t *out, const uint8_t *k, const uint8_t *P)
{
	uint8_t w[NSLOTS][FP_SIZE], *wb = (uint8_t *)w;
	uint8_t zi[FP_SIZE];
	int i;

	memcpy(w[X1], P, 64);
	memcpy(w[Z1], fp_one, FP_SIZE);
	memcpy(w[X2], P, 64);

	for (i = 1; i < 256; i++) {
		const uint8_t *ip;
		const uint8_t mask = -((k[i >> 3] >> (7 - (i & 7))) & 1);
		unsigned j;

		for (ip = p256_step; ip != p256_step + sizeof(p256_step);
		     ip += 3) {
			uint8_t *d = w[ip[0] & 31];
			const uint8_t *s1 = w[ip[1]], *s2 = w[ip[2]];

			if (ip[0] >> 5 == 2)
				fp_mul(d, s1, s2, p256_p);
			else
				fp_addsub(d, s1, s2, !(ip[0] >> 5), p256_p);
		}
		/* R = bit ? S : D; the three coordinates are contiguous */
		for (j = 0; j < 3 * FP_SIZE; j++)
			wb[X1 * FP_SIZE + j] = wb[DX * FP_SIZE + j] ^
				(mask & (wb[DX * FP_SIZE + j] ^ wb[SX * FP_SIZE + j]));
	}

	/* affine: x = X/Z, y = Y/Z */
	fp_inv(zi, w[Z1], p256_p);
	fp_mul(out, w[X1], zi, p256_p);
	fp_mul(out + FP_SIZE, w[Y1], zi, p256_p);
}

/* r = ([k]G).x mod n,  s = k^-1 (z + r d) mod n */
void ecdsa_sign(uint8_t *rs, const uint8_t *d, const uint8_t *k,
                const uint8_t *z)
{
	uint8_t R[64], t[FP_SIZE], zz[FP_SIZE], ki[FP_SIZE];

	p256_smult(R, k, p256_g);
	memcpy(rs, R, FP_SIZE);
	fp_try_sub(rs, p256_n, 0);		/* x < p < 2n: one subtraction  */
	fp_mul(t, rs, d, p256_n);		/* r d                          */
	memcpy(zz, z, FP_SIZE);
	fp_try_sub(zz, p256_n, 0);		/* z < 2^256 < 2n               */
	fp_add(t, t, zz, p256_n);		/* z + r d                      */
	fp_inv(ki, k, p256_n);
	fp_mul(rs + FP_SIZE, ki, t, p256_n);
}
