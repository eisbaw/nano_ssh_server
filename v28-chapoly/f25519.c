/* Arithmetic mod p = 2^255-19
 * Daniel Beer <dlbeer@gmail.com>, 5 Jan 2014
 *
 * This file is in the public domain.
 */

#include "f25519.h"
#include "fprime.h"

#ifdef FULL_C25519_CODE
const uint8_t f25519_zero[F25519_SIZE] = {0};
#endif
/* Set to 1 at startup by ed25519_gen(): 32 bytes of bss instead of 32
 * bytes of rodata. */
uint8_t f25519_one[F25519_SIZE];

/* Common tail of every operation: bit 255 of the result (also the top bit
 * of c, the last limb before masking) is 2^255 = 19 mod p, so mask it and
 * add 19 per set bit, propagating the carry. */
static void f25519_carry(uint8_t *r, uint32_t c)
{
	int i;

	r[31] &= 127;
	c = (c >> 7) * 19;

	for (i = 0; i < F25519_SIZE; i++) {
		c += r[i];
		r[i] = c;
		c >>= 8;
	}
}

void f25519_normalize(uint8_t *x)
{
	uint8_t p[F25519_SIZE];

	/* Fold bit 255 (and anything above it that arrived from outside)
	 * back in as 19s: x is then below 2^255 + 18 < 2p, so subtracting p
	 * once, if x is not below it, finishes the job.  p = 2^255 - 19 is
	 * built in place and the conditional subtraction is fprime.c's. */
	f25519_carry(x, x[31]);
	memset(p, 0xff, F25519_SIZE);
	p[0] = 0xed;
	p[31] = 0x7f;
	raw_try_sub(x, p);
}

uint8_t f25519_eq(const uint8_t *x, const uint8_t *y)
{
	uint8_t sum = 0;
	int i;

	for (i = 0; i < F25519_SIZE; i++)
		sum |= x[i] ^ y[i];

	sum |= (sum >> 4);
	sum |= (sum >> 2);
	sum |= (sum >> 1);

	return (sum ^ 1) & 1;
}

void f25519_select(uint8_t *dst,
		   const uint8_t *zero, const uint8_t *one,
		   uint8_t condition)
{
	const uint8_t mask = -condition;
	int i;

	for (i = 0; i < F25519_SIZE; i++)
		dst[i] = zero[i] ^ (mask & (one[i] ^ zero[i]));
}

/* r = a + b (neg = 0) or a - b (neg = 1).  Subtraction is a + (2p - b) to
 * avoid underflow, with 2p = 2^256 - 38 = 0xff..ffda fed in limb by limb;
 * the running sum is signed so a limb's borrow simply propagates.  The
 * sum is below 4p < 2^257, so the carry-out and bit 255 together are what
 * f25519_carry() folds back in as multiples of 19. */
void f25519_addsub(uint8_t *r, const uint8_t *a, const uint8_t *b, int neg)
{
	int32_t c = 0;
	int i;

	for (i = 0; i < F25519_SIZE; i++) {
		int32_t bi = b[i];

		if (neg)
			bi = (i ? 255 : 218) - bi;
		c += a[i] + bi;
		r[i] = (uint8_t)c;
		c >>= 8;
	}
	f25519_carry(r, (uint32_t)c << 8 | r[31]);
}

void f25519_neg(uint8_t *r, const uint8_t *a)
{
	uint32_t c = 0;
	int i;

	/* Calculate 2p - a, to avoid underflow */
	c = 218;
	for (i = 0; i + 1 < F25519_SIZE; i++) {
		c += 65280 - ((uint32_t)a[i]);
		r[i] = c;
		c >>= 8;
	}

	c -= ((uint32_t)a[31]);
	r[31] = c;
	f25519_carry(r, c);
}

void f25519_mul__distinct(uint8_t *r, const uint8_t *a, const uint8_t *b)
{
	uint32_t c = 0;
	int i;

	for (i = 0; i < F25519_SIZE; i++) {
		int j;

		c >>= 8;
		for (j = 0; j <= i; j++)
			c += ((uint32_t)a[j]) * ((uint32_t)b[i - j]);

		for (; j < F25519_SIZE; j++)
			c += ((uint32_t)a[j]) *
			     ((uint32_t)b[i + F25519_SIZE - j]) * 38;

		r[i] = c;
	}

	f25519_carry(r, c);
}

#ifdef FULL_C25519_CODE
void f25519_mul(uint8_t *r, const uint8_t *a, const uint8_t *b)
{
	uint8_t tmp[F25519_SIZE];

	f25519_mul__distinct(tmp, a, b);
	f25519_copy(r, tmp);
}
#endif

/* r = x^e for e = 2^(top+1) - 2^low + pat: every bit from `low` up to
 * `top` is set and the bits below come from pat.  Both exponents in this
 * file have that shape, so one square-and-multiply loop serves the inverse
 * and the square root.  A zero bit multiplies by one instead of skipping
 * the multiply: constant time, and no branch or copy in the loop. */
static void f25519_pow(uint8_t *r, const uint8_t *x, int top, int low,
		       unsigned pat)
{
	uint8_t s[F25519_SIZE];
	int i;

	f25519_copy(r, x);
	for (i = top - 1; i >= 0; i--) {
		f25519_mul__distinct(s, r, r);
		f25519_mul__distinct(r, s,
				     i >= low || (pat >> i) & 1 ? x : f25519_one);
	}
}

/* Inverse by Fermat: x^(p-2), p-2 = 2^255-21 = bits 5..254 set + 0b01011 */
void f25519_inv__distinct(uint8_t *r, const uint8_t *x)
{
	f25519_pow(r, x, 254, 5, 11);
}

void f25519_sqrt(uint8_t *r, const uint8_t *a)
{
	uint8_t v[F25519_SIZE];
	uint8_t i[F25519_SIZE];
	uint8_t x[F25519_SIZE];
	uint8_t y[F25519_SIZE];

	/* v = (2a)^((p-5)/8) [x = 2a]; (p-5)/8 = 2^252-3 = bits 2..251 + 0b01 */
	f25519_add(x, a, a);
	f25519_pow(v, x, 251, 2, 1);

	/* i = 2av^2 - 1 */
	f25519_mul__distinct(y, v, v);
	f25519_mul__distinct(i, x, y);
	f25519_sub(i, i, f25519_one);

	/* r = avi */
	f25519_mul__distinct(x, v, a);
	f25519_mul__distinct(r, x, i);
}
