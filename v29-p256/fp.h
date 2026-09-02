/*
 * fp.h - generic modular arithmetic on 256-bit big-endian byte strings.
 *
 * Descended from Daniel Beer's public-domain fprime.c (c25519), rewritten
 * for v29-p256 in three ways:
 *
 *  - big-endian.  Every number this server exchanges with the peer (P-256
 *    coordinates, ECDSA r and s, the SHA-256 hash it signs) is big-endian
 *    on the wire, so elements are used in place with no byte reversal.
 *    Poly1305 is the one little-endian consumer and it indexes from the
 *    other end (see chapoly.h).
 *  - the modulus may use all 256 bits (P-256's p and n do).  The original
 *    required 2p-1 to fit the element; here every reduction is a single
 *    pass that computes s1 + s2 - m (or s1 - s2) with a signed carry and
 *    adds m back when that went negative, and the multiplier is built from
 *    such additions, so nothing ever holds more than 256 bits.
 *  - the multiplier always runs 256 iterations, so there is no msb scan of
 *    the modulus and no bit-count argument; a short multiplier (Poly1305's
 *    128-bit r) just spends its leading iterations shifting zeros.
 *
 * Every routine here is used with three moduli: P-256's field prime p and
 * group order n, and Poly1305's 2^130-5.  The modulus is the global fp_m
 * rather than an argument of every call: p256_smult(), ecdsa_sign() and
 * poly1305() each set it before their first field operation.  Timing does
 * not depend on the element values (the multiplier chooses its addend
 * with a cmov and the reductions are masked), only on the modulus.
 */
#ifndef FP_H
#define FP_H

#include <stdint.h>
#include "nolibc.h"

#define FP_SIZE 32

extern uint8_t fp_zero[FP_SIZE];   /* bss */
extern uint8_t fp_one[FP_SIZE];    /* fp_one[31] = 1, set by main() */
extern const uint8_t *fp_m;        /* the modulus, set by the caller */

/* d = s1 + s2 (neg = 0) or s1 - s2 (neg = -1) mod m, for s1, s2 < m; an
 * addition also accepts any s1 + s2 < 2m (so fp_add(d, x, fp_zero) reduces
 * an x < 2m).  d may alias s1 or s2. */
void fp_addsub(uint8_t *d, const uint8_t *s1, const uint8_t *s2, int neg);
#define fp_add(d, a, b) fp_addsub((d), (a), (b), 0)
#define fp_sub(d, a, b) fp_addsub((d), (a), (b), -1)

/* r = a * b mod m for a < m and any 256-bit b.  r must not alias a or b. */
void fp_mul(uint8_t *r, const uint8_t *a, const uint8_t *b);

/* r = x^(m-2) mod m: the inverse of x for a prime m whose low byte is >= 2
 * (true of p and n).  r must not alias x. */
void fp_inv(uint8_t *r, const uint8_t *x);

#endif /* FP_H */
