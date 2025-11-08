/*
 * Complete Ed25519 implementation
 * Based on RFC 8032 and djb's ref10 implementation
 * Implements key generation and signing for SSH server use
 */
#ifndef ED25519_MINIMAL_H
#define ED25519_MINIMAL_H

#include <stdint.h>
#include <string.h>
#include "fe25519.h"
#include "sha512_minimal.h"
#include "random_minimal.h"

typedef struct { fe X, Y, Z; } ge_p2;
typedef struct { fe X, Y, Z, T; } ge_p3;
typedef struct { fe X, Y, Z, T; } ge_p1p1;
typedef struct { fe yplusx, yminusx, xy2d; } ge_precomp;
typedef struct { fe YplusX, YminusX, Z, T2d; } ge_cached;

/* d = -121665/121666 */
static const fe d = {-10913610, 13857413, -15372611, 6949391, 114729, -8787816, -6275908, -3247719, -18696448, -12055116};
static const fe d2 = {-21827239, -5839606, -30745221, 13898782, 229458, 15978800, -12551817, -6495438, 29715968, 9444199};
static const fe sqrtm1 = {-32595792, -7943725, 9377950, 3500415, 12389472, -272473, -25146209, -2005654, 326686, 11406482};

/* Base point */
static const ge_p3 ge_base = {
    {25967493, -14356035, 29566456, 3660896, -12694345, 4014787, 27544626, -11754271, -6127391, 25961939},
    {-12545711, 934026, -3576485, -6364006, 31287772, -4716689, -7994600, -12661505, -1408303, 9843630},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {-7927307, 29872153, 4455705, 5668744, 23506502, -12741825, -27653434, -1042001, 2051984, 10529039}
};

/* Point operations */
static void ge_p3_0(ge_p3 *h) { fe_0(h->X); fe_1(h->Y); fe_1(h->Z); fe_0(h->T); }

static void ge_p3_tobytes(uint8_t s[32], const ge_p3 *h) {
    fe recip, x, y;
    fe_invert(recip, h->Z);
    fe_mul(x, h->X, recip);
    fe_mul(y, h->Y, recip);
    fe_tobytes(s, y);
    s[31] ^= fe_isnegative(x) << 7;
}

static void ge_p1p1_to_p2(ge_p2 *r, const ge_p1p1 *p) {
    fe_mul(r->X, p->X, p->T);
    fe_mul(r->Y, p->Y, p->Z);
    fe_mul(r->Z, p->Z, p->T);
}

static void ge_p1p1_to_p3(ge_p3 *r, const ge_p1p1 *p) {
    fe_mul(r->X, p->X, p->T);
    fe_mul(r->Y, p->Y, p->Z);
    fe_mul(r->Z, p->Z, p->T);
    fe_mul(r->T, p->X, p->Y);
}

static void ge_p2_dbl(ge_p1p1 *r, const ge_p2 *p) {
    fe t0;
    fe_sq(r->X, p->X);
    fe_sq(r->Z, p->Y);
    fe_sq2(r->T, p->Z);
    fe_add(r->Y, p->X, p->Y);
    fe_sq(t0, r->Y);
    fe_add(r->Y, r->Z, r->X);
    fe_sub(r->Z, r->Z, r->X);
    fe_sub(r->X, t0, r->Y);
    fe_sub(r->T, r->T, r->Z);
}

static void ge_p3_dbl(ge_p1p1 *r, const ge_p3 *p) {
    ge_p2 q;
    q.X[0] = p->X[0]; q.X[1] = p->X[1]; q.X[2] = p->X[2]; q.X[3] = p->X[3]; q.X[4] = p->X[4];
    q.X[5] = p->X[5]; q.X[6] = p->X[6]; q.X[7] = p->X[7]; q.X[8] = p->X[8]; q.X[9] = p->X[9];
    q.Y[0] = p->Y[0]; q.Y[1] = p->Y[1]; q.Y[2] = p->Y[2]; q.Y[3] = p->Y[3]; q.Y[4] = p->Y[4];
    q.Y[5] = p->Y[5]; q.Y[6] = p->Y[6]; q.Y[7] = p->Y[7]; q.Y[8] = p->Y[8]; q.Y[9] = p->Y[9];
    q.Z[0] = p->Z[0]; q.Z[1] = p->Z[1]; q.Z[2] = p->Z[2]; q.Z[3] = p->Z[3]; q.Z[4] = p->Z[4];
    q.Z[5] = p->Z[5]; q.Z[6] = p->Z[6]; q.Z[7] = p->Z[7]; q.Z[8] = p->Z[8]; q.Z[9] = p->Z[9];
    ge_p2_dbl(r, &q);
}

static void ge_madd(ge_p1p1 *r, const ge_p3 *p, const ge_precomp *q) {
    fe t0;
    fe_add(r->X, p->Y, p->X);
    fe_sub(r->Y, p->Y, p->X);
    fe_mul(r->Z, r->X, q->yplusx);
    fe_mul(r->Y, r->Y, q->yminusx);
    fe_mul(r->T, q->xy2d, p->T);
    fe_add(t0, p->Z, p->Z);
    fe_sub(r->X, r->Z, r->Y);
    fe_add(r->Y, r->Z, r->Y);
    fe_add(r->Z, t0, r->T);
    fe_sub(r->T, t0, r->T);
}

static void ge_add(ge_p1p1 *r, const ge_p3 *p, const ge_cached *q) {
    fe t0;
    fe_add(r->X, p->Y, p->X);
    fe_sub(r->Y, p->Y, p->X);
    fe_mul(r->Z, r->X, q->YplusX);
    fe_mul(r->Y, r->Y, q->YminusX);
    fe_mul(r->T, q->T2d, p->T);
    fe_mul(r->X, p->Z, q->Z);
    fe_add(t0, r->X, r->X);
    fe_sub(r->X, r->Z, r->Y);
    fe_add(r->Y, r->Z, r->Y);
    fe_add(r->Z, t0, r->T);
    fe_sub(r->T, t0, r->T);
}

static void ge_p3_to_cached(ge_cached *r, const ge_p3 *p) {
    fe_add(r->YplusX, p->Y, p->X);
    fe_sub(r->YminusX, p->Y, p->X);
    fe_copy(r->Z, p->Z);
    fe_mul(r->T2d, p->T, d2);
}

/* Scalar multiplication: r = a*B where B is base point
 * Simple double-and-add implementation - not constant-time but correct */
static void ge_scalarmult_base(ge_p3 *h, const uint8_t a[32]) {
    int i;
    ge_p1p1 r;
    ge_cached c;

    /* Start with identity/zero point */
    ge_p3_0(h);

    /* Double-and-add from MSB to LSB */
    for (i = 255; i >= 0; i--) {
        unsigned int bit = (a[i / 8] >> (i & 7)) & 1;

        /* Double: h = 2*h */
        ge_p3_dbl(&r, h);
        ge_p1p1_to_p3(h, &r);

        /* Add base point if bit is set: h = h + B */
        if (bit) {
            ge_p3_to_cached(&c, &ge_base);
            ge_add(&r, h, &c);
            ge_p1p1_to_p3(h, &r);
        }
    }
}

/* Scalar operations mod L */
static void sc_reduce(uint8_t s[64]) {
    int64_t s0 = 2097151 & (s[0] | ((int64_t)s[1] << 8) | ((int64_t)s[2] << 16));
    int64_t s1 = 2097151 & ((s[2] >> 5) | ((int64_t)s[3] << 3) | ((int64_t)s[4] << 11) | ((int64_t)s[5] << 19));
    int64_t s2 = 2097151 & ((s[5] >> 2) | ((int64_t)s[6] << 6) | ((int64_t)s[7] << 14));
    int64_t s3 = 2097151 & ((s[7] >> 7) | ((int64_t)s[8] << 1) | ((int64_t)s[9] << 9) | ((int64_t)s[10] << 17));
    int64_t s4 = 2097151 & ((s[10] >> 4) | ((int64_t)s[11] << 4) | ((int64_t)s[12] << 12));
    int64_t s5 = 2097151 & ((s[12] >> 1) | ((int64_t)s[13] << 7) | ((int64_t)s[14] << 15));
    int64_t s6 = 2097151 & ((s[14] >> 6) | ((int64_t)s[15] << 2) | ((int64_t)s[16] << 10) | ((int64_t)s[17] << 18));
    int64_t s7 = 2097151 & ((s[17] >> 3) | ((int64_t)s[18] << 5) | ((int64_t)s[19] << 13));
    int64_t s8 = 2097151 & (s[20] | ((int64_t)s[21] << 8) | ((int64_t)s[22] << 16));
    int64_t s9 = 2097151 & ((s[22] >> 5) | ((int64_t)s[23] << 3) | ((int64_t)s[24] << 11) | ((int64_t)s[25] << 19));
    int64_t s10 = 2097151 & ((s[25] >> 2) | ((int64_t)s[26] << 6) | ((int64_t)s[27] << 14));
    int64_t s11 = 2097151 & ((s[27] >> 7) | ((int64_t)s[28] << 1) | ((int64_t)s[29] << 9) | ((int64_t)s[30] << 17));
    int64_t s12 = 2097151 & ((s[30] >> 4) | ((int64_t)s[31] << 4) | ((int64_t)s[32] << 12));
    int64_t s13 = 2097151 & ((s[32] >> 1) | ((int64_t)s[33] << 7) | ((int64_t)s[34] << 15));
    int64_t s14 = 2097151 & ((s[34] >> 6) | ((int64_t)s[35] << 2) | ((int64_t)s[36] << 10) | ((int64_t)s[37] << 18));
    int64_t s15 = 2097151 & ((s[37] >> 3) | ((int64_t)s[38] << 5) | ((int64_t)s[39] << 13));
    int64_t s16 = 2097151 & (s[40] | ((int64_t)s[41] << 8) | ((int64_t)s[42] << 16));
    int64_t s17 = 2097151 & ((s[42] >> 5) | ((int64_t)s[43] << 3) | ((int64_t)s[44] << 11) | ((int64_t)s[45] << 19));
    int64_t s18 = 2097151 & ((s[45] >> 2) | ((int64_t)s[46] << 6) | ((int64_t)s[47] << 14));
    int64_t s19 = 2097151 & ((s[47] >> 7) | ((int64_t)s[48] << 1) | ((int64_t)s[49] << 9) | ((int64_t)s[50] << 17));
    int64_t s20 = 2097151 & ((s[50] >> 4) | ((int64_t)s[51] << 4) | ((int64_t)s[52] << 12));
    int64_t s21 = 2097151 & ((s[52] >> 1) | ((int64_t)s[53] << 7) | ((int64_t)s[54] << 15));
    int64_t s22 = 2097151 & ((s[54] >> 6) | ((int64_t)s[55] << 2) | ((int64_t)s[56] << 10) | ((int64_t)s[57] << 18));
    int64_t s23 = (s[57] >> 3) | ((int64_t)s[58] << 5) | ((int64_t)s[59] << 13) | ((int64_t)s[60] << 21) | ((int64_t)s[61] << 29) | ((int64_t)s[62] << 37) | ((int64_t)s[63] << 45);
    int64_t carry[23];

    s11 += s23 * 666643; s12 += s23 * 470296; s13 += s23 * 654183; s14 -= s23 * 997805; s15 += s23 * 136657; s16 -= s23 * 683901;
    s10 += s22 * 666643; s11 += s22 * 470296; s12 += s22 * 654183; s13 -= s22 * 997805; s14 += s22 * 136657; s15 -= s22 * 683901;
    s9 += s21 * 666643; s10 += s21 * 470296; s11 += s21 * 654183; s12 -= s21 * 997805; s13 += s21 * 136657; s14 -= s21 * 683901;
    s8 += s20 * 666643; s9 += s20 * 470296; s10 += s20 * 654183; s11 -= s20 * 997805; s12 += s20 * 136657; s13 -= s20 * 683901;
    s7 += s19 * 666643; s8 += s19 * 470296; s9 += s19 * 654183; s10 -= s19 * 997805; s11 += s19 * 136657; s12 -= s19 * 683901;
    s6 += s18 * 666643; s7 += s18 * 470296; s8 += s18 * 654183; s9 -= s18 * 997805; s10 += s18 * 136657; s11 -= s18 * 683901;

    carry[6] = (s6 + (1<<20)) >> 21; s7 += carry[6]; s6 -= carry[6] << 21;
    carry[8] = (s8 + (1<<20)) >> 21; s9 += carry[8]; s8 -= carry[8] << 21;
    carry[10] = (s10 + (1<<20)) >> 21; s11 += carry[10]; s10 -= carry[10] << 21;
    carry[12] = (s12 + (1<<20)) >> 21; s13 += carry[12]; s12 -= carry[12] << 21;
    carry[14] = (s14 + (1<<20)) >> 21; s15 += carry[14]; s14 -= carry[14] << 21;
    carry[16] = (s16 + (1<<20)) >> 21; s17 += carry[16]; s16 -= carry[16] << 21;

    carry[7] = (s7 + (1<<20)) >> 21; s8 += carry[7]; s7 -= carry[7] << 21;
    carry[9] = (s9 + (1<<20)) >> 21; s10 += carry[9]; s9 -= carry[9] << 21;
    carry[11] = (s11 + (1<<20)) >> 21; s12 += carry[11]; s11 -= carry[11] << 21;
    carry[13] = (s13 + (1<<20)) >> 21; s14 += carry[13]; s13 -= carry[13] << 21;
    carry[15] = (s15 + (1<<20)) >> 21; s16 += carry[15]; s15 -= carry[15] << 21;

    s5 += s17 * 666643; s6 += s17 * 470296; s7 += s17 * 654183; s8 -= s17 * 997805; s9 += s17 * 136657; s10 -= s17 * 683901;
    s4 += s16 * 666643; s5 += s16 * 470296; s6 += s16 * 654183; s7 -= s16 * 997805; s8 += s16 * 136657; s9 -= s16 * 683901;
    s3 += s15 * 666643; s4 += s15 * 470296; s5 += s15 * 654183; s6 -= s15 * 997805; s7 += s15 * 136657; s8 -= s15 * 683901;
    s2 += s14 * 666643; s3 += s14 * 470296; s4 += s14 * 654183; s5 -= s14 * 997805; s6 += s14 * 136657; s7 -= s14 * 683901;
    s1 += s13 * 666643; s2 += s13 * 470296; s3 += s13 * 654183; s4 -= s13 * 997805; s5 += s13 * 136657; s6 -= s13 * 683901;
    s0 += s12 * 666643; s1 += s12 * 470296; s2 += s12 * 654183; s3 -= s12 * 997805; s4 += s12 * 136657; s5 -= s12 * 683901;
    s12 = 0;

    carry[0] = (s0 + (1<<20)) >> 21; s1 += carry[0]; s0 -= carry[0] << 21;
    carry[2] = (s2 + (1<<20)) >> 21; s3 += carry[2]; s2 -= carry[2] << 21;
    carry[4] = (s4 + (1<<20)) >> 21; s5 += carry[4]; s4 -= carry[4] << 21;
    carry[6] = (s6 + (1<<20)) >> 21; s7 += carry[6]; s6 -= carry[6] << 21;
    carry[8] = (s8 + (1<<20)) >> 21; s9 += carry[8]; s8 -= carry[8] << 21;
    carry[10] = (s10 + (1<<20)) >> 21; s11 += carry[10]; s10 -= carry[10] << 21;

    carry[1] = (s1 + (1<<20)) >> 21; s2 += carry[1]; s1 -= carry[1] << 21;
    carry[3] = (s3 + (1<<20)) >> 21; s4 += carry[3]; s3 -= carry[3] << 21;
    carry[5] = (s5 + (1<<20)) >> 21; s6 += carry[5]; s5 -= carry[5] << 21;
    carry[7] = (s7 + (1<<20)) >> 21; s8 += carry[7]; s7 -= carry[7] << 21;
    carry[9] = (s9 + (1<<20)) >> 21; s10 += carry[9]; s9 -= carry[9] << 21;
    carry[11] = (s11 + (1<<20)) >> 21; s12 += carry[11]; s11 -= carry[11] << 21;

    s0 += s12 * 666643; s1 += s12 * 470296; s2 += s12 * 654183; s3 -= s12 * 997805; s4 += s12 * 136657; s5 -= s12 * 683901; s12 = 0;

    carry[0] = s0 >> 21; s1 += carry[0]; s0 -= carry[0] << 21;
    carry[1] = s1 >> 21; s2 += carry[1]; s1 -= carry[1] << 21;
    carry[2] = s2 >> 21; s3 += carry[2]; s2 -= carry[2] << 21;
    carry[3] = s3 >> 21; s4 += carry[3]; s3 -= carry[3] << 21;
    carry[4] = s4 >> 21; s5 += carry[4]; s4 -= carry[4] << 21;
    carry[5] = s5 >> 21; s6 += carry[5]; s5 -= carry[5] << 21;
    carry[6] = s6 >> 21; s7 += carry[6]; s6 -= carry[6] << 21;
    carry[7] = s7 >> 21; s8 += carry[7]; s7 -= carry[7] << 21;
    carry[8] = s8 >> 21; s9 += carry[8]; s8 -= carry[8] << 21;
    carry[9] = s9 >> 21; s10 += carry[9]; s9 -= carry[9] << 21;
    carry[10] = s10 >> 21; s11 += carry[10]; s10 -= carry[10] << 21;
    carry[11] = s11 >> 21; s12 += carry[11]; s11 -= carry[11] << 21;

    s0 += s12 * 666643; s1 += s12 * 470296; s2 += s12 * 654183; s3 -= s12 * 997805; s4 += s12 * 136657; s5 -= s12 * 683901;

    carry[0] = s0 >> 21; s1 += carry[0]; s0 -= carry[0] << 21;
    carry[1] = s1 >> 21; s2 += carry[1]; s1 -= carry[1] << 21;
    carry[2] = s2 >> 21; s3 += carry[2]; s2 -= carry[2] << 21;
    carry[3] = s3 >> 21; s4 += carry[3]; s3 -= carry[3] << 21;
    carry[4] = s4 >> 21; s5 += carry[4]; s4 -= carry[4] << 21;
    carry[5] = s5 >> 21; s6 += carry[5]; s5 -= carry[5] << 21;
    carry[6] = s6 >> 21; s7 += carry[6]; s6 -= carry[6] << 21;
    carry[7] = s7 >> 21; s8 += carry[7]; s7 -= carry[7] << 21;
    carry[8] = s8 >> 21; s9 += carry[8]; s8 -= carry[8] << 21;
    carry[9] = s9 >> 21; s10 += carry[9]; s9 -= carry[9] << 21;
    carry[10] = s10 >> 21; s11 += carry[10]; s10 -= carry[10] << 21;

    s[0] = s0 >> 0; s[1] = s0 >> 8; s[2] = (s0 >> 16) | (s1 << 5);
    s[3] = s1 >> 3; s[4] = s1 >> 11; s[5] = (s1 >> 19) | (s2 << 2);
    s[6] = s2 >> 6; s[7] = (s2 >> 14) | (s3 << 7);
    s[8] = s3 >> 1; s[9] = s3 >> 9; s[10] = (s3 >> 17) | (s4 << 4);
    s[11] = s4 >> 4; s[12] = s4 >> 12; s[13] = (s4 >> 20) | (s5 << 1);
    s[14] = s5 >> 7; s[15] = (s5 >> 15) | (s6 << 6);
    s[16] = s6 >> 2; s[17] = s6 >> 10; s[18] = (s6 >> 18) | (s7 << 3);
    s[19] = s7 >> 5; s[20] = s7 >> 13;
    s[21] = s8 >> 0; s[22] = s8 >> 8; s[23] = (s8 >> 16) | (s9 << 5);
    s[24] = s9 >> 3; s[25] = s9 >> 11; s[26] = (s9 >> 19) | (s10 << 2);
    s[27] = s10 >> 6; s[28] = (s10 >> 14) | (s11 << 7);
    s[29] = s11 >> 1; s[30] = s11 >> 9; s[31] = s11 >> 17;
    memset(s + 32, 0, 32);
}

static void sc_muladd(uint8_t s[32], const uint8_t a[32], const uint8_t b[32], const uint8_t c[32]) {
    int64_t a0 = 2097151 & (a[0] | ((int64_t)a[1] << 8) | ((int64_t)a[2] << 16));
    int64_t a1 = 2097151 & ((a[2] >> 5) | ((int64_t)a[3] << 3) | ((int64_t)a[4] << 11) | ((int64_t)a[5] << 19));
    int64_t a2 = 2097151 & ((a[5] >> 2) | ((int64_t)a[6] << 6) | ((int64_t)a[7] << 14));
    int64_t a3 = 2097151 & ((a[7] >> 7) | ((int64_t)a[8] << 1) | ((int64_t)a[9] << 9) | ((int64_t)a[10] << 17));
    int64_t a4 = 2097151 & ((a[10] >> 4) | ((int64_t)a[11] << 4) | ((int64_t)a[12] << 12));
    int64_t a5 = 2097151 & ((a[12] >> 1) | ((int64_t)a[13] << 7) | ((int64_t)a[14] << 15));
    int64_t a6 = 2097151 & ((a[14] >> 6) | ((int64_t)a[15] << 2) | ((int64_t)a[16] << 10) | ((int64_t)a[17] << 18));
    int64_t a7 = 2097151 & ((a[17] >> 3) | ((int64_t)a[18] << 5) | ((int64_t)a[19] << 13));
    int64_t a8 = 2097151 & (a[20] | ((int64_t)a[21] << 8) | ((int64_t)a[22] << 16));
    int64_t a9 = 2097151 & ((a[22] >> 5) | ((int64_t)a[23] << 3) | ((int64_t)a[24] << 11) | ((int64_t)a[25] << 19));
    int64_t a10 = 2097151 & ((a[25] >> 2) | ((int64_t)a[26] << 6) | ((int64_t)a[27] << 14));
    int64_t a11 = (a[27] >> 7) | ((int64_t)a[28] << 1) | ((int64_t)a[29] << 9) | ((int64_t)a[30] << 17) | ((int64_t)a[31] << 25);

    int64_t b0 = 2097151 & (b[0] | ((int64_t)b[1] << 8) | ((int64_t)b[2] << 16));
    int64_t b1 = 2097151 & ((b[2] >> 5) | ((int64_t)b[3] << 3) | ((int64_t)b[4] << 11) | ((int64_t)b[5] << 19));
    int64_t b2 = 2097151 & ((b[5] >> 2) | ((int64_t)b[6] << 6) | ((int64_t)b[7] << 14));
    int64_t b3 = 2097151 & ((b[7] >> 7) | ((int64_t)b[8] << 1) | ((int64_t)b[9] << 9) | ((int64_t)b[10] << 17));
    int64_t b4 = 2097151 & ((b[10] >> 4) | ((int64_t)b[11] << 4) | ((int64_t)b[12] << 12));
    int64_t b5 = 2097151 & ((b[12] >> 1) | ((int64_t)b[13] << 7) | ((int64_t)b[14] << 15));
    int64_t b6 = 2097151 & ((b[14] >> 6) | ((int64_t)b[15] << 2) | ((int64_t)b[16] << 10) | ((int64_t)b[17] << 18));
    int64_t b7 = 2097151 & ((b[17] >> 3) | ((int64_t)b[18] << 5) | ((int64_t)b[19] << 13));
    int64_t b8 = 2097151 & (b[20] | ((int64_t)b[21] << 8) | ((int64_t)b[22] << 16));
    int64_t b9 = 2097151 & ((b[22] >> 5) | ((int64_t)b[23] << 3) | ((int64_t)b[24] << 11) | ((int64_t)b[25] << 19));
    int64_t b10 = 2097151 & ((b[25] >> 2) | ((int64_t)b[26] << 6) | ((int64_t)b[27] << 14));
    int64_t b11 = (b[27] >> 7) | ((int64_t)b[28] << 1) | ((int64_t)b[29] << 9) | ((int64_t)b[30] << 17) | ((int64_t)b[31] << 25);

    int64_t c0 = 2097151 & (c[0] | ((int64_t)c[1] << 8) | ((int64_t)c[2] << 16));
    int64_t c1 = 2097151 & ((c[2] >> 5) | ((int64_t)c[3] << 3) | ((int64_t)c[4] << 11) | ((int64_t)c[5] << 19));
    int64_t c2 = 2097151 & ((c[5] >> 2) | ((int64_t)c[6] << 6) | ((int64_t)c[7] << 14));
    int64_t c3 = 2097151 & ((c[7] >> 7) | ((int64_t)c[8] << 1) | ((int64_t)c[9] << 9) | ((int64_t)c[10] << 17));
    int64_t c4 = 2097151 & ((c[10] >> 4) | ((int64_t)c[11] << 4) | ((int64_t)c[12] << 12));
    int64_t c5 = 2097151 & ((c[12] >> 1) | ((int64_t)c[13] << 7) | ((int64_t)c[14] << 15));
    int64_t c6 = 2097151 & ((c[14] >> 6) | ((int64_t)c[15] << 2) | ((int64_t)c[16] << 10) | ((int64_t)c[17] << 18));
    int64_t c7 = 2097151 & ((c[17] >> 3) | ((int64_t)c[18] << 5) | ((int64_t)c[19] << 13));
    int64_t c8 = 2097151 & (c[20] | ((int64_t)c[21] << 8) | ((int64_t)c[22] << 16));
    int64_t c9 = 2097151 & ((c[22] >> 5) | ((int64_t)c[23] << 3) | ((int64_t)c[24] << 11) | ((int64_t)c[25] << 19));
    int64_t c10 = 2097151 & ((c[25] >> 2) | ((int64_t)c[26] << 6) | ((int64_t)c[27] << 14));
    int64_t c11 = (c[27] >> 7) | ((int64_t)c[28] << 1) | ((int64_t)c[29] << 9) | ((int64_t)c[30] << 17) | ((int64_t)c[31] << 25);

    int64_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, s15, s16, s17, s18, s19, s20, s21, s22, s23;
    int64_t carry[23];

    s0 = c0 + a0*b0;
    s1 = c1 + a0*b1 + a1*b0;
    s2 = c2 + a0*b2 + a1*b1 + a2*b0;
    s3 = c3 + a0*b3 + a1*b2 + a2*b1 + a3*b0;
    s4 = c4 + a0*b4 + a1*b3 + a2*b2 + a3*b1 + a4*b0;
    s5 = c5 + a0*b5 + a1*b4 + a2*b3 + a3*b2 + a4*b1 + a5*b0;
    s6 = c6 + a0*b6 + a1*b5 + a2*b4 + a3*b3 + a4*b2 + a5*b1 + a6*b0;
    s7 = c7 + a0*b7 + a1*b6 + a2*b5 + a3*b4 + a4*b3 + a5*b2 + a6*b1 + a7*b0;
    s8 = c8 + a0*b8 + a1*b7 + a2*b6 + a3*b5 + a4*b4 + a5*b3 + a6*b2 + a7*b1 + a8*b0;
    s9 = c9 + a0*b9 + a1*b8 + a2*b7 + a3*b6 + a4*b5 + a5*b4 + a6*b3 + a7*b2 + a8*b1 + a9*b0;
    s10 = c10 + a0*b10 + a1*b9 + a2*b8 + a3*b7 + a4*b6 + a5*b5 + a6*b4 + a7*b3 + a8*b2 + a9*b1 + a10*b0;
    s11 = c11 + a0*b11 + a1*b10 + a2*b9 + a3*b8 + a4*b7 + a5*b6 + a6*b5 + a7*b4 + a8*b3 + a9*b2 + a10*b1 + a11*b0;
    s12 = a1*b11 + a2*b10 + a3*b9 + a4*b8 + a5*b7 + a6*b6 + a7*b5 + a8*b4 + a9*b3 + a10*b2 + a11*b1;
    s13 = a2*b11 + a3*b10 + a4*b9 + a5*b8 + a6*b7 + a7*b6 + a8*b5 + a9*b4 + a10*b3 + a11*b2;
    s14 = a3*b11 + a4*b10 + a5*b9 + a6*b8 + a7*b7 + a8*b6 + a9*b5 + a10*b4 + a11*b3;
    s15 = a4*b11 + a5*b10 + a6*b9 + a7*b8 + a8*b7 + a9*b6 + a10*b5 + a11*b4;
    s16 = a5*b11 + a6*b10 + a7*b9 + a8*b8 + a9*b7 + a10*b6 + a11*b5;
    s17 = a6*b11 + a7*b10 + a8*b9 + a9*b8 + a10*b7 + a11*b6;
    s18 = a7*b11 + a8*b10 + a9*b9 + a10*b8 + a11*b7;
    s19 = a8*b11 + a9*b10 + a10*b9 + a11*b8;
    s20 = a9*b11 + a10*b10 + a11*b9;
    s21 = a10*b11 + a11*b10;
    s22 = a11*b11;
    s23 = 0;

    carry[0] = (s0 + (1<<20)) >> 21; s1 += carry[0]; s0 -= carry[0] << 21;
    carry[2] = (s2 + (1<<20)) >> 21; s3 += carry[2]; s2 -= carry[2] << 21;
    carry[4] = (s4 + (1<<20)) >> 21; s5 += carry[4]; s4 -= carry[4] << 21;
    carry[6] = (s6 + (1<<20)) >> 21; s7 += carry[6]; s6 -= carry[6] << 21;
    carry[8] = (s8 + (1<<20)) >> 21; s9 += carry[8]; s8 -= carry[8] << 21;
    carry[10] = (s10 + (1<<20)) >> 21; s11 += carry[10]; s10 -= carry[10] << 21;
    carry[12] = (s12 + (1<<20)) >> 21; s13 += carry[12]; s12 -= carry[12] << 21;
    carry[14] = (s14 + (1<<20)) >> 21; s15 += carry[14]; s14 -= carry[14] << 21;
    carry[16] = (s16 + (1<<20)) >> 21; s17 += carry[16]; s16 -= carry[16] << 21;
    carry[18] = (s18 + (1<<20)) >> 21; s19 += carry[18]; s18 -= carry[18] << 21;
    carry[20] = (s20 + (1<<20)) >> 21; s21 += carry[20]; s20 -= carry[20] << 21;
    carry[22] = (s22 + (1<<20)) >> 21; s23 += carry[22]; s22 -= carry[22] << 21;

    carry[1] = (s1 + (1<<20)) >> 21; s2 += carry[1]; s1 -= carry[1] << 21;
    carry[3] = (s3 + (1<<20)) >> 21; s4 += carry[3]; s3 -= carry[3] << 21;
    carry[5] = (s5 + (1<<20)) >> 21; s6 += carry[5]; s5 -= carry[5] << 21;
    carry[7] = (s7 + (1<<20)) >> 21; s8 += carry[7]; s7 -= carry[7] << 21;
    carry[9] = (s9 + (1<<20)) >> 21; s10 += carry[9]; s9 -= carry[9] << 21;
    carry[11] = (s11 + (1<<20)) >> 21; s12 += carry[11]; s11 -= carry[11] << 21;
    carry[13] = (s13 + (1<<20)) >> 21; s14 += carry[13]; s13 -= carry[13] << 21;
    carry[15] = (s15 + (1<<20)) >> 21; s16 += carry[15]; s15 -= carry[15] << 21;
    carry[17] = (s17 + (1<<20)) >> 21; s18 += carry[17]; s17 -= carry[17] << 21;
    carry[19] = (s19 + (1<<20)) >> 21; s20 += carry[19]; s19 -= carry[19] << 21;
    carry[21] = (s21 + (1<<20)) >> 21; s22 += carry[21]; s21 -= carry[21] << 21;

    s11 += s23 * 666643; s12 += s23 * 470296; s13 += s23 * 654183; s14 -= s23 * 997805; s15 += s23 * 136657; s16 -= s23 * 683901;
    s10 += s22 * 666643; s11 += s22 * 470296; s12 += s22 * 654183; s13 -= s22 * 997805; s14 += s22 * 136657; s15 -= s22 * 683901;
    s9 += s21 * 666643; s10 += s21 * 470296; s11 += s21 * 654183; s12 -= s21 * 997805; s13 += s21 * 136657; s14 -= s21 * 683901;
    s8 += s20 * 666643; s9 += s20 * 470296; s10 += s20 * 654183; s11 -= s20 * 997805; s12 += s20 * 136657; s13 -= s20 * 683901;
    s7 += s19 * 666643; s8 += s19 * 470296; s9 += s19 * 654183; s10 -= s19 * 997805; s11 += s19 * 136657; s12 -= s19 * 683901;
    s6 += s18 * 666643; s7 += s18 * 470296; s8 += s18 * 654183; s9 -= s18 * 997805; s10 += s18 * 136657; s11 -= s18 * 683901;

    carry[6] = (s6 + (1<<20)) >> 21; s7 += carry[6]; s6 -= carry[6] << 21;
    carry[8] = (s8 + (1<<20)) >> 21; s9 += carry[8]; s8 -= carry[8] << 21;
    carry[10] = (s10 + (1<<20)) >> 21; s11 += carry[10]; s10 -= carry[10] << 21;
    carry[12] = (s12 + (1<<20)) >> 21; s13 += carry[12]; s12 -= carry[12] << 21;
    carry[14] = (s14 + (1<<20)) >> 21; s15 += carry[14]; s14 -= carry[14] << 21;
    carry[16] = (s16 + (1<<20)) >> 21; s17 += carry[16]; s16 -= carry[16] << 21;

    carry[7] = (s7 + (1<<20)) >> 21; s8 += carry[7]; s7 -= carry[7] << 21;
    carry[9] = (s9 + (1<<20)) >> 21; s10 += carry[9]; s9 -= carry[9] << 21;
    carry[11] = (s11 + (1<<20)) >> 21; s12 += carry[11]; s11 -= carry[11] << 21;
    carry[13] = (s13 + (1<<20)) >> 21; s14 += carry[13]; s13 -= carry[13] << 21;
    carry[15] = (s15 + (1<<20)) >> 21; s16 += carry[15]; s15 -= carry[15] << 21;

    s5 += s17 * 666643; s6 += s17 * 470296; s7 += s17 * 654183; s8 -= s17 * 997805; s9 += s17 * 136657; s10 -= s17 * 683901;
    s4 += s16 * 666643; s5 += s16 * 470296; s6 += s16 * 654183; s7 -= s16 * 997805; s8 += s16 * 136657; s9 -= s16 * 683901;
    s3 += s15 * 666643; s4 += s15 * 470296; s5 += s15 * 654183; s6 -= s15 * 997805; s7 += s15 * 136657; s8 -= s15 * 683901;
    s2 += s14 * 666643; s3 += s14 * 470296; s4 += s14 * 654183; s5 -= s14 * 997805; s6 += s14 * 136657; s7 -= s14 * 683901;
    s1 += s13 * 666643; s2 += s13 * 470296; s3 += s13 * 654183; s4 -= s13 * 997805; s5 += s13 * 136657; s6 -= s13 * 683901;
    s0 += s12 * 666643; s1 += s12 * 470296; s2 += s12 * 654183; s3 -= s12 * 997805; s4 += s12 * 136657; s5 -= s12 * 683901;
    s12 = 0;

    carry[0] = (s0 + (1<<20)) >> 21; s1 += carry[0]; s0 -= carry[0] << 21;
    carry[2] = (s2 + (1<<20)) >> 21; s3 += carry[2]; s2 -= carry[2] << 21;
    carry[4] = (s4 + (1<<20)) >> 21; s5 += carry[4]; s4 -= carry[4] << 21;
    carry[6] = (s6 + (1<<20)) >> 21; s7 += carry[6]; s6 -= carry[6] << 21;
    carry[8] = (s8 + (1<<20)) >> 21; s9 += carry[8]; s8 -= carry[8] << 21;
    carry[10] = (s10 + (1<<20)) >> 21; s11 += carry[10]; s10 -= carry[10] << 21;

    carry[1] = (s1 + (1<<20)) >> 21; s2 += carry[1]; s1 -= carry[1] << 21;
    carry[3] = (s3 + (1<<20)) >> 21; s4 += carry[3]; s3 -= carry[3] << 21;
    carry[5] = (s5 + (1<<20)) >> 21; s6 += carry[5]; s5 -= carry[5] << 21;
    carry[7] = (s7 + (1<<20)) >> 21; s8 += carry[7]; s7 -= carry[7] << 21;
    carry[9] = (s9 + (1<<20)) >> 21; s10 += carry[9]; s9 -= carry[9] << 21;
    carry[11] = (s11 + (1<<20)) >> 21; s12 += carry[11]; s11 -= carry[11] << 21;

    s0 += s12 * 666643; s1 += s12 * 470296; s2 += s12 * 654183; s3 -= s12 * 997805; s4 += s12 * 136657; s5 -= s12 * 683901; s12 = 0;

    carry[0] = s0 >> 21; s1 += carry[0]; s0 -= carry[0] << 21;
    carry[1] = s1 >> 21; s2 += carry[1]; s1 -= carry[1] << 21;
    carry[2] = s2 >> 21; s3 += carry[2]; s2 -= carry[2] << 21;
    carry[3] = s3 >> 21; s4 += carry[3]; s3 -= carry[3] << 21;
    carry[4] = s4 >> 21; s5 += carry[4]; s4 -= carry[4] << 21;
    carry[5] = s5 >> 21; s6 += carry[5]; s5 -= carry[5] << 21;
    carry[6] = s6 >> 21; s7 += carry[6]; s6 -= carry[6] << 21;
    carry[7] = s7 >> 21; s8 += carry[7]; s7 -= carry[7] << 21;
    carry[8] = s8 >> 21; s9 += carry[8]; s8 -= carry[8] << 21;
    carry[9] = s9 >> 21; s10 += carry[9]; s9 -= carry[9] << 21;
    carry[10] = s10 >> 21; s11 += carry[10]; s10 -= carry[10] << 21;
    carry[11] = s11 >> 21; s12 += carry[11]; s11 -= carry[11] << 21;

    s0 += s12 * 666643; s1 += s12 * 470296; s2 += s12 * 654183; s3 -= s12 * 997805; s4 += s12 * 136657; s5 -= s12 * 683901;

    carry[0] = s0 >> 21; s1 += carry[0]; s0 -= carry[0] << 21;
    carry[1] = s1 >> 21; s2 += carry[1]; s1 -= carry[1] << 21;
    carry[2] = s2 >> 21; s3 += carry[2]; s2 -= carry[2] << 21;
    carry[3] = s3 >> 21; s4 += carry[3]; s3 -= carry[3] << 21;
    carry[4] = s4 >> 21; s5 += carry[4]; s4 -= carry[4] << 21;
    carry[5] = s5 >> 21; s6 += carry[5]; s5 -= carry[5] << 21;
    carry[6] = s6 >> 21; s7 += carry[6]; s6 -= carry[6] << 21;
    carry[7] = s7 >> 21; s8 += carry[7]; s7 -= carry[7] << 21;
    carry[8] = s8 >> 21; s9 += carry[8]; s8 -= carry[8] << 21;
    carry[9] = s9 >> 21; s10 += carry[9]; s9 -= carry[9] << 21;
    carry[10] = s10 >> 21; s11 += carry[10]; s10 -= carry[10] << 21;

    s[0] = s0 >> 0; s[1] = s0 >> 8; s[2] = (s0 >> 16) | (s1 << 5);
    s[3] = s1 >> 3; s[4] = s1 >> 11; s[5] = (s1 >> 19) | (s2 << 2);
    s[6] = s2 >> 6; s[7] = (s2 >> 14) | (s3 << 7);
    s[8] = s3 >> 1; s[9] = s3 >> 9; s[10] = (s3 >> 17) | (s4 << 4);
    s[11] = s4 >> 4; s[12] = s4 >> 12; s[13] = (s4 >> 20) | (s5 << 1);
    s[14] = s5 >> 7; s[15] = (s5 >> 15) | (s6 << 6);
    s[16] = s6 >> 2; s[17] = s6 >> 10; s[18] = (s6 >> 18) | (s7 << 3);
    s[19] = s7 >> 5; s[20] = s7 >> 13;
    s[21] = s8 >> 0; s[22] = s8 >> 8; s[23] = (s8 >> 16) | (s9 << 5);
    s[24] = s9 >> 3; s[25] = s9 >> 11; s[26] = (s9 >> 19) | (s10 << 2);
    s[27] = s10 >> 6; s[28] = (s10 >> 14) | (s11 << 7);
    s[29] = s11 >> 1; s[30] = s11 >> 9; s[31] = s11 >> 17;
}

/* Public API */
static void crypto_sign_keypair(uint8_t pk[32], uint8_t sk[64]) {
    uint8_t seed[32], h[64];
    ge_p3 A;

    randombytes_buf(seed, 32);
    sha512(h, seed, 32);
    h[0] &= 248;
    h[31] &= 127;
    h[31] |= 64;

    ge_scalarmult_base(&A, h);
    ge_p3_tobytes(pk, &A);

    memcpy(sk, seed, 32);
    memcpy(sk + 32, pk, 32);
}

static void crypto_sign_detached(uint8_t sig[64], unsigned long long *siglen,
                                  const uint8_t *m, unsigned long long mlen,
                                  const uint8_t sk[64]) {
    uint8_t az[64], nonce[64], hram[64];
    ge_p3 R;

    sha512(az, sk, 32);
    az[0] &= 248;
    az[31] &= 127;
    az[31] |= 64;

    sha512_ctx ctx;
    sha512_init(&ctx);
    sha512_update(&ctx, az + 32, 32);
    sha512_update(&ctx, m, mlen);
    sha512_final(&ctx, nonce);
    sc_reduce(nonce);

    ge_scalarmult_base(&R, nonce);
    ge_p3_tobytes(sig, &R);

    sha512_init(&ctx);
    sha512_update(&ctx, sig, 32);
    sha512_update(&ctx, sk + 32, 32);
    sha512_update(&ctx, m, mlen);
    sha512_final(&ctx, hram);
    sc_reduce(hram);

    sc_muladd(sig + 32, hram, az, nonce);

    if (siglen) *siglen = 64;
}

#endif /* ED25519_MINIMAL_H */
