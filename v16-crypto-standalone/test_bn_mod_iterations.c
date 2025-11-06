#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "bignum_tiny.h"
#include "diffie_hellman.h"

/* Modified bn_mod with iteration counter */
static void bn_mod_debug(bn_t *r, const bn_t *a, const bn_t *m, int *iterations_out) {
    *r = *a;

    /* Fast path: if a < m, done */
    if (bn_cmp(r, m) < 0) {
        *iterations_out = 0;
        return;
    }

    int max_iterations = 4096;
    int iterations = 0;

    /* Repeated subtraction until r < m */
    while (bn_cmp(r, m) >= 0 && iterations < max_iterations) {
        bn_sub(r, r, m);
        iterations++;
    }

    *iterations_out = iterations;

    /* If we hit the limit, something is wrong - try binary method */
    if (iterations >= max_iterations) {
        printf("  [Hit iteration limit, using binary method]\n");
        /* Binary long division fallback... */
    }
}

int main() {
    bn_t temp_base, temp, prime;
    int iters;

    bn_from_bytes(&prime, dh_group14_prime, 256);

    /* Simulate the problematic sequence from modexp */
    printf("Simulating modexp squaring loop:\n\n");

    bn_zero(&temp_base);
    temp_base.array[0] = 2;  /* Start with 2 */

    for (int j = 0; j < 20; j++) {
        printf("Iteration j=%d:\n", j);
        printf("  Before: temp_base.array[0]=%u, array[1]=%u\n",
               temp_base.array[0], temp_base.array[1]);

        /* Square temp_base */
        bn_mul(&temp, &temp_base, &temp_base);
        printf("  After mul: temp.array[0]=%u, array[1]=%u\n",
               temp.array[0], temp.array[1]);

        /* Reduce mod prime */
        bn_mod_debug(&temp_base, &temp, &prime, &iters);
        printf("  After mod: temp_base.array[0]=%u, array[1]=%u (iters=%d)\n",
               temp_base.array[0], temp_base.array[1], iters);

        if (bn_is_zero(&temp_base)) {
            printf("  ❌ temp_base became ZERO!\n");
            break;
        }

        printf("\n");
    }

    return 0;
}
