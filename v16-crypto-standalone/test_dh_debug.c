#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "bignum_tiny.h"
#include "diffie_hellman.h"
#include "csprng.h"

int main() {
    uint8_t priv[256], pub[256];
    
    memset(priv, 0, 256);
    priv[255] = 100;  // Small private key
    
    printf("Test 1: Small private key (100)\n");
    if (dh_generate_keypair is available)  {
        // Generate manually
        bn_t priv_bn, pub_bn, prime_bn, gen_bn;
        
        bn_from_bytes(&priv_bn, priv, 256);
        bn_from_bytes(&prime_bn, dh_group14_prime, 256);
        bn_zero(&gen_bn);
        gen_bn.array[0] = 2;
        bn_zero(&pub_bn);
        
        bn_modexp(&pub_bn, &gen_bn, &priv_bn, &prime_bn);
        bn_to_bytes(&pub_bn, pub, 256);
        
        printf("Public key: ");
        for (int i = 0; i < 32; i++) printf("%02x", pub[i]);
        printf("...\n");
        
        if (pub[255] == 0 && pub[254] == 0 && pub[253] == 0) {
            printf("❌ Result looks like zeros\n");
        } else {
            printf("✅ Result has data\n");
        }
    }
    
    return 0;
}
