/* fp.c - see fp.h.  Big-endian: byte 0 is the most significant. */
#include "fp.h"

uint8_t fp_zero[FP_SIZE];
uint8_t fp_one[FP_SIZE];
const uint8_t *fp_m;

/* One pass computes s1 + s2 - m (in [-m, m)) or s1 + ~s2 + 1, which is
 * s1 - s2 + 2^256 (in (2^256 - m, 2^256 + m)); in both cases the final
 * carry, less the 1 the negation put in, is -1 exactly when the true value
 * s1 + s2 - m or s1 - s2 is negative, and then m is added back.  The
 * masks depend on the data, the branches do not. */
void fp_addsub(uint8_t *d, const uint8_t *s1, const uint8_t *s2, int neg)
{
	const uint8_t *m = fp_m;
	const unsigned nm = neg & 0xff;		/* 0 or 0xff */
	int32_t c = -neg, mask;
	int i;

	for (i = FP_SIZE - 1; i >= 0; i--) {
		c += s1[i] + (s2[i] ^ nm) - (m[i] & ~nm);
		d[i] = (uint8_t)c;
		c >>= 8;
	}
	mask = c + neg;
	c = 0;
	for (i = FP_SIZE - 1; i >= 0; i--) {
		c += d[i] + (m[i] & mask);
		d[i] = (uint8_t)c;
		c >>= 8;
	}
}

/* Double-and-add over the bits of b, top down: r = 2r + (bit ? a : 0),
 * each step a modular addition, so the reduction lives in fp_addsub() only.
 * The addend is chosen with a cmov, so nothing depends on the bit.  The sum
 * is read through acc, which points at fp_zero until the first step has
 * written r, so r needs no clearing; i runs from -256 so the loop test is
 * a plain inc/jne and the byte index (i >> 3) + 32 is (i >> 3) & 31, which
 * needs no sign extension. */
void fp_mul(uint8_t *r, const uint8_t *a, const uint8_t *b)
{
	const uint8_t *acc = fp_zero;
	int i;

	for (i = -256; i; i++) {
		const uint8_t *s = (b[(i >> 3) & 31] << (i & 7)) & 0x80 ?
				   a : fp_zero;

		fp_add(r, acc, acc);
		fp_add(r, r, s);
		acc = r;
	}
}

/* Fermat: x^(m-2) by square-and-multiply over the bits of m - 2, which is
 * m with its last byte lowered by 2 (the low byte is >= 2 for p and n), so
 * the exponent is read from the modulus itself and the 2 taken off the
 * byte while it is in a register.  sq is the square of the result so far
 * and points at fp_one before the first step, so the result needs no
 * initialisation; a clear bit multiplies by one instead of skipping, so
 * the loop has no data-dependent branch. */
/* noinline: its one caller is the p256.c interpreter loop, which would
 * otherwise spill its decoded operands around the inlined body */
__attribute__((noinline))
void fp_inv(uint8_t *r, const uint8_t *x)
{
	static uint8_t t[FP_SIZE];	/* static: no frame, no rsp copies */
	const uint8_t *sq = fp_one;
	int i;

	for (i = -256; i; i++) {
		unsigned e = fp_m[FP_SIZE + (i >> 3)];

		if ((i >> 3) == -1)	/* the last byte */
			e -= 2;
		fp_mul(r, sq, (e << (i & 7)) & 0x80 ? x : fp_one);
		fp_mul(t, r, r);
		sq = t;
	}
}
