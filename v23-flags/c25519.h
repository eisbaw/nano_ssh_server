/* Curve25519 (Montgomery form) scalar multiplication
 * Daniel Beer <dlbeer@gmail.com>, 18 Apr 2014
 *
 * This file is in the public domain.
 */

#ifndef C25519_H_
#define C25519_H_

#include "f25519.h"

/* Any two points on the curve can be added by treating their X
 * coordinates only. This is sufficient for Diffie-Hellman.
 */
#define C25519_EXPONENT_SIZE  32

/* Prepare an exponent by clamping appropriate bits */
static inline void c25519_prepare(uint8_t *key)
{
	key[0] &= 0xf8;
	key[31] &= 0x7f;
	key[31] |= 0x40;
}

/* X coordinate of the base point */
extern const uint8_t c25519_base_x[F25519_SIZE];

/* X coordinate of the result of the scalar multiplication */
void c25519_smult(uint8_t *result, const uint8_t *q, const uint8_t *e);

#endif
