/* Shared ABI check: native fallback must not reference the Windows transport. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Select only the guest adapter after including the host's standard headers. */
#ifdef TSFP_TEST_NATIVE_FALLBACK
#undef _WIN32
#elif !defined(_WIN32)
#define _WIN32 1
#endif

static uint32_t memory[32],g_eax,g_esp;
static ptrdiff_t g_xbox_mem_offset;
static int originals,enabled;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)

#ifdef _WIN32
int pc_network_enabled(void) { return enabled; }
unsigned pc_network_arguments(uint32_t address) { return address==0x1234u?2u:0u; }
uint32_t pc_network_call(uint32_t address,const uint32_t *args,ptrdiff_t offset)
{
    return address==0x1234u && args[0]==17 && args[1]==29 && offset==g_xbox_mem_offset
        ? 0x55667788u : 0;
}
#endif

#include "../src/pc_network_guest.inc"

static void original(void) { originals++;g_eax=0x11223344u;g_esp+=4; }

int main(void)
{
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    g_esp=16;memory[5]=17;memory[6]=29;
    pc_network_guest(0x1234u,original);
    CHECK(originals==1 && g_eax==0x11223344u && g_esp==20);
    enabled=1;g_esp=16;
    pc_network_guest(0x1234u,original);
#ifdef TSFP_TEST_NATIVE_FALLBACK
    CHECK(originals==2 && g_eax==0x11223344u && g_esp==20);
    puts("PASS: native fallback retains original call/register/stack behavior without host networking");
#else
    CHECK(originals==1 && g_eax==0x55667788u && g_esp==28);
    puts("PASS: Windows transport dispatch and disabled fallback retain the guest ABI");
#endif
    return 0;
}
