/*
 * X25519 on top of the Edwards code that is already linked for Ed25519.
 *
 * v26-genk and earlier carried two independent scalar multipliers: the
 * c25519 Montgomery ladder (xc_double/xc_diffadd/c25519_smult, ~900 bytes)
 * for the key exchange, and the twisted-Edwards group law for signing.
 * Curve25519 and Ed25519 are the same group in different coordinates, so
 * the ladder is redundant: map the peer's Montgomery u-coordinate onto the
 * Edwards curve, reuse ed25519_smult(), and map back.
 *
 *     y = (u-1)/(u+1)        u = (1+y)/(1-y)
 *     x = sqrt((y^2-1)/(d*y^2+1))
 *
 * Only y matters for the result, and (x, y) and (-x, y) multiply to points
 * with the same y, so the square root's sign is irrelevant and is not
 * chosen.  A u-coordinate that lies on the quadratic twist has no square
 * root here; f25519_sqrt() then returns a value that is not a curve point
 * and the derived secret is wrong, so the handshake fails at the first MAC
 * check.  Real SSH clients only ever send curve points.
 */
#ifndef X25519_ED_H
#define X25519_ED_H

#include <stdint.h>

/* Both clamp private_key in place (RFC 7748), which is idempotent. */
int crypto_scalarmult_base(uint8_t *public_key, uint8_t *private_key);
int crypto_scalarmult(uint8_t *shared, uint8_t *private_key,
		      const uint8_t *peer_public);

#endif /* X25519_ED_H */
