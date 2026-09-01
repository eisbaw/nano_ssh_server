/* Edwards curve operations
 * Daniel Beer <dlbeer@gmail.com>, 9 Jan 2014
 *
 * This file is in the public domain.
 */

#include "ed25519.h"

#ifndef COMPACT_DISABLE_ED25519

/* Base point is (numbers wrapped):
 *
 *     x = 151122213495354007725011514095885315114
 *         54012693041857206046113283949847762202
 *     y = 463168356949264781694283940034751631413
 *         07993866256225615783033603165251855960
 *
 * y is derived by transforming the original Montgomery base (u=9). x
 * is the corresponding positive coordinate for the new curve equation.
 * t is x*y.
 */
/* Both constant points, and the addition-law constant k = 2d, are built at
 * startup by ed25519_gen() rather than stored:
 *   k       = 2d, one field addition
 *   base    y = 0x58 then 31 x 0x66 (little-endian 4/5 mod p), z = 1,
 *             t = x*y; only the base x coordinate has no short derivation
 *             and stays as data (recovering it from y costs more code in
 *             sign fixing than the 32 bytes it would save)
 */
static const uint8_t ed25519_base_x[F25519_SIZE] = {
	0x1a, 0xd5, 0x25, 0x8f, 0x60, 0x2d, 0x56, 0xc9,
	0xb2, 0xa7, 0x25, 0x95, 0x60, 0xc7, 0x2c, 0x69,
	0x5c, 0xdc, 0xd6, 0xfd, 0x31, 0xe2, 0xa4, 0xc0,
	0xfe, 0x53, 0x6e, 0xcd, 0xd3, 0x36, 0x69, 0x21
};

struct ed25519_pt ed25519_base;
/* d = -(121665/121666). Deriving this from 121665/121666 at startup was
 * tried and cost ~30 bytes more in f25519_load/f25519_neg than the 32 bytes
 * of table it removes, so it stays as data. */
const uint8_t ed25519_d[F25519_SIZE] = {
	0xa3, 0x78, 0x59, 0x13, 0xca, 0x4d, 0xeb, 0x75,
	0xab, 0xd8, 0x41, 0x41, 0x4d, 0x0a, 0x70, 0x00,
	0x98, 0xe8, 0x79, 0x77, 0x79, 0x40, 0xc7, 0x8c,
	0x73, 0xfe, 0x6f, 0x2b, 0xee, 0x6c, 0x03, 0x52
};

static uint8_t ed25519_k[F25519_SIZE];   /* k = 2d, for the addition law */

void ed25519_gen(void)
{
	f25519_one[0] = 1;
	f25519_add(ed25519_k, ed25519_d, ed25519_d);

	/* base point: y = 4/5, x stored */
	memset(ed25519_base.y, 0x66, F25519_SIZE);
	ed25519_base.y[0] = 0x58;
	memcpy(ed25519_base.x, ed25519_base_x, F25519_SIZE);
	ed25519_base.z[0] = 1;
	f25519_mul__distinct(ed25519_base.t, ed25519_base.x, ed25519_base.y);

}

/* Recover a point from its y coordinate: x = sqrt((y^2-1)/(d*y^2+1)).
 * Used to map Montgomery u-coordinates in for X25519, where (x, y) and
 * (-x, y) are equally good, so the sign of the root is left as it falls.
 *
 * If y does not correspond to a curve point (a peer u-coordinate on the
 * quadratic twist), x^2 is a non-residue and the result is not on the
 * curve; the derived secret is then simply wrong and the handshake fails.
 */
void ed25519_from_y(struct ed25519_pt *p, const uint8_t *y)
{
	uint8_t a[F25519_SIZE];
	uint8_t b[F25519_SIZE];
	uint8_t c[F25519_SIZE];
	uint8_t x[F25519_SIZE];

	f25519_mul__distinct(c, y, y);          /* y^2            */
	f25519_mul__distinct(a, c, ed25519_d);
	f25519_add(a, a, f25519_one);           /* d*y^2 + 1      */
	f25519_inv__distinct(b, a);
	f25519_sub(a, c, f25519_one);           /* y^2 - 1        */
	f25519_mul__distinct(c, a, b);
	f25519_sqrt(x, c);

	/* project: (x, y, x*y, 1) */
	f25519_copy(p->x, x);
	f25519_copy(p->y, y);
	f25519_copy(p->z, f25519_one);
	f25519_mul__distinct(p->t, x, y);
}

/* The addition law, as a tiny program over a flat file of 32-byte field
 * elements.  Written out as straight-line C, the 18 field operations cost
 * ~17 bytes each just to load three pointers and call; encoded as three
 * bytes per instruction and run by the loop below, they cost three.
 *
 * Slots:  0-3 P1 (x,y,t,z)   4-7 P2   8 k=2d   9-16 a,b,c,d,e,f,g,h
 *        17-20 result (x,y,t,z) - laid out to match struct ed25519_pt, so
 *        the operands and the result copy in and out as whole structs.
 *
 * Instruction: { op << 5 | dst, src1, src2 }, op 0 = sub, 1 = add, 2 = mul.
 * Every mul has a destination distinct from its sources, as
 * f25519_mul__distinct() requires.
 */
#define ED_SUB(d, x, y)  (uint8_t)(0u << 5 | (d)), (x), (y)
#define ED_ADD(d, x, y)  (uint8_t)(1u << 5 | (d)), (x), (y)
#define ED_MUL(d, x, y)  (uint8_t)(2u << 5 | (d)), (x), (y)

static const uint8_t ed25519_add_prog[] = {
	ED_SUB(11,  1,  0),   /* c = Y1-X1            */
	ED_SUB(12,  5,  4),   /* d = Y2-X2            */
	ED_MUL( 9, 11, 12),   /* A = (Y1-X1)(Y2-X2)   */
	ED_ADD(11,  1,  0),   /* c = Y1+X1            */
	ED_ADD(12,  5,  4),   /* d = Y2+X2            */
	ED_MUL(10, 11, 12),   /* B = (Y1+X1)(Y2+X2)   */
	ED_MUL(12,  2,  6),   /* d = T1 T2            */
	ED_MUL(11, 12,  8),   /* C = T1 k T2          */
	ED_MUL(12,  3,  7),   /* d = Z1 Z2            */
	ED_ADD(12, 12, 12),   /* D = 2 Z1 Z2          */
	ED_SUB(13, 10,  9),   /* E = B - A            */
	ED_SUB(14, 12, 11),   /* F = D - C            */
	ED_ADD(15, 12, 11),   /* G = D + C            */
	ED_ADD(16, 10,  9),   /* H = B + A            */
	ED_MUL(17, 13, 14),   /* X3 = E F             */
	ED_MUL(18, 15, 16),   /* Y3 = G H             */
	ED_MUL(19, 13, 16),   /* T3 = E H             */
	ED_MUL(20, 14, 15)    /* Z3 = F G             */
};

void ed25519_add(struct ed25519_pt *r,
		 const struct ed25519_pt *p1, const struct ed25519_pt *p2)
{
	/* Explicit formulas database: add-2008-hwcd-3
	 *
	 * source 2008 Hisil--Wong--Carter--Dawson,
	 *     http://eprint.iacr.org/2008/522, Section 3.1
	 * appliesto extended-1
	 * parameter k
	 * assume k = 2 d
	 *
	 * On this curve (a = -1, d a non-square) these formulas are complete:
	 * Z3 = F*G = 4(Z1^2*Z2^2 - d^2*T1^2*T2^2), which can only vanish if
	 * +/-d is a square, so there is no exceptional case - including
	 * P1 == P2.  v27-onecurve therefore drops the dedicated doubling
	 * routine (dbl-2008-hwcd) and doubles with ed25519_add(r, p, p),
	 * trading two extra field multiplies per bit for ~400 bytes of code.
	 */
	uint8_t w[21][F25519_SIZE];
	const uint8_t *ip;

	memcpy(w[0], p1, sizeof(*p1));
	memcpy(w[4], p2, sizeof(*p2));
	memcpy(w[8], ed25519_k, F25519_SIZE);

	for (ip = ed25519_add_prog;
	     ip != ed25519_add_prog + sizeof(ed25519_add_prog); ip += 3) {
		uint8_t *d = w[ip[0] & 31];
		const uint8_t *s1 = w[ip[1]];
		const uint8_t *s2 = w[ip[2]];

		if (ip[0] >> 5 == 2)
			f25519_mul__distinct(d, s1, s2);
		else
			f25519_addsub(d, s1, s2, !(ip[0] >> 5));
	}

	memcpy(r, w[17], sizeof(*r));
}

void ed25519_smult(struct ed25519_pt *r_out, const struct ed25519_pt *p,
		   const uint8_t *e)
{
	struct ed25519_pt r;
	uint8_t *rb = (uint8_t *)&r;
	int i;

	/* r = neutral (0, 1, 0, 1) */
	memset(&r, 0, sizeof(r));
	r.y[0] = 1;
	r.z[0] = 1;

	for (i = 255; i >= 0; i--) {
		const uint8_t mask = -((e[i >> 3] >> (i & 7)) & 1);
		const uint8_t *sb;
		struct ed25519_pt s;
		unsigned j;

		ed25519_add(&r, &r, &r);
		ed25519_add(&s, &r, p);

		/* Constant-time r = bit ? s : r. The four coordinates are
		 * contiguous, so one byte loop replaces four f25519_select()
		 * calls. */
		sb = (const uint8_t *)&s;
		for (j = 0; j < sizeof(r); j++)
			rb[j] ^= mask & (rb[j] ^ sb[j]);
	}

	ed25519_copy(r_out, &r);
}
#endif
