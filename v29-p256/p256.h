/*
 * p256.h - NIST P-256 (secp256r1) on the generic fp.c arithmetic, for
 * ecdh-sha2-nistp256 and ecdsa-sha2-nistp256.
 *
 * v28-chapoly needed Curve25519 for the key exchange, Ed25519 (the same
 * curve, other coordinates) for the host key, SHA-512 for the Ed25519
 * challenge and SHA-256 for everything else.  P-256 serves both the key
 * exchange and the signature in one coordinate system, and ECDSA hashes
 * with SHA-256, so the SHA-512 core, the Curve25519 field code with its
 * carry folding, the square root and the Montgomery<->Edwards mapping all
 * go; what is left is one modular multiplier (fp.c) and one short
 * Weierstrass double-and-add program run by a tiny interpreter.
 *
 * Points are affine x || y, 64 big-endian bytes - exactly the wire format
 * minus its 0x04 prefix, so peer points are used in place.
 */
#ifndef P256_H
#define P256_H

#include <stdint.h>

extern const uint8_t p256_p[32];      /* field prime */
extern const uint8_t p256_n[32];      /* group order */
extern const uint8_t p256_g[64];      /* base point x || y */

/* Scalars are 32 big-endian bytes with bit 255 set and bit 254 clear (see
 * p256.c for why that makes the double-and-add exception-free); the
 * caller draws 32 random bytes and calls p256_clamp() on them. */
static inline void p256_clamp(uint8_t *k)
{
	k[0] = (k[0] | 0x80) & 0xbf;
}

/* out = [k]P, affine.  out must not alias P. */
void p256_smult(uint8_t *out, const uint8_t *k, const uint8_t *P);

/* ECDSA over the 32-byte hash z with secret d and nonce k (clamped):
 * rs = r || s, two 32-byte big-endian integers. */
void ecdsa_sign(uint8_t *rs, const uint8_t *d, const uint8_t *k,
                const uint8_t *z);

#endif /* P256_H */
