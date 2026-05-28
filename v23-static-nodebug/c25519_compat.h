/*
 * Curve25519 X25519 wrapper backed by the c25519 Montgomery ladder.
 *
 * Replaces the 20 KB curve25519-donna-c64 blob used by v19-donna with the
 * small public-domain c25519 ladder, which reuses the f25519 field
 * arithmetic already linked for Ed25519. The scalar is clamped on a copy so
 * the stored private key matches donna/libsodium semantics.
 */
#ifndef C25519_COMPAT_H
#define C25519_COMPAT_H

#include <stdint.h>
#include <string.h>
#include "c25519.h"

static inline int crypto_scalarmult_base(uint8_t *public_key, const uint8_t *private_key)
{
	uint8_t e[32];
	memcpy(e, private_key, 32);
	c25519_prepare(e);
	c25519_smult(public_key, c25519_base_x, e);
	return 0;
}

static inline int crypto_scalarmult(uint8_t *shared, const uint8_t *private_key, const uint8_t *peer_public)
{
	uint8_t e[32];
	memcpy(e, private_key, 32);
	c25519_prepare(e);
	c25519_smult(shared, peer_public, e);
	return 0;
}

#endif /* C25519_COMPAT_H */
