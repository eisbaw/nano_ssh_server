/* fp.c - see fp.h.  Big-endian: byte 0 is the most significant. */
#include "fp.h"

uint8_t fp_zero[FP_SIZE];
uint8_t fp_one[FP_SIZE];

void fp_select(uint8_t *dst, const uint8_t *zero, const uint8_t *one,
               uint8_t cond)
{
	const uint8_t mask = -cond;
	int i;

	for (i = 0; i < FP_SIZE; i++)
		dst[i] = zero[i] ^ (mask & (one[i] ^ zero[i]));
}

uint32_t fp_try_sub(uint8_t *x, const uint8_t *m, uint32_t hi)
{
	uint8_t t[FP_SIZE];
	int32_t c = 0;
	int i;

	for (i = FP_SIZE - 1; i >= 0; i--) {
		c += x[i] - m[i];
		t[i] = (uint8_t)c;
		c >>= 8;		/* 0, or -1 on borrow */
	}
	/* V >= m  iff  hi >= borrow  iff  hi + c >= 0 */
	c += (int32_t)hi;
	fp_select(x, x, t, c >= 0);
	return c >= 0 ? (uint32_t)c : 0;
}

void fp_addsub(uint8_t *d, const uint8_t *s1, const uint8_t *s2, int neg,
               const uint8_t *m)
{
	int32_t c = 0;
	int i;

	/* subtraction is s1 + (m - s2): never below zero, below 2m */
	for (i = FP_SIZE - 1; i >= 0; i--) {
		c += s1[i] + (neg ? m[i] - s2[i] : s2[i]);
		d[i] = (uint8_t)c;
		c >>= 8;
	}
	fp_try_sub(d, m, (uint32_t)c);
}

/* Shift-and-add over the bits of b, top down: r = 2r + (bit ? a : 0),
 * then at most two subtractions of m bring the sum (< 3m) back below m.
 * The addend is chosen with a cmov, so nothing depends on the bit. */
void fp_mul(uint8_t *r, const uint8_t *a, const uint8_t *b, const uint8_t *m)
{
	int i, j;

	memset(r, 0, FP_SIZE);
	for (i = 0; i < 256; i++) {
		const uint8_t *s = (b[i >> 3] >> (7 - (i & 7))) & 1 ? a : fp_zero;
		uint32_t c = 0;

		for (j = FP_SIZE - 1; j >= 0; j--) {
			c += 2 * r[j] + s[j];
			r[j] = (uint8_t)c;
			c >>= 8;
		}
		c = fp_try_sub(r, m, c);
		fp_try_sub(r, m, c);
	}
}

/* Fermat: x^(m-2).  Both exponents this server uses (p-2, n-2) have bit
 * 255 set, so the chain starts from x itself at bit 254; a clear bit
 * multiplies by one instead of skipping, so the loop has no branch. */
void fp_inv(uint8_t *r, const uint8_t *x, const uint8_t *m)
{
	uint8_t e[FP_SIZE], t[FP_SIZE];
	int i;

	memcpy(e, m, FP_SIZE);
	e[FP_SIZE - 1] -= 2;
	memcpy(r, x, FP_SIZE);
	for (i = 1; i < 256; i++) {
		fp_mul(t, r, r, m);
		fp_mul(r, t, (e[i >> 3] >> (7 - (i & 7))) & 1 ? x : fp_one, m);
	}
}
