#include <stdio.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include "bignum_tiny.h"
#include "diffie_hellman.h"
#include "csprng.h"

int main() {
    uint8_t private_key[256], public_key[256];
    struct timespec start, end;

    printf("Timing DH key generation...\n\n");

    if (random_bytes(private_key, 256) < 0) {
        return 1;
    }
    private_key[0] &= 0x7F;

    clock_gettime(CLOCK_MONOTONIC, &start);

    int result = dh_generate_keypair(private_key, public_key);

    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed = (end.tv_sec - start.tv_sec) + 
                    (end.tv_nsec - start.tv_nsec) / 1000000000.0;

    if (result == 0 && public_key[0] != 0) {
        printf("✅ Key generation successful\n");
        printf("Time: %.3f seconds\n", elapsed);
    } else {
        printf("❌ Key generation failed\n");
        return 1;
    }

    return 0;
}
