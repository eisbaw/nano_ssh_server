#include <stdio.h>
#include <stdint.h>
#include "rsa.h"

void print_bn_hex(const char *name, const bn_t *x, int start, int end) {
    uint8_t bytes[256];
    bn_to_bytes(x, bytes, 256);
    printf("%s (bytes[%d..%d]): ", name, start, end);
    for (int i = start; i <= end; i++) {
        printf("%02x", bytes[i]);
    }
    printf("\n");
}

int main() {
    rsa_key_t key;
    rsa_init_key(&key);

    printf("=== RSA Key Check ===\n");
    print_bn_hex("n", &key.n, 0, 31);
    print_bn_hex("e (first)", &key.e, 0, 7);
    print_bn_hex("e (last)", &key.e, 248, 255);
    print_bn_hex("d", &key.d, 0, 31);

    printf("\nDirect check - key.e.array[0] = %u (should be 65537)\n", key.e.array[0]);
    
    return 0;
}
