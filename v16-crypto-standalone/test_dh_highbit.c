#include <stdio.h>
#include <errno.h>
#include "diffie_hellman.h"

int main() {
    uint8_t priv[256], pub[256];
    
    printf("Testing if DH public keys have high bit set...\n\n");
    int high_bit_count = 0;
    for (int i = 0; i < 20; i++) {
        if (dh_generate_keypair(priv, pub) < 0) {
            printf("Failed\n");
            return 1;
        }
        int has_high_bit = (pub[0] & 0x80) ? 1 : 0;
        high_bit_count += has_high_bit;
        printf("Key %2d: First byte = 0x%02x %s\n",
               i+1, pub[0], has_high_bit ? "← HIGH BIT SET (looks negative!)" : "");
    }
    
    printf("\n");
    printf("Result: %d/%d keys have high bit set\n", high_bit_count, 20);
    printf("\nConclusion: DH public keys MUST be encoded as mpint\n");
    printf("(with leading 0x00 byte if high bit set), not as raw string!\n");
    return 0;
}
