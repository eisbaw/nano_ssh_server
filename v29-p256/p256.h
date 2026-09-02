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

extern const uint8_t p256_pn[64];     /* field prime || group order */
#define p256_p p256_pn
#define p256_n (p256_pn + 32)
extern const uint8_t p256_g[64];      /* base point x || y */

/* Scalars are 32 big-endian bytes below n, drawn uniformly by the caller
 * (see p256.c for how the ladder avoids its exceptional cases without
 * constraining them). */

/* The interpreter's file of field elements (p256.c).  Its last three
 * slots, which the scalar multiplication never touches, are written
 * directly by the caller: the scalar k of every multiplication (the host
 * secret, the ephemeral key, the nonce), the ECDSA secret d and hash z. */
extern uint8_t p256_w[16][32];
#define P256_D p256_w[13]
#define P256_K p256_w[14]
#define P256_Z p256_w[15]

/* out = [P256_K]P in wire form, 0x04 || x || y (65 bytes: a copy of
 * p256_w[0..1], where the affine point is computed, behind the prefix). */
void p256_smult(uint8_t *out, const uint8_t *P);

/* ECDSA over P256_Z with secret P256_D and nonce P256_K:
 * r || s, two 32-byte big-endian integers, at p256_w[0]. */
void ecdsa_sign(void);

#endif /* P256_H */
