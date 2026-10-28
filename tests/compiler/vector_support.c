/* The oracle uses raw IEEE single representations, including NaN payloads,
 * signed zeros, infinities and subnormals. No floating arithmetic is involved. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "under_test.h"

static uint32_t evaluations;
static float argument(uint32_t bits)
{
    float value;
    ++evaluations;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int check(uint32_t bits)
{
    const uint32_t prior = evaluations;
    RecompXmm value = XMM_SCALAR(argument(bits));
    uint32_t observed[4];
    memcpy(observed, &value, sizeof(observed));
    if (evaluations != prior + 1u || observed[0] != bits ||
        observed[1] || observed[2] || observed[3]) {
        printf("vector mismatch %08x -> %08x %08x %08x %08x\n",
               bits, observed[0], observed[1], observed[2], observed[3]);
        return 1;
    }
    return 0;
}

int main(void)
{
    static const uint32_t edges[] = {0u, 0x80000000u, 1u, 0x807fffffu,
        0x00800000u, 0x7f7fffffu, 0x7f800000u, 0xff800000u,
        0x7fc01234u, 0xffc0abcdu, 0x7f800001u, 0xff800001u};
    uint32_t state = 0x4e1c9b27u;
    unsigned i;
    for (i = 0; i < sizeof(edges)/sizeof(edges[0]); ++i)
        if (check(edges[i])) return 1;
    for (i = 0; i < 131072u; ++i) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        if (check(state)) return 1;
    }
    printf("%u vector cases passed\n", evaluations);
    return 0;
}
