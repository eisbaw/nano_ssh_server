/*
 * Profile modexp to identify bottlenecks
 */
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <errno.h>

/* Counters for profiling */
unsigned long bn_mul_wide_calls = 0;
unsigned long bn_mod_wide_calls = 0;
unsigned long bn_mod_wide_iterations = 0;
double bn_mul_wide_time = 0;
double bn_mod_wide_time = 0;

#define BN_WORDS 64
#define BN_BYTES 256
#define BN_2X_WORDS 128

typedef struct { uint32_t array[BN_WORDS]; } bn_t;
typedef struct { uint32_t array[BN_2X_WORDS]; } bn_2x_t;

static inline void bn_zero(bn_t *a) { memset(a, 0, sizeof(bn_t)); }
static inline void bn_2x_zero(bn_2x_t *a) { memset(a, 0, sizeof(bn_2x_t)); }

static inline int bn_is_zero(const bn_t *a) {
    for (int i = 0; i < BN_WORDS; i++) {
        if (a->array[i] != 0) return 0;
    }
    return 1;
}

static inline void bn_from_bytes(bn_t *a, const uint8_t *bytes, size_t len) {
    bn_zero(a);
    if (len > BN_BYTES) len = BN_BYTES;
    for (size_t i = 0; i < len; i++) {
        int word_idx = (len - 1 - i) / 4;
        int byte_idx = (len - 1 - i) % 4;
        a->array[word_idx] |= ((uint32_t)bytes[i]) << (byte_idx * 8);
    }
}

static int bn_cmp(const bn_t *a, const bn_t *b) {
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (a->array[i] < b->array[i]) return -1;
        if (a->array[i] > b->array[i]) return 1;
    }
    return 0;
}

static inline uint32_t mul_add_word(uint32_t *result, uint32_t a, uint32_t b, uint32_t carry) {
    uint64_t product = (uint64_t)a * b + *result + carry;
    *result = (uint32_t)product;
    return (uint32_t)(product >> 32);
}

static void bn_mul_wide(bn_2x_t *r, const bn_t *a, const bn_t *b) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    bn_mul_wide_calls++;
    bn_2x_zero(r);

    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t carry = 0;
        for (int j = 0; j < BN_WORDS; j++) {
            carry = mul_add_word(&r->array[i + j], a->array[i], b->array[j], carry);
        }
        if (i + BN_WORDS < BN_2X_WORDS) {
            r->array[i + BN_WORDS] = carry;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    bn_mul_wide_time += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

static int bn_2x_cmp_mod(const bn_2x_t *a, const bn_t *m) {
    for (int i = BN_2X_WORDS - 1; i >= BN_WORDS; i--) {
        if (a->array[i] != 0) return 1;
    }
    for (int i = BN_WORDS - 1; i >= 0; i--) {
        if (a->array[i] < m->array[i]) return -1;
        if (a->array[i] > m->array[i]) return 1;
    }
    return 0;
}

static void bn_mod_wide(bn_t *r, const bn_2x_t *a, const bn_t *m) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    bn_mod_wide_calls++;
    bn_2x_t temp = *a;

    /* Fast path */
    int temp_high = -1;
    for (int i = BN_2X_WORDS - 1; i >= 0; i--) {
        if (temp.array[i] != 0) {
            temp_high = i;
            break;
        }
    }

    if (temp_high < BN_WORDS) {
        int cmp = 0;
        for (int i = BN_WORDS - 1; i >= 0; i--) {
            if (temp.array[i] < m->array[i]) {
                cmp = -1;
                break;
            }
            if (temp.array[i] > m->array[i]) {
                cmp = 1;
                break;
            }
        }

        if (cmp < 0) {
            bn_zero(r);
            for (int i = 0; i < BN_WORDS; i++) {
                r->array[i] = temp.array[i];
            }
            clock_gettime(CLOCK_MONOTONIC, &end);
            bn_mod_wide_time += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
            return;
        }
    }

    /* Binary long division */
    int iterations = 0;
    int max_iterations = 4096;

    while (iterations < max_iterations) {
        bn_mod_wide_iterations++;

        int cmp = bn_2x_cmp_mod(&temp, m);
        if (cmp < 0) break;

        /* Find MSB words */
        int temp_msb_word = -1;
        for (int i = BN_2X_WORDS - 1; i >= 0; i--) {
            if (temp.array[i] != 0) {
                temp_msb_word = i;
                break;
            }
        }

        int m_msb_word = -1;
        for (int i = BN_WORDS - 1; i >= 0; i--) {
            if (m->array[i] != 0) {
                m_msb_word = i;
                break;
            }
        }

        if (temp_msb_word < 0 || m_msb_word < 0) break;

        int word_shift = temp_msb_word - m_msb_word;
        if (word_shift < 0) word_shift = 0;

        /* Shift m */
        bn_2x_t m_shifted;
        bn_2x_zero(&m_shifted);
        for (int i = 0; i < BN_WORDS && i + word_shift < BN_2X_WORDS; i++) {
            m_shifted.array[i + word_shift] = m->array[i];
        }

        /* Try to subtract */
        int can_subtract = 1;
        for (int i = BN_2X_WORDS - 1; i >= 0; i--) {
            if (temp.array[i] < m_shifted.array[i]) {
                can_subtract = 0;
                break;
            }
            if (temp.array[i] > m_shifted.array[i]) {
                break;
            }
        }

        if (can_subtract) {
            uint32_t borrow = 0;
            for (int i = 0; i < BN_2X_WORDS; i++) {
                uint32_t next_borrow = (temp.array[i] < m_shifted.array[i] + borrow) ? 1 : 0;
                temp.array[i] = temp.array[i] - m_shifted.array[i] - borrow;
                borrow = next_borrow;
            }
        } else if (word_shift > 0) {
            word_shift--;
            bn_2x_zero(&m_shifted);
            for (int i = 0; i < BN_WORDS && i + word_shift < BN_2X_WORDS; i++) {
                m_shifted.array[i + word_shift] = m->array[i];
            }

            uint32_t borrow = 0;
            for (int i = 0; i < BN_2X_WORDS; i++) {
                uint32_t next_borrow = (temp.array[i] < m_shifted.array[i] + borrow) ? 1 : 0;
                temp.array[i] = temp.array[i] - m_shifted.array[i] - borrow;
                borrow = next_borrow;
            }
        } else {
            break;
        }

        iterations++;
    }

    /* Copy result */
    bn_zero(r);
    for (int i = 0; i < BN_WORDS; i++) {
        r->array[i] = temp.array[i];
    }

    /* Final cleanup */
    if (bn_cmp(r, m) >= 0) {
        bn_t temp_result;
        uint32_t borrow = 0;
        for (int i = 0; i < BN_WORDS; i++) {
            uint32_t next_borrow = (r->array[i] < m->array[i] + borrow) ? 1 : 0;
            temp_result.array[i] = r->array[i] - m->array[i] - borrow;
            borrow = next_borrow;
        }
        *r = temp_result;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    bn_mod_wide_time += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

static void bn_mulmod(bn_t *r, const bn_t *a, const bn_t *b, const bn_t *m) {
    bn_2x_t product;
    bn_mul_wide(&product, a, b);
    bn_mod_wide(r, &product, m);
}

static void bn_modexp(bn_t *r, const bn_t *base, const bn_t *exp, const bn_t *mod) {
    bn_t result, temp_base;

    bn_zero(&result);
    result.array[0] = 1;

    bn_2x_t base_wide;
    bn_2x_zero(&base_wide);
    for (int i = 0; i < BN_WORDS; i++) {
        base_wide.array[i] = base->array[i];
    }
    bn_mod_wide(&temp_base, &base_wide, mod);

    for (int i = 0; i < BN_WORDS; i++) {
        uint32_t exp_word = exp->array[i];
        for (int j = 0; j < 32; j++) {
            if (exp_word & 1) {
                bn_mulmod(&result, &result, &temp_base, mod);
            }
            bn_mulmod(&temp_base, &temp_base, &temp_base, mod);
            exp_word >>= 1;
        }
    }

    *r = result;
}

/* DH Group14 prime */
static const uint8_t dh_group14_prime[256] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34,
    0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
    0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74,
    0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22,
    0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
    0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B,
    0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37,
    0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
    0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6,
    0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x37, 0xED, 0x6B,
    0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
    0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5,
    0xAE, 0x9F, 0x24, 0x11, 0x7C, 0x4B, 0x1F, 0xE6,
    0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D,
    0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05,
    0x98, 0xDA, 0x48, 0x36, 0x1C, 0x55, 0xD3, 0x9A,
    0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,
    0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96,
    0x1C, 0x62, 0xF3, 0x56, 0x20, 0x85, 0x52, 0xBB,
    0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D,
    0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04,
    0xF1, 0x74, 0x6C, 0x08, 0xCA, 0x18, 0x21, 0x7C,
    0x32, 0x90, 0x5E, 0x46, 0x2E, 0x36, 0xCE, 0x3B,
    0xE3, 0x9E, 0x77, 0x2C, 0x18, 0x0E, 0x86, 0x03,
    0x9B, 0x27, 0x83, 0xA2, 0xEC, 0x07, 0xA2, 0x8F,
    0xB5, 0xC5, 0x5D, 0xF0, 0x6F, 0x4C, 0x52, 0xC9,
    0xDE, 0x2B, 0xCB, 0xF6, 0x95, 0x58, 0x17, 0x18,
    0x39, 0x95, 0x49, 0x7C, 0xEA, 0x95, 0x6A, 0xE5,
    0x15, 0xD2, 0x26, 0x18, 0x98, 0xFA, 0x05, 0x10,
    0x15, 0x72, 0x8E, 0x5A, 0x8A, 0xAC, 0xAA, 0x68,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

/* Simple CSPRNG */
#include <fcntl.h>
#include <unistd.h>
static int random_bytes(uint8_t *buf, size_t len) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return -1;
    ssize_t n = read(fd, buf, len);
    close(fd);
    return (n == (ssize_t)len) ? 0 : -1;
}

int main() {
    uint8_t private_key[256];
    bn_t priv, pub, prime, generator;
    struct timespec start, end;

    printf("========================================\n");
    printf("Profiling DH Key Generation\n");
    printf("========================================\n\n");

    if (random_bytes(private_key, 256) < 0) {
        return 1;
    }
    private_key[0] &= 0x7F;

    bn_from_bytes(&priv, private_key, 256);
    bn_from_bytes(&prime, dh_group14_prime, 256);
    bn_zero(&generator);
    generator.array[0] = 2;

    clock_gettime(CLOCK_MONOTONIC, &start);
    bn_modexp(&pub, &generator, &priv, &prime);
    clock_gettime(CLOCK_MONOTONIC, &end);

    double total_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("Total time: %.3f seconds\n\n", total_time);
    printf("Function Call Statistics:\n");
    printf("  bn_mul_wide: %lu calls, %.3f sec (%.1f%%)\n",
           bn_mul_wide_calls, bn_mul_wide_time,
           100.0 * bn_mul_wide_time / total_time);
    printf("  bn_mod_wide: %lu calls, %.3f sec (%.1f%%)\n",
           bn_mod_wide_calls, bn_mod_wide_time,
           100.0 * bn_mod_wide_time / total_time);
    printf("\n");
    printf("bn_mod_wide internals:\n");
    printf("  Total iterations: %lu\n", bn_mod_wide_iterations);
    printf("  Avg iterations per call: %.1f\n",
           (double)bn_mod_wide_iterations / bn_mod_wide_calls);
    printf("\n");
    printf("Bottleneck: ");
    if (bn_mod_wide_time > bn_mul_wide_time) {
        printf("bn_mod_wide (%.1f%% of total)\n", 100.0 * bn_mod_wide_time / total_time);
        printf("Recommendation: Optimize modular reduction\n");
    } else {
        printf("bn_mul_wide (%.1f%% of total)\n", 100.0 * bn_mul_wide_time / total_time);
        printf("Recommendation: Optimize multiplication\n");
    }

    return 0;
}
