/* Unsigned displacement oracle uses a widened modulus. Its argument deliberately
 * changes DF, making both duplicate evaluation and a late direction read visible. */
#include <stdint.h>
#include <stdio.h>
#include "under_test.h"
RECOMP_TLS int g_df;
static uint32_t argument_calls, incoming;
static uint32_t argument(void) { ++argument_calls; g_df = !g_df; return incoming; }
static int check(int direction, uint32_t bits)
{
    const uint32_t before = argument_calls;
    const uint32_t expected = direction ? (uint32_t)(UINT64_C(0x100000000)-bits) : bits;
    g_df = direction; incoming = bits;
    const uint32_t observed = RECOMP_DF_STEP(argument());
    if (observed != expected || argument_calls != before+1u || g_df != !direction) {
        puts("direction mismatch"); return 1;
    }
    return 0;
}
int main(void)
{
    static const int directions[] = {0,1,-1,17};
    static const uint32_t edges[] = {0u,1u,2u,4u,0x7fffffffu,0x80000000u,0xffffffffu};
    uint32_t seed = 0x46b17923u;
    for (unsigned d=0; d<4; ++d) {
        for (unsigned i=0; i<7; ++i) if(check(directions[d],edges[i])) return 1;
        for (unsigned i=0; i<65536; ++i) {
            seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
            if(check(directions[d],seed)) return 1;
        }
    }
    printf("%u direction cases passed\n", argument_calls);
    return 0;
}
