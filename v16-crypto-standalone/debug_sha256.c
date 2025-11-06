#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "sha256_minimal.h"

int main(void) {
    const char *input = "The quick brown fox jumps over the lazy dog. This is a test.";
    uint8_t hash[32];
    
    printf("Input: '%s'\n", input);
    printf("Length: %zu\n\n", strlen(input));
    
    sha256(hash, (const uint8_t *)input, strlen(input));
    
    printf("SHA-256: ");
    for (int i = 0; i < 32; i++) {
        printf("%02x", hash[i]);
    }
    printf("\n");
    
    printf("Expected: 0087b96e24a6926179ac34ea0ce344f6034b49685eaab46632c5c18c415296ba\n");
    
    return 0;
}
