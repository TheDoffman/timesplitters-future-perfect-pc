#include "pc_hang_policy.h"
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Watchdog policy failure at %d\n", __LINE__); return 1; } } while(0)

int main(void)
{
    PcHangPolicy p = {0};
    CHECK(!pc_hang_poll(&p, 100, 0, 0, 0, 1000, 4000));
    CHECK(!pc_hang_poll(&p, 10000, 0, 0, 0, 1000, 4000));
    CHECK(!pc_hang_poll(&p, 11000, 0, 1, 0, 1000, 4000));
    CHECK(!pc_hang_poll(&p, 14000, 0, 1, 0, 1000, 4000));
    CHECK(pc_hang_poll(&p, 15000, 0, 1, 0, 1000, 4000));
    CHECK(!pc_hang_poll(&p, 16000, 0, 1, 0, 1000, 4000));
    CHECK(!pc_hang_poll(&p, 16100, 1, 1, 0, 1000, 4000));
    CHECK(!pc_hang_poll(&p, 17000, 1, 1, 0, 1000, 4000));
    CHECK(pc_hang_poll(&p, 17100, 1, 1, 0, 1000, 4000));
    CHECK(!pc_hang_poll(&p, 18000, 1, 1, 1, 1000, 4000));
    CHECK(!pc_hang_poll(&p, 20000, 1, 1, 0, 1000, 4000)); /* Still same incident. */
    CHECK(!pc_hang_poll(&p, 20100, 2, 1, 0, 1000, 4000));
    CHECK(!pc_hang_poll(&p, 21000, 2, 1, 1, 1000, 4000));
    CHECK(!pc_hang_poll(&p, 22000, 2, 1, 1, 1000, 4000));
    CHECK(!pc_hang_poll(&p, 22500, 2, 1, 0, 1000, 4000));
    CHECK(pc_hang_poll(&p, 23000, 2, 1, 0, 1000, 4000));
    CHECK(!pc_hang_poll(&p, 23100, 3, 1, 0, 1000, 4000));
    CHECK(!pc_hang_poll(&p, 50000, 3, 1, 0, 1000, 4000)); /* Sleep/resume resets grace. */
    CHECK(!pc_hang_poll(&p, 50999, 3, 1, 0, 1000, 4000));
    CHECK(pc_hang_poll(&p, 51000, 3, 1, 0, 1000, 4000));
    puts("PASS watchdog startup/active grace, one report per stall, recovery, modal/background suppression and sleep/resume.");
    return 0;
}
