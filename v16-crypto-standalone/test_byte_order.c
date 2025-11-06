#include <stdio.h>
#include <stdint.h>
#include "bignum_v17.h"

int main() {
    /* Small test: load 0x12345678 */
    uint8_t bytes[4] = {0x12, 0x34, 0x56, 0x78};
    bn_t n;

    bn_from_bytes(&n, bytes, 4);

    printf("Loaded bytes: 12 34 56 78\n");
    printf("n.array[0] = 0x%08x\n", n.array[0]);
    printf("Expected: 0x12345678 (big-endian) or 0x78563412 (little-endian issue)\n\n");

    /* Test with actual RSA modulus first 4 bytes */
    uint8_t rsa_bytes[4] = {0xa7, 0x3e, 0x9d, 0x97};
    bn_from_bytes(&n, rsa_bytes, 4);

    printf("Loaded RSA first 4 bytes: a7 3e 9d 97\n");
    printf("n.array[0] = 0x%08x\n", n.array[0]);
    printf("Expected: 0xa73e9d97\n");

    return 0;
}
