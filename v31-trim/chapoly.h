/*
 * chacha20-poly1305@openssh.com for v31-trim.
 *
 * ChaCha20 uses one add/XOR/rotate step for all four steps of a quarter
 * round, on a per-key state the key derivation writes the key into.
 * Poly1305: h = (h + block_i || 0x01) * r mod 2^130-5 over 16-byte blocks
 * on the generic fp.c arithmetic with a third modulus, one two-instruction
 * interpreter program per block (p256.c); fp.c is big-endian (every other
 * number in this server is), and Poly1305's numbers are little-endian, so
 * the key and the blocks are copied in byte-reversed (lecopy()) and the
 * tag is read from the far end of the element.
 *
 * OpenSSH framing (PROTOCOL.chacha20poly1305): 64-byte key = K_2 (bytes
 * 0..31, payload) || K_1 (32..63, length).  Nonce = 8-byte big-endian packet
 * sequence number.  Length is encrypted with K_1 at counter 0, payload with
 * K_2 from counter 1, Poly1305 one-time key = first 32 bytes of the K_2
 * counter-0 block, tag over encrypted length || encrypted payload.
 */
#ifndef CHAPOLY_H
#define CHAPOLY_H

#include <stdint.h>
#include "nolibc.h"
#include "fp.h"
#include "p256.h"

#define ROTL32(v, n) (((v) << (n)) | ((v) >> (32 - (n))))

/* XOR the ChaCha20 keystream for (key, nonce = seq, block counter ctr) into
 * buf.  st is the key's state, which the key derivation writes the key
 * words of: this fills in the constant, the counter and the nonce (words
 * 13 and 14 stay zero from bss) and uses it in place.  The state words are
 * native little-endian, as the target is x86-64. */
static void chacha_xor(uint32_t *st, uint8_t *buf, uint32_t seq,
                       uint32_t ctr, size_t len)
{
	uint32_t x[16];
	size_t n;
	int i;

	/* "expand 32-byte k" as two immediates: no rodata, no call */
	*(u64a *)st = 0x3320646e61707865;
	*(u64a *)(st + 2) = 0x6b20657479622d32;
	st[12] = ctr;
	st[15] = __builtin_bswap32(seq);

	/* at least one block: every caller has a length (and for none, the
	 * XOR loop below does nothing) */
	do {
		n = len < 64 ? len : 64;
		memcpy(x, st, 64);
		/* Each word packs d:c:b:a as four nibbles. The first four
		 * quarter-rounds are columns; the next four are diagonals.
		 * Swapping the bytes exchanges a/c and b/d, so one body does
		 * a += b; d = rol(d ^ a), then c += d; b = rol(b ^ c).
		 * The four rotations are consumed low byte first: 16,12,8,7.
		 * After four swaps q is back where it started. */
		static const uint16_t qr[8] = {
			0xc840, 0xd951, 0xea62, 0xfb73,
			0xfa50, 0xcb61, 0xd872, 0xe943
		};
		/* 80 quarter-rounds = 20 full rounds, with an inc/jne loop. */
		for (i = -80; i; i++) {
			uint16_t q = qr[i & 7];
			uint32_t rotations = 0x07080c10;

			/* opaque to GCC, which would otherwise count the four
			 * steps in a second register instead of ending on the
			 * flags of the shift */
			__asm__("" : "+r"(rotations));
			do {
				unsigned a = q & 15, b = (q >> 4) & 15, d = q >> 12;
				unsigned rot = (uint8_t)rotations;
				x[a] += x[b];
				x[d] = ROTL32(x[d] ^ x[a], rot);
				q = (q >> 8) | (q << 8);
			} while (rotations >>= 8);
		}
		/* keystream word = x + st, XORed in a word at a time: every
		 * length here is a multiple of 4 (4, 32, and packet lengths,
		 * which are 4 mod 8).  A malformed received length would only
		 * XOR up to 3 bytes into the already verified tag behind it.
		 * Index words directly: no signed byte-index / 4 conversion. */
		for (unsigned j = 0; j * 4 < n; j++)
			((u32a *)buf)[j] ^= x[j] + st[j];
		buf += n;
		st[12]++;
	} while (len -= n);
}

/* Zero the z bytes at d, then copy n > 0 bytes from *s into them from the
 * last one down: a little-endian number into the low end of a big-endian
 * element.  *s advances past the copy; returns the byte below it.  The
 * copy loads forwards and stores backwards, flipping the direction flag
 * around each store (it is clear again at the end, as the ABI wants). */
static inline __attribute__((always_inline)) uint8_t *
lecopy(uint8_t *d, size_t z, const uint8_t **s, size_t n)
{
	unsigned a = 0;
	__asm__ volatile ("rep stosb\n\t"
			  "dec %0\n\t"
			  "mov %4, %1\n"
			  "1:\tlodsb\n\t"
			  "std\n\t"
			  "stosb\n\t"
			  "cld\n\t"
			  "loop 1b"
			  : "+D"(d), "+c"(z), "+S"(*s), "+a"(a)
			  : "r"(n) : "memory");
	return d;
}

/* tag = Poly1305(key, m), returning nonzero if the tag it overwrites was
 * different - an OR of XORs, so the time does not depend on where.
 * Little-endian numbers on the big-endian fp elements: LE byte i lives at
 * element byte 31 - i. */
static unsigned poly1305(uint8_t *tag, const uint8_t *key,
                     const uint8_t *m, size_t len)
{
	uint8_t *r = POLY_R, *h = POLY_H;
	unsigned c = 0;
	uint8_t d = 0;

	/* h and r are adjacent slots: h = 0, r = the key's first 16 bytes */
	const uint8_t *k0 = key;
	lecopy(h, 2 * FP_SIZE, &k0, 16);
	/* clamp: LE r[3,7,11,15] &= 15, r[4,8,12] &= 252 (the last pair's
	 * &= 252 lands on r[15], which is zero anyway); the counter is
	 * opaque so the loop ends on the flags of its sub */
	long k = 16;
	__asm__("" : "+r"(k));
	do {
		r[k + 12] &= 15;
		r[k + 11] &= 252;
	} while (k -= 4);

	while (len) {
		size_t n = len < 16 ? len : 16;

		/* the block, and the 1 above it */
		*lecopy(POLY_T, FP_SIZE, &m, n) = 1;
		p256_run(p256_poly);		/* h = (t + h) * r   */
		len -= n;
	}

	/* tag = (h + s) mod 2^128, LE, over the old one; j runs up to 0 and
	 * every index is a fixed offset from it */
	long j = -16;
	__asm__("" : "+r"(j));
	do {
		c += h[15 - j] + key[32 + j];
		d |= tag[16 + j] ^ (uint8_t)c;
		tag[16 + j] = (uint8_t)c;
		c >>= 8;
	} while (++j);
	return d;
}

#endif /* CHAPOLY_H */
