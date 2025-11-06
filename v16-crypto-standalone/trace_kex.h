/*
 * KEX tracing utilities
 */
#ifndef TRACE_KEX_H
#define TRACE_KEX_H

#include <stdio.h>
#include <stdint.h>

static void trace_hex(const char *label, const uint8_t *data, size_t len) {
    fprintf(stderr, "TRACE [%s] (%zu bytes):\n", label, len);
    for (size_t i = 0; i < len && i < 128; i += 16) {
        fprintf(stderr, "  %04zx: ", i);
        for (size_t j = 0; j < 16 && i + j < len; j++) {
            fprintf(stderr, "%02x ", data[i + j]);
        }
        fprintf(stderr, "\n");
    }
    if (len > 128) {
        fprintf(stderr, "  ... (%zu more bytes)\n", len - 128);
    }
}

static void trace_mpint(const char *label, const uint8_t *data, size_t len) {
    fprintf(stderr, "TRACE [%s] mpint: ", label);
    if (len == 0) {
        fprintf(stderr, "(empty)\n");
        return;
    }
    // Show first and last few bytes
    if (len <= 32) {
        for (size_t i = 0; i < len; i++) {
            fprintf(stderr, "%02x", data[i]);
        }
    } else {
        for (size_t i = 0; i < 16; i++) {
            fprintf(stderr, "%02x", data[i]);
        }
        fprintf(stderr, "...");
        for (size_t i = len - 8; i < len; i++) {
            fprintf(stderr, "%02x", data[i]);
        }
    }
    fprintf(stderr, " (%zu bytes, first_byte=0x%02x)\n", len, data[0]);
}

#endif
