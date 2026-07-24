/* SHA512
 * Daniel Beer <dlbeer@gmail.com>, 22 Apr 2014
 *
 * This file is in the public domain.
 */

#include "sha512.h"

#if !defined(COMPACT_DISABLE_ED25519) || !defined(COMPACT_DISABLE_X25519_DERIVE)
/* FIPS 180-4 constants, generated at startup by sha_gentables() instead of
 * being stored as ~1 KB of rodata:
 *   K512[i] = first 64 fractional bits of cbrt(i-th prime), i = 0..79
 *   H512[j] = first 64 fractional bits of sqrt(j-th prime), j = 0..7
 * The SHA-256 K table and initial state are the top 32 bits of the same
 * values (see sha256_minimal.h), so one generator covers both hashes. */
struct sha512_state sha512_initial_state;
uint64_t sha512_kgen[80];

typedef unsigned __int128 u128;

/* r = a * m (256-bit x 64-bit -> 256-bit), returns carry-out */
static uint64_t mul256_64(uint64_t r[4], const uint64_t a[4], uint64_t m)
{
	u128 c = 0;
	int i;

	for (i = 0; i < 4; i++) {
		c += (u128)a[i] * m;
		r[i] = (uint64_t)c;
		c >>= 64;
	}
	return (uint64_t)c;
}

/* First 64 fractional bits of p^(1/e), e in {2,3}, p < 2^9.
 * Bit-by-bit root extraction on a 64.64 fixed-point candidate:
 * accept a bit iff cand^e <= p << 64*e, compared in 256-bit precision
 * (the shifted target is p in limb index e, zero elsewhere). */
static uint64_t root_frac(uint64_t p, int e)
{
	u128 x = 0;
	int bit, i, k;

	for (bit = 66; bit >= 0; bit--) {
		u128 cand = x | ((u128)1 << bit);
		uint64_t r[4] = { (uint64_t)cand, (uint64_t)(cand >> 64), 0, 0 };
		uint64_t ovf = 0;
		int gt;

		for (k = 1; k < e; k++) {
			uint64_t lo[4], hi[4];
			u128 c = 0;

			ovf |= mul256_64(lo, r, (uint64_t)cand);
			ovf |= mul256_64(hi, r, (uint64_t)(cand >> 64));
			ovf |= hi[3];
			r[0] = lo[0];
			for (i = 1; i < 4; i++) {
				c += (u128)lo[i] + hi[i - 1];
				r[i] = (uint64_t)c;
				c >>= 64;
			}
			ovf |= (uint64_t)c;
		}
		gt = ovf != 0;
		for (i = 3; !gt && i >= 0; i--) {
			uint64_t t = (i == e) ? p : 0;
			if (r[i] != t) {
				gt = r[i] > t;
				break;
			}
		}
		if (!gt)
			x = cand;
	}
	return (uint64_t)x;
}

void sha_gentables(void)
{
	unsigned p = 1;
	int i;

	for (i = 0; i < 80; i++) {
		unsigned d;

		do {
			p++;
			for (d = 2; d * d <= p && p % d; d++)
				;
		} while (d * d <= p);
		sha512_kgen[i] = root_frac(p, 3);
		if (i < 8)
			sha512_initial_state.h[i] = root_frac(p, 2);
	}
}

static inline uint64_t load64(const uint8_t *x)
{
	uint64_t r;

	r = *(x++);
	r = (r << 8) | *(x++);
	r = (r << 8) | *(x++);
	r = (r << 8) | *(x++);
	r = (r << 8) | *(x++);
	r = (r << 8) | *(x++);
	r = (r << 8) | *(x++);
	r = (r << 8) | *(x++);

	return r;
}

static inline void store64(uint8_t *x, uint64_t v)
{
	x += 7;
	*(x--) = v;
	v >>= 8;
	*(x--) = v;
	v >>= 8;
	*(x--) = v;
	v >>= 8;
	*(x--) = v;
	v >>= 8;
	*(x--) = v;
	v >>= 8;
	*(x--) = v;
	v >>= 8;
	*(x--) = v;
	v >>= 8;
	*(x--) = v;
}

static inline uint64_t rot64(uint64_t x, int bits)
{
	return (x >> bits) | (x << (64 - bits));
}

void sha512_block(struct sha512_state *s, const uint8_t *blk)
{
	uint64_t w[16];
	uint64_t a, b, c, d, e, f, g, h;
	int i;

	for (i = 0; i < 16; i++) {
		w[i] = load64(blk);
		blk += 8;
	}

	/* Load state */
	a = s->h[0];
	b = s->h[1];
	c = s->h[2];
	d = s->h[3];
	e = s->h[4];
	f = s->h[5];
	g = s->h[6];
	h = s->h[7];

	for (i = 0; i < 80; i++) {
		/* Compute value of w[i + 16]. w[wrap(i)] is currently w[i] */
		const uint64_t wi = w[i & 15];
		const uint64_t wi15 = w[(i + 1) & 15];
		const uint64_t wi2 = w[(i + 14) & 15];
		const uint64_t wi7 = w[(i + 9) & 15];
		const uint64_t s0 =
			rot64(wi15, 1) ^ rot64(wi15, 8) ^ (wi15 >> 7);
		const uint64_t s1 =
			rot64(wi2, 19) ^ rot64(wi2, 61) ^ (wi2 >> 6);

		/* Round calculations */
		const uint64_t S0 = rot64(a, 28) ^ rot64(a, 34) ^ rot64(a, 39);
		const uint64_t S1 = rot64(e, 14) ^ rot64(e, 18) ^ rot64(e, 41);
		const uint64_t ch = (e & f) ^ ((~e) & g);
		const uint64_t temp1 = h + S1 + ch + sha512_kgen[i] + wi;
		const uint64_t maj = (a & b) ^ (a & c) ^ (b & c);
		const uint64_t temp2 = S0 + maj;

		/* Update round state */
		h = g;
		g = f;
		f = e;
		e = d + temp1;
		d = c;
		c = b;
		b = a;
		a = temp1 + temp2;

		/* w[wrap(i)] becomes w[i + 16] */
		w[i & 15] = wi + s0 + wi7 + s1;
	}

	/* Store state */
	s->h[0] += a;
	s->h[1] += b;
	s->h[2] += c;
	s->h[3] += d;
	s->h[4] += e;
	s->h[5] += f;
	s->h[6] += g;
	s->h[7] += h;
}

/* One-shot hash of the single SHA-512 input this server ever computes: the
 * 96-byte Ed25519 challenge R || A || M.  96 bytes plus the 0x80 marker and
 * the 16-byte length field still fit one 128-byte block, so the streaming
 * sha512_final()/sha512_get() pair (and the second, conditional
 * sha512_block() call site that GCC was inlining a full copy of) are gone.
 */
void sha512_96(uint8_t *out, const uint8_t *in)
{
	struct sha512_state s;
	uint8_t blk[SHA512_BLOCK_SIZE];
	int i;

	memcpy(&s, &sha512_initial_state, sizeof(s));

	memset(blk, 0, sizeof(blk));
	memcpy(blk, in, SHA512_96_SIZE);
	blk[SHA512_96_SIZE] = 0x80;
	blk[SHA512_BLOCK_SIZE - 2] = (SHA512_96_SIZE * 8) >> 8;   /* 96*8 = 0x300 */

	sha512_block(&s, blk);

	for (i = 0; i < 8; i++)
		store64(out + i * 8, s.h[i]);
}
#endif
