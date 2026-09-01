/* Edwards curve signature system
 * Daniel Beer <dlbeer@gmail.com>, 22 Apr 2014
 *
 * This file is in the public domain.
 */

#ifndef EDSIGN_H_
#define EDSIGN_H_

#ifndef COMPACT_DISABLE_ED25519
#include <stdint.h>
#include <stddef.h>

/* This is the Ed25519 signature system, as described in:
 *
 *     Daniel J. Bernstein, Niels Duif, Tanja Lange, Peter Schwabe, Bo-Yin
 *     Yang. High-speed high-security signatures. Journal of Cryptographic
 *     Engineering 2 (2012), 77-89. Document ID:
 *     a1a62a2f76d23f65d622484ddd09caf8. URL:
 *     http://cr.yp.to/papers.html#ed25519. Date: 2011.09.26.
 *
 * Signatures are wire-compatible with RFC 8032 and verify against any
 * Ed25519 verifier, but the key is not an RFC 8032 seed: it is the secret
 * scalar itself, drawn at random rather than expanded from a seed with
 * SHA-512 (see edsign.c).  Signing is therefore randomised, not
 * deterministic, and there is no seed to export.
 */

/* Secret key: one scalar mod L. Public key: a packed Edwards point. */
#define EDSIGN_SECRET_KEY_SIZE  32
#define EDSIGN_PUBLIC_KEY_SIZE  32
#define EDSIGN_SIGNATURE_SIZE   64

/* Generate a fresh keypair. */
void edsign_keygen(uint8_t *pub, uint8_t *secret);

/* Sign a 32-byte message (the SSH exchange hash). */
void edsign_sign(uint8_t *signature, const uint8_t *pub,
		 const uint8_t *secret, const uint8_t *message);

#endif
#endif
