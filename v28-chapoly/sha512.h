/* SHA512
 * Daniel Beer <dlbeer@gmail.com>, 22 Apr 2014
 *
 * This file is in the public domain.
 */

#ifndef SHA512_H_
#define SHA512_H_

#if !defined(COMPACT_DISABLE_ED25519) || !defined(COMPACT_DISABLE_X25519_DERIVE)
#include <stdint.h>
#include <stddef.h>
#include "nolibc.h"

/* SHA512 state. State is updated as data is fed in, and then the final
 * hash can be read out in slices.
 *
 * Data is fed in as a sequence of full blocks terminated by a single
 * partial block.
 */
struct sha512_state {
	uint64_t  h[8];
};

/* FIPS 180-4 constants, filled in at startup by sha_gentables() (which must
 * run before any hashing). Shared with SHA-256: its K table and initial
 * state are the top 32 bits of these values. */
extern struct sha512_state sha512_initial_state;
extern uint64_t sha512_kgen[80];
void sha_gentables(void);

/* Feed a full block in */
#define SHA512_BLOCK_SIZE  128

void sha512_block(struct sha512_state *s, const uint8_t *blk);

/* Hash R || A || M, three 32-byte strings (the Ed25519 challenge). */
#define SHA512_HASH_SIZE  64
#define SHA512_96_SIZE    96

void sha512_3x32(uint8_t *out, const uint8_t *r, const uint8_t *a,
		 const uint8_t *m);

#endif
#endif
