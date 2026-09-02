/*
 * chacha20-poly1305@openssh.com for v29-p256.
 *
 * ChaCha20 as in v28-chapoly.  Poly1305 is h = (h + block_i || 0x01) * r
 * mod 2^130-5 over 16-byte blocks on the generic fp.c arithmetic with a
 * second modulus; fp.c is big-endian now (every other number in this
 * server is), and Poly1305's numbers are little-endian, so the key, the
 * blocks and the tag are indexed from the far end of the element - which
 * costs nothing, the fill loops just count the other way.
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

#define ROTL32(v, n) (((v) << (n)) | ((v) >> (32 - (n))))

/* XOR the ChaCha20 keystream for (key, nonce = seq, block counter ctr) into
 * buf.  The state words are native little-endian, as the target is x86-64. */
static void chacha_xor(const uint8_t *key, uint32_t seq, uint32_t ctr,
                       uint8_t *buf, size_t len)
{
	uint32_t st[16], x[16];
	int i;

	memcpy(st, "expand 32-byte k", 16);
	memcpy(st + 4, key, 32);
	st[12] = ctr;
	st[13] = 0;
	st[14] = 0;
	st[15] = __builtin_bswap32(seq);

	while (len) {
		size_t n = len < 64 ? len : 64;

		memcpy(x, st, 64);
		/* 10 double rounds: four column quarter-rounds (s = 0) then
		 * four diagonal ones (s = 1); the operand indices follow from
		 * the column a and the diagonal shift s. */
		for (i = 0; i < 80; i++) {
			int a = i & 3, s = (i >> 2) & 1;
			int b = 4 + ((a + s) & 3), c = 8 + ((a + 2 * s) & 3),
			    d = 12 + ((a + 3 * s) & 3);

			x[a] += x[b]; x[d] = ROTL32(x[d] ^ x[a], 16);
			x[c] += x[d]; x[b] = ROTL32(x[b] ^ x[c], 12);
			x[a] += x[b]; x[d] = ROTL32(x[d] ^ x[a], 8);
			x[c] += x[d]; x[b] = ROTL32(x[b] ^ x[c], 7);
		}
		for (i = 0; i < 16; i++)
			x[i] += st[i];
		for (i = 0; (size_t)i < n; i++)
			buf[i] ^= ((uint8_t *)x)[i];
		buf += n;
		len -= n;
		st[12]++;
	}
}

/* tag = Poly1305(key, m).  Little-endian numbers on the big-endian fp
 * elements: LE byte i lives at element byte 31 - i. */
static void poly1305(uint8_t *tag, const uint8_t *key,
                     const uint8_t *m, size_t len)
{
	uint8_t p[FP_SIZE], r[FP_SIZE], h[FP_SIZE], t[FP_SIZE];
	unsigned c = 0;
	int i;

	for (i = 0; i < FP_SIZE; i++) {
		p[i] = i > 14 ? 0xff : 0;	/* 2^130-5 = 03 || ff x 15 || fb */
		r[i] = i > 15 ? key[31 - i] : 0;
		h[i] = 0;
	}
	p[15] = 3;
	p[31] = 0xfb;
	/* clamp: LE r[3,7,11,15] &= 15, r[4,8,12] &= 252 (the last pair's
	 * &= 252 lands on r[15], which is zero anyway) */
	for (i = 28; i > 12; i -= 4) {
		r[i] &= 15;
		r[i - 1] &= 252;
	}

	while (len) {
		size_t n = len < 16 ? len : 16;

		for (i = 0; i < FP_SIZE; i++) {
			size_t j = 31 - i;

			t[i] = j < n ? m[j] : j == n;
		}
		fp_add(t, t, h, p);		/* t = block + h      */
		fp_mul(h, t, r, p);		/* h = t * r mod p    */
		m += n;
		len -= n;
	}

	for (i = 0; i < 16; i++) {
		c += h[31 - i] + key[16 + i];
		tag[i] = (uint8_t)c;
		c >>= 8;
	}
}

#endif /* CHAPOLY_H */
