/* Edwards curve signature system
 * Daniel Beer <dlbeer@gmail.com>, 22 Apr 2014
 *
 * This file is in the public domain.
 *
 * Reduced for v27-onecurve.  RFC 8032 derives the secret scalar and the
 * per-signature nonce by hashing a 32-byte seed with SHA-512, and hashes
 * the message with a prefix block to get the challenge.  Only the challenge
 * is part of the verification equation
 *
 *     [s]B = R + [H(R || A || M)]A
 *
 * so a verifier cannot distinguish a key/nonce derived that way from one
 * drawn straight out of getrandom(2).  Doing the latter deletes the key
 * expansion, the nonce derivation and the whole prefixed-hash block
 * machinery; what remains is one 96-byte hash, because the message here is
 * always the 32-byte SSH exchange hash.
 *
 * Verification (edsign_verify) is gone too: the server only ever signs.
 */

#include "ed25519.h"

#ifndef COMPACT_DISABLE_ED25519
#include "sha512.h"
#include "fprime.h"
#include "edsign.h"
#include "random_minimal.h"

/* Group order L = 2^252 + 27742317777372353535851937790883648493 */
static const uint8_t ed25519_order[FPRIME_SIZE] = {
	0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
	0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
};

/* Uniform scalar mod L: 64 random bytes reduced, so the bias is negligible */
static void rand_scalar(uint8_t *s)
{
	uint8_t r[SHA512_HASH_SIZE];

	randombytes_buf(r, sizeof(r));
	fprime_from_bytes(s, r, sizeof(r), ed25519_order);
}

/* r = pack([k]B): the compressed point is y with the low bit of x in the
 * top bit.  Unprojecting and packing are fused here so the coordinates are
 * normalized once, and ed25519_unproject()/ed25519_pack() disappear. */
static void sm_pack(uint8_t *r, const uint8_t *k)
{
	struct ed25519_pt p;
	uint8_t zinv[F25519_SIZE];
	uint8_t x[F25519_SIZE];

	ed25519_smult(&p, &ed25519_base, k);

	f25519_inv__distinct(zinv, p.z);
	f25519_mul__distinct(x, p.x, zinv);
	f25519_mul__distinct(r, p.y, zinv);
	f25519_normalize(x);
	f25519_normalize(r);
	r[31] |= (x[0] & 1) << 7;
}

void edsign_keygen(uint8_t *pub, uint8_t *secret)
{
	rand_scalar(secret);
	sm_pack(pub, secret);
}

/* z = H(R || A || M) mod L: exactly 96 bytes, i.e. one padded SHA-512 block. */
static void challenge(uint8_t *z, const uint8_t *r, const uint8_t *a,
		      const uint8_t *m)
{
	uint8_t b[SHA512_96_SIZE];
	uint8_t h[SHA512_HASH_SIZE];

	memcpy(b, r, 32);
	memcpy(b + 32, a, 32);
	memcpy(b + 64, m, 32);
	sha512_96(h, b);

	fprime_from_bytes(z, h, sizeof(h), ed25519_order);
}

void edsign_sign(uint8_t *signature, const uint8_t *pub,
		 const uint8_t *secret, const uint8_t *message)
{
	uint8_t k[FPRIME_SIZE];
	uint8_t z[FPRIME_SIZE];
	uint8_t s[FPRIME_SIZE];

	/* R = [k]B for a fresh random k */
	rand_scalar(k);
	sm_pack(signature, k);

	/* s = k + z*a mod L */
	challenge(z, signature, pub, message);
	fprime_mul(s, z, secret, ed25519_order);
	fprime_add(s, k, ed25519_order);
	memcpy(signature + 32, s, FPRIME_SIZE);
}
#endif
