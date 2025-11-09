/*
 * Shared field element operations for Curve25519 and Ed25519
 * Field: integers modulo 2^255 - 19
 */
#ifndef FE25519_H
#define FE25519_H

#include <stdint.h>
#include <string.h>

/* Field element: 10 limbs of 26/25 bits each */
typedef int64_t fe[10];

/* Field constants */
static void fe_0(fe h) { memset(h, 0, sizeof(fe)); }
static void fe_1(fe h) { fe_0(h); h[0] = 1; }
static void fe_copy(fe h, const fe f) { int i; for (i = 0; i < 10; i++) h[i] = f[i]; }

static void fe_add(fe h, const fe f, const fe g) {
    int i; for (i = 0; i < 10; i++) h[i] = f[i] + g[i];
}

static void fe_sub(fe h, const fe f, const fe g) {
    int i; for (i = 0; i < 10; i++) h[i] = f[i] - g[i];
}

static void fe_neg(fe h, const fe f) {
    int i; for (i = 0; i < 10; i++) h[i] = -f[i];
}

static void fe_mul(fe h, const fe f, const fe g) {
    int64_t f0=f[0], f1=f[1], f2=f[2], f3=f[3], f4=f[4], f5=f[5], f6=f[6], f7=f[7], f8=f[8], f9=f[9];
    int64_t g0=g[0], g1=g[1], g2=g[2], g3=g[3], g4=g[4], g5=g[5], g6=g[6], g7=g[7], g8=g[8], g9=g[9];
    int64_t g1_19=19*g1, g2_19=19*g2, g3_19=19*g3, g4_19=19*g4, g5_19=19*g5, g6_19=19*g6, g7_19=19*g7, g8_19=19*g8, g9_19=19*g9;
    int64_t h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, c;

    h0 = f0*g0 + f1*g9_19 + f2*g8_19 + f3*g7_19 + f4*g6_19 + f5*g5_19 + f6*g4_19 + f7*g3_19 + f8*g2_19 + f9*g1_19;
    h1 = f0*g1 + f1*g0    + f2*g9_19 + f3*g8_19 + f4*g7_19 + f5*g6_19 + f6*g5_19 + f7*g4_19 + f8*g3_19 + f9*g2_19;
    h2 = f0*g2 + f1*g1    + f2*g0    + f3*g9_19 + f4*g8_19 + f5*g7_19 + f6*g6_19 + f7*g5_19 + f8*g4_19 + f9*g3_19;
    h3 = f0*g3 + f1*g2    + f2*g1    + f3*g0    + f4*g9_19 + f5*g8_19 + f6*g7_19 + f7*g6_19 + f8*g5_19 + f9*g4_19;
    h4 = f0*g4 + f1*g3    + f2*g2    + f3*g1    + f4*g0    + f5*g9_19 + f6*g8_19 + f7*g7_19 + f8*g6_19 + f9*g5_19;
    h5 = f0*g5 + f1*g4    + f2*g3    + f3*g2    + f4*g1    + f5*g0    + f6*g9_19 + f7*g8_19 + f8*g7_19 + f9*g6_19;
    h6 = f0*g6 + f1*g5    + f2*g4    + f3*g3    + f4*g2    + f5*g1    + f6*g0    + f7*g9_19 + f8*g8_19 + f9*g7_19;
    h7 = f0*g7 + f1*g6    + f2*g5    + f3*g4    + f4*g3    + f5*g2    + f6*g1    + f7*g0    + f8*g9_19 + f9*g8_19;
    h8 = f0*g8 + f1*g7    + f2*g6    + f3*g5    + f4*g4    + f5*g3    + f6*g2    + f7*g1    + f8*g0    + f9*g9_19;
    h9 = f0*g9 + f1*g8    + f2*g7    + f3*g6    + f4*g5    + f5*g4    + f6*g3    + f7*g2    + f8*g1    + f9*g0;

    c = h0>>26; h1 += c; h0 -= c<<26; c = h4>>26; h5 += c; h4 -= c<<26;
    c = h1>>25; h2 += c; h1 -= c<<25; c = h5>>25; h6 += c; h5 -= c<<25;
    c = h2>>26; h3 += c; h2 -= c<<26; c = h6>>26; h7 += c; h6 -= c<<26;
    c = h3>>25; h4 += c; h3 -= c<<25; c = h7>>25; h8 += c; h7 -= c<<25;
    c = h8>>26; h9 += c; h8 -= c<<26; c = h9>>25; h0 += c*19; h9 -= c<<25;
    c = h0>>26; h1 += c; h0 -= c<<26;

    h[0]=h0; h[1]=h1; h[2]=h2; h[3]=h3; h[4]=h4; h[5]=h5; h[6]=h6; h[7]=h7; h[8]=h8; h[9]=h9;
}

static void fe_sq(fe h, const fe f) { fe_mul(h, f, f); }
static void fe_sq2(fe h, const fe f) { fe_sq(h, f); int i; for (i = 0; i < 10; i++) h[i] += h[i]; }

static void fe_invert(fe out, const fe z) {
    fe t0, t1, t2, t3;
    int i;
    fe_sq(t0, z); fe_sq(t1, t0); fe_sq(t1, t1); fe_mul(t1, z, t1); fe_mul(t0, t0, t1);
    fe_sq(t2, t0); fe_mul(t1, t1, t2); fe_sq(t2, t1); for (i = 1; i < 5; i++) fe_sq(t2, t2); fe_mul(t1, t2, t1);
    fe_sq(t2, t1); for (i = 1; i < 10; i++) fe_sq(t2, t2); fe_mul(t2, t2, t1);
    fe_sq(t3, t2); for (i = 1; i < 20; i++) fe_sq(t3, t3); fe_mul(t2, t3, t2);
    fe_sq(t2, t2); for (i = 1; i < 10; i++) fe_sq(t2, t2); fe_mul(t1, t2, t1);
    fe_sq(t2, t1); for (i = 1; i < 50; i++) fe_sq(t2, t2); fe_mul(t2, t2, t1);
    fe_sq(t3, t2); for (i = 1; i < 100; i++) fe_sq(t3, t3); fe_mul(t2, t3, t2);
    fe_sq(t2, t2); for (i = 1; i < 50; i++) fe_sq(t2, t2); fe_mul(t1, t2, t1);
    fe_sq(t1, t1); for (i = 1; i < 5; i++) fe_sq(t1, t1); fe_mul(out, t1, t0);
}

static void fe_tobytes(uint8_t s[32], const fe h) {
    fe t; int64_t c; int i;
    for (i = 0; i < 10; i++) t[i] = h[i];
    for (i = 0; i < 2; i++) {
        c = t[9]>>25; t[0] += c*19; t[9] -= c<<25;
        c = t[0]>>26; t[1] += c; t[0] -= c<<26; c = t[1]>>25; t[2] += c; t[1] -= c<<25;
        c = t[2]>>26; t[3] += c; t[2] -= c<<26; c = t[3]>>25; t[4] += c; t[3] -= c<<25;
        c = t[4]>>26; t[5] += c; t[4] -= c<<26; c = t[5]>>25; t[6] += c; t[5] -= c<<25;
        c = t[6]>>26; t[7] += c; t[6] -= c<<26; c = t[7]>>25; t[8] += c; t[7] -= c<<25;
        c = t[8]>>26; t[9] += c; t[8] -= c<<26;
    }
    s[0] = t[0]; s[1] = t[0]>>8; s[2] = t[0]>>16; s[3] = (t[0]>>24) | (t[1]<<2);
    s[4] = t[1]>>6; s[5] = t[1]>>14; s[6] = (t[1]>>22) | (t[2]<<3);
    s[7] = t[2]>>5; s[8] = t[2]>>13; s[9] = (t[2]>>21) | (t[3]<<5);
    s[10] = t[3]>>3; s[11] = t[3]>>11; s[12] = (t[3]>>19) | (t[4]<<6);
    s[13] = t[4]>>2; s[14] = t[4]>>10; s[15] = t[4]>>18;
    s[16] = t[5]; s[17] = t[5]>>8; s[18] = t[5]>>16; s[19] = (t[5]>>24) | (t[6]<<1);
    s[20] = t[6]>>7; s[21] = t[6]>>15; s[22] = (t[6]>>23) | (t[7]<<3);
    s[23] = t[7]>>5; s[24] = t[7]>>13; s[25] = (t[7]>>21) | (t[8]<<4);
    s[26] = t[8]>>4; s[27] = t[8]>>12; s[28] = (t[8]>>20) | (t[9]<<6);
    s[29] = t[9]>>2; s[30] = t[9]>>10; s[31] = t[9]>>18;
}

static void fe_frombytes(fe h, const uint8_t s[32]) {
    int64_t h0 = s[0] | ((int64_t)s[1] << 8) | ((int64_t)s[2] << 16) | ((int64_t)s[3] << 24);
    int64_t h1 = s[4] | ((int64_t)s[5] << 8) | ((int64_t)s[6] << 16) | ((int64_t)s[7] << 24);
    int64_t h2 = s[8] | ((int64_t)s[9] << 8) | ((int64_t)s[10] << 16) | ((int64_t)s[11] << 24);
    int64_t h3 = s[12] | ((int64_t)s[13] << 8) | ((int64_t)s[14] << 16) | ((int64_t)s[15] << 24);
    int64_t h4 = s[16] | ((int64_t)s[17] << 8) | ((int64_t)s[18] << 16) | ((int64_t)s[19] << 24);
    int64_t h5 = s[20] | ((int64_t)s[21] << 8) | ((int64_t)s[22] << 16) | ((int64_t)s[23] << 24);
    int64_t h6 = s[24] | ((int64_t)s[25] << 8) | ((int64_t)s[26] << 16) | ((int64_t)s[27] << 24);
    int64_t h7 = s[28] | ((int64_t)s[29] << 8) | ((int64_t)s[30] << 16) | ((int64_t)s[31] << 24);

    h[0] = h0 & 0x3ffffff; h[1] = (h0 >> 26) | ((h1 << 6) & 0x1ffffff);
    h[2] = (h1 >> 19) & 0x3ffffff; h[3] = (h1 >> 45) | ((h2 << 13) & 0x1ffffff);
    h[4] = (h2 >> 12) & 0x3ffffff; h[5] = (h2 >> 38) | ((h3 << 20) & 0x1ffffff);
    h[6] = (h3 >> 5) & 0x3ffffff; h[7] = (h3 >> 31) | ((h4 << 27) & 0x1ffffff);
    h[8] = (h4 >> 4) & 0x3ffffff; h[9] = (h5 >> 30) | ((h6 << 8) & 0x1ffffff) | ((h7 << 14) & 0x1ffffff);
}

static int fe_isnegative(const fe f) { uint8_t s[32]; fe_tobytes(s, f); return s[0] & 1; }

#endif /* FE25519_H */
