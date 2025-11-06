#include <stdio.h>
#include <stdint.h>
#include "bignum_trace.h"
#include "rsa.h"

int main() {
    rsa_key_t key;
    bn_t m, c;

    rsa_init_key(&key);

    bn_zero(&m);
    m.array[0] = 42;

    printf("=== Tracing m^e mod n where m=42, e=65537 ===\n");
    printf("m.array[0] = %u\n", m.array[0]);
    printf("e.array[0] = %u\n", key.e.array[0]);
    printf("n.array[0] = %08x\n\n", key.n.array[0]);

    bn_modexp_trace(&c, &m, &key.e, &key.n);

    printf("\nResult: c.array[0] = %u\n", c.array[0]);

    return 0;
}
