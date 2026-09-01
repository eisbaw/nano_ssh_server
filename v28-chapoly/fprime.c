/* Arithmetic in prime fields
 * Daniel Beer <dlbeer@gmail.com>, 10 Jan 2014
 *
 * This file is in the public domain.
 */

#include "fprime.h"

#ifndef COMPACT_DISABLE_ED25519
#ifdef FULL_C25519_CODE
const uint8_t fprime_zero[FPRIME_SIZE] = {0};
const uint8_t fprime_one[FPRIME_SIZE] = {1};
#endif

static void raw_add(uint8_t *x, const uint8_t *p)
{
	uint16_t c = 0;
	int i;

	for (i = 0; i < FPRIME_SIZE; i++) {
		c += ((uint16_t)x[i]) + ((uint16_t)p[i]);
		x[i] = c;
		c >>= 8;
	}
}

void raw_try_sub(uint8_t *x, const uint8_t *p)
{
	uint8_t minusp[FPRIME_SIZE];
	uint16_t c = 0;
	int i;

	for (i = 0; i < FPRIME_SIZE; i++) {
		c = ((uint16_t)x[i]) - ((uint16_t)p[i]) - c;
		minusp[i] = c;
		c = (c >> 8) & 1;
	}

	fprime_select(x, minusp, x, c);
}

/* Warning: this function is variable-time */
static int prime_msb(const uint8_t *p)
{
	int i;
	uint8_t x;

	for (i = FPRIME_SIZE - 1; i >= 0; i--)
		if (p[i])
			break;

	x = p[i];
	i <<= 3;

	while (x) {
		x >>= 1;
		i++;
	}

	return i - 1;
}

/* Warning: this function may be variable-time in the argument n */
static void shift_n_bits(uint8_t *x, int n)
{
	uint16_t c = 0;
	int i;

	for (i = 0; i < FPRIME_SIZE; i++) {
		c |= ((uint16_t)x[i]) << n;
		x[i] = c;
		c >>= 8;
	}
}

#ifdef FULL_C25519_CODE
void fprime_load(uint8_t *x, uint32_t c)
{
	unsigned int i;

	for (i = 0; i < sizeof(c); i++) {
		x[i] = c;
		c >>= 8;
	}

	for (; i < FPRIME_SIZE; i++)
		x[i] = 0;
}
#endif

/* n = x mod modulus, x a little-endian byte string of len bytes: plain
 * shift-and-reduce over every bit.  n < modulus before each step, so one
 * conditional subtraction after the shift keeps it there.  (The original
 * preloaded the top bits to save a few iterations; at 512 bits it is not
 * worth the code.) */
void fprime_from_bytes(uint8_t *n,
		       const uint8_t *x, size_t len,
		       const uint8_t *modulus)
{
	int i;

	memset(n, 0, FPRIME_SIZE);

	for (i = (int)(len << 3) - 1; i >= 0; i--) {
		shift_n_bits(n, 1);
		n[0] |= (x[i >> 3] >> (i & 7)) & 1;
		raw_try_sub(n, modulus);
	}
}

#ifdef FULL_C25519_CODE
void fprime_normalize(uint8_t *x, const uint8_t *modulus)
{
	uint8_t n[FPRIME_SIZE];

	fprime_from_bytes(n, x, FPRIME_SIZE, modulus);
	fprime_copy(x, n);
}

uint8_t fprime_eq(const uint8_t *x, const uint8_t *y)
{
	uint8_t sum = 0;
	int i;

	for (i = 0; i < FPRIME_SIZE; i++)
		sum |= x[i] ^ y[i];

	sum |= (sum >> 4);
	sum |= (sum >> 2);
	sum |= (sum >> 1);

	return (sum ^ 1) & 1;
}
#endif
void fprime_select(uint8_t *dst,
		   const uint8_t *zero, const uint8_t *one,
		   uint8_t condition)
{
	const uint8_t mask = -condition;
	int i;

	for (i = 0; i < FPRIME_SIZE; i++)
		dst[i] = zero[i] ^ (mask & (one[i] ^ zero[i]));
}

void fprime_add(uint8_t *r, const uint8_t *a, const uint8_t *modulus)
{
	raw_add(r, a);
	raw_try_sub(r, modulus);
}

#ifdef FULL_C25519_CODE
void fprime_sub(uint8_t *r, const uint8_t *a, const uint8_t *modulus)
{
	raw_add(r, modulus);
	raw_try_sub(r, a);
	raw_try_sub(r, modulus);
}
#endif

/* Shift-and-add: r = 2r (+ a) mod modulus for each bit of b, top down.  A
 * clear bit adds fprime_zero instead of a: the operand is chosen with a
 * cmov and the same add runs either way, so there is no copy-then-select
 * and still nothing depends on the bit. */
static uint8_t fprime_zero[FPRIME_SIZE];

void fprime_mul(uint8_t *r, const uint8_t *a, const uint8_t *b,
		const uint8_t *modulus)
{
	int i;

	memset(r, 0, FPRIME_SIZE);

	for (i = prime_msb(modulus); i >= 0; i--) {
		const uint8_t bit = (b[i >> 3] >> (i & 7)) & 1;

		shift_n_bits(r, 1);
		raw_try_sub(r, modulus);
		fprime_add(r, bit ? a : fprime_zero, modulus);
	}
}

#ifdef FULL_C25519_CODE
void fprime_inv(uint8_t *r, const uint8_t *a, const uint8_t *modulus)
{
	uint8_t pm2[FPRIME_SIZE];
	uint16_t c = 2;
	int i;

	/* Compute (p-2) */
	fprime_copy(pm2, modulus);
	for (i = 0; i < FPRIME_SIZE; i++) {
		c = modulus[i] - c;
		pm2[i] = c;
		c >>= 8;
	}

	/* Binary exponentiation */
	fprime_load(r, 1);

	for (i = prime_msb(modulus); i >= 0; i--) {
		uint8_t r2[FPRIME_SIZE];

		fprime_mul(r2, r, r, modulus);

		if ((pm2[i >> 3] >> (i & 7)) & 1)
			fprime_mul(r, r2, a, modulus);
		else
			fprime_copy(r, r2);
	}
}
#endif
#endif
