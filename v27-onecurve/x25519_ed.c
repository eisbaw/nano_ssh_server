/* X25519 expressed with the Edwards group law - see x25519_ed.h. */

#include "x25519_ed.h"
#include "ed25519.h"
#include "nolibc.h"

/* p->u, where u = (1+y)/(1-y) and y = Y/Z, i.e. u = (Z+Y)/(Z-Y).
 * Working straight from the projective coordinates keeps this to a single
 * inversion (unprojecting first would need two). */
static void ed_to_u(uint8_t *u, const struct ed25519_pt *p)
{
	uint8_t a[F25519_SIZE];
	uint8_t b[F25519_SIZE];
	uint8_t c[F25519_SIZE];

	f25519_add(a, p->z, p->y);
	f25519_sub(b, p->z, p->y);
	f25519_inv__distinct(c, b);
	f25519_mul__distinct(u, a, c);
	f25519_normalize(u);
}

/* u->p: y = (u-1)/(u+1), then ed25519_from_y() recovers x. */
static void ed_from_u(struct ed25519_pt *p, const uint8_t *u)
{
	uint8_t y[F25519_SIZE];
	uint8_t a[F25519_SIZE];
	uint8_t b[F25519_SIZE];
	uint8_t c[F25519_SIZE];

	f25519_sub(a, u, f25519_one);
	f25519_add(b, u, f25519_one);
	f25519_inv__distinct(c, b);
	f25519_mul__distinct(y, a, c);

	ed25519_from_y(p, y);
}

/* RFC 7748 scalar clamping, applied to a copy so the caller's private key
 * is left as generated. */
static void smult_u(uint8_t *out, const struct ed25519_pt *q,
		    const uint8_t *private_key)
{
	struct ed25519_pt p;
	uint8_t e[ED25519_EXPONENT_SIZE];

	memcpy(e, private_key, sizeof(e));
	ed25519_prepare(e);
	ed25519_smult(&p, q, e);
	ed_to_u(out, &p);
}

int crypto_scalarmult_base(uint8_t *public_key, const uint8_t *private_key)
{
	smult_u(public_key, &ed25519_base, private_key);
	return 0;
}

int crypto_scalarmult(uint8_t *shared, const uint8_t *private_key,
		      const uint8_t *peer_public)
{
	struct ed25519_pt q;

	ed_from_u(&q, peer_public);
	smult_u(shared, &q, private_key);
	return 0;
}
