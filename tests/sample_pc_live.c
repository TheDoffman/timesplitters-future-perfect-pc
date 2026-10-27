/* Exercise the real Windows collector/DbgHelp/PDB path without game assets. */
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
void xbox_SamplerStart(void);
static volatile uint32_t sample_value;
__declspec(noinline) void sampling_pc_busy(void) {
    LARGE_INTEGER frequency,start,now;
    QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&start);
    do {
        for(unsigned i=0;i<100000;i++)sample_value=(sample_value^i)*1664525u+1013904223u;
        QueryPerformanceCounter(&now);
    } while(now.QuadPart-start.QuadPart<frequency.QuadPart);
}
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    _putenv_s("RECOMP_SAMPLE","200");_putenv_s("RECOMP_SAMPLE_REPORT","0.25");
    _putenv_s("RECOMP_SAMPLE_DEPTH","1");
    _putenv_s("RECOMP_SAMPLE_PC_SYMBOL","sampling_pc_busy");
    _putenv_s("RECOMP_SAMPLE_DUMP",argv[1]);
    xbox_SamplerStart();sampling_pc_busy();Sleep(100);return 0;
}
