#define _CRT_SECURE_NO_WARNINGS
#include "guest_allocation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char ram[65536], physical[131072];
static tsfp_allocations state;
static tsfp_allocation_record heap_records[1024], physical_records[128];

static int start(size_t heap_capacity, size_t physical_capacity) {
    tsfp_allocation_config config = {0};
    config.ram = ram; config.ram_size = sizeof(ram);
    config.heap_begin = 256; config.reserve_begin = 32768;
    config.physical = physical; config.physical_base = 0x80000000u;
    config.physical_size = sizeof(physical); config.physical_begin = 0x80004000u;
    config.physical_end = 0x8001e000u;
    config.protected_begin = 0x80001000u; config.protected_end = 0x80002000u;
    return tsfp_allocations_init(&state, &config, heap_records, heap_capacity,
                                 physical_records, physical_capacity);
}

int main(int argc, char **argv) {
    FILE *dump;
    char command;
    unsigned a, b, low, high, result;
    if (argc != 4) return 2;
    memset(ram, 0xa5, sizeof(ram)); memset(physical, 0xb6, sizeof(physical));
    if (!start(strtoul(argv[2], NULL, 0), strtoul(argv[3], NULL, 0))) return 3;
    dump = fopen(argv[1], "wb"); if (!dump) return 4;
    while (scanf(" %c %u %u", &command, &a, &b) == 3) {
        switch (command) {
        case 'H': result = tsfp_heap_allocate(&state, a, b); break;
        case 'R': result = tsfp_reserve_allocate(&state, a, b); break;
        case 'C': result = tsfp_physical_allocate(&state, a, b); break;
        case 'B':
            if (scanf("%u %u", &low, &high) != 2) return 10;
            result = tsfp_physical_allocate_range(&state, a, b, low, high); break;
        case 'P': result = tsfp_physical_allocate_at(&state, a, b); break;
        case 'F': result = (unsigned)tsfp_heap_release(&state, a); break;
        case 'G': result = (unsigned)tsfp_physical_release(&state, a); break;
        case 'Q': result = tsfp_allocation_remaining(&state, a); break;
        case 'S': tsfp_allocations_stop(&state); result = 0; break;
        case 'N': result = (unsigned)start(a, b); break;
        case 'D':
            if (fwrite(ram, 1, sizeof(ram), dump) != sizeof(ram) ||
                fwrite(physical, 1, sizeof(physical), dump) != sizeof(physical)) return 5;
            result = 0; break;
        case 'W':
            if (a >= sizeof(ram)) return 6;
            ram[a] = (unsigned char)b; result = 0; break;
        case 'V':
            if (a >= sizeof(physical)) return 7;
            physical[a] = (unsigned char)b; result = 0; break;
        default: return 8;
        }
        printf("%u %u", result, tsfp_physical_extent(&state));
        for (size_t i = 0; ; ++i) {
            uint32_t address = 0, bytes = 0;
            if (!tsfp_physical_block(&state, i, &address, &bytes)) break;
            printf(" %u %u", address, bytes);
        }
        putchar('\n');
    }
    if (fclose(dump)) return 9;
    return 0;
}
