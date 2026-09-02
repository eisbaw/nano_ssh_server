/* SHA512
 * Daniel Beer <dlbeer@gmail.com>, 22 Apr 2014
 *
 * This file is in the public domain.
 */

#include "sha512.h"
#include "fprime.h"

#if !defined(COMPACT_DISABLE_ED25519) || !defined(COMPACT_DISABLE_X25519_DERIVE)
/* FIPS 180-4 constants, generated at startup by sha_gentables() instead of
 * being stored as ~1 KB of rodata:
 *   K512[i] = first 64 fractional bits of cbrt(i-th prime), i = 0..79
 *   H512[j] = first 64 fractional bits of sqrt(j-th prime), j = 0..7
 * The SHA-256 K table and initial state are the top 32 bits of the same
 * values (see sha256_minimal.h), so one generator covers both hashes. */
struct sha512_state sha512_initial_state;
uint64_t sha512_kgen[80];

/* First 64 fractional bits of p^(1/e), e in {2,3}, p < 2^9: bit-by-bit
 * root extraction on a 3.64 fixed-point candidate x, accepting a bit iff
 * x^e <= p << 64e.  The powers are taken with fprime_mul() - the generic
 * shift-and-add multiplier already linked for Ed25519 and Poly1305 - under
 * the modulus 2^208, which exceeds every product (x < 2^67, x^3 < 2^201)
 * and so never reduces: a 256-bit multiply for no extra code. */
static uint8_t gen_mod[FPRIME_SIZE];	/* 2^208, set by sha_gentables() */

static uint64_t root_frac(unsigned p, int e)
{
	uint8_t x[FPRIME_SIZE], r[2][FPRIME_SIZE];
	uint64_t v;
	int bit, i, k;

	memset(x, 0, sizeof(x));
	for (bit = 66; bit >= 0; bit--) {
		const uint8_t *a = x;

		x[bit >> 3] |= 1 << (bit & 7);
		for (k = 1; k < e; k++) {
			fprime_mul(r[k & 1], a, x, gen_mod);
			a = r[k & 1];
		}
		/* a <= p << 64e ?  p occupies bytes 8e and 8e+1 of the target */
		for (i = FPRIME_SIZE - 1; i >= 0; i--) {
			unsigned t = i == 8 * e ? p & 255 : i == 8 * e + 1 ? p >> 8 : 0;

			if (a[i] != t) {
				if (a[i] > t)
					x[bit >> 3] ^= 1 << (bit & 7);
				break;
			}
		}
	}
	memcpy(&v, x, sizeof(v));
	return v;
}

void sha_gentables(void)
{
	unsigned p = 1;
	int i;

	gen_mod[26] = 1;
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
	return __builtin_bswap64(*(const u64a *)x);
}

static inline void store64(uint8_t *x, uint64_t v)
{
	*(u64a *)x = __builtin_bswap64(v);
}

static inline uint64_t rot64(uint64_t x, int bits)
{
	return (x >> bits) | (x << (64 - bits));
}

void sha512_block(struct sha512_state *s, const uint8_t *blk)
{
	uint64_t w[16], v[8];
	int i;

	for (i = 0; i < 16; i++) {
		w[i] = load64(blk);
		blk += 8;
	}

	/* Working variables a..h as one array: the per-round rotation is a
	 * shift of the array instead of seven moves. */
	memcpy(v, s->h, sizeof(v));

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
		const uint64_t S0 = rot64(v[0], 28) ^ rot64(v[0], 34) ^ rot64(v[0], 39);
		const uint64_t S1 = rot64(v[4], 14) ^ rot64(v[4], 18) ^ rot64(v[4], 41);
		const uint64_t ch = (v[4] & v[5]) ^ ((~v[4]) & v[6]);
		const uint64_t temp1 = v[7] + S1 + ch + sha512_kgen[i] + wi;
		const uint64_t maj = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
		const uint64_t temp2 = S0 + maj;
		int j;

		for (j = 7; j > 0; j--)
			v[j] = v[j - 1];
		v[4] += temp1;
		v[0] = temp1 + temp2;

		/* w[wrap(i)] becomes w[i + 16] */
		w[i & 15] = wi + s0 + wi7 + s1;
	}

	for (i = 0; i < 8; i++)
		s->h[i] += v[i];
}

/* One-shot hash of the single SHA-512 input this server ever computes: the
 * Ed25519 challenge R || A || M, three 32-byte strings.  96 bytes plus the
 * 0x80 marker and the 16-byte length field fit one 128-byte block, which
 * is assembled straight from the three inputs by one loop - no streaming
 * sha512_final()/sha512_get(), no intermediate 96-byte buffer. */
void sha512_3x32(uint8_t *out, const uint8_t *r, const uint8_t *a,
		 const uint8_t *m)
{
	struct sha512_state s;
	uint8_t blk[SHA512_BLOCK_SIZE];
	int i;

	memcpy(&s, &sha512_initial_state, sizeof(s));

	for (i = 0; i < SHA512_BLOCK_SIZE; i++)
		blk[i] = i < 32 ? r[i] : i < 64 ? a[i - 32] : i < 96 ? m[i - 64] :
			 i == 96 ? 0x80 : 0;
	blk[SHA512_BLOCK_SIZE - 2] = (SHA512_96_SIZE * 8) >> 8;   /* 96*8 = 0x300 */

	sha512_block(&s, blk);

	for (i = 0; i < 8; i++)
		store64(out + i * 8, s.h[i]);
}
#endif
