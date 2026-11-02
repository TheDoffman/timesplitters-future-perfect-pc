#define _GNU_SOURCE 1
#include "x64_access.h"
#include "mmio_device.h"
#include <signal.h>
#include <ucontext.h>
int tsfp_native_mmio(siginfo_t *info,void *context) {
    ucontext_t *uc=context;
    static const int index[16]={REG_RAX,REG_RCX,REG_RDX,REG_RBX,REG_RSP,REG_RBP,REG_RSI,REG_RDI,
        REG_R8,REG_R9,REG_R10,REG_R11,REG_R12,REG_R13,REG_R14,REG_R15};
    uint64_t regs[16];for(unsigned i=0;i<16;i++)regs[i]=(uint64_t)uc->uc_mcontext.gregs[index[i]];
    uintptr_t pc=(uintptr_t)uc->uc_mcontext.gregs[REG_RIP];TsfpX64Access a;
    if(!tsfp_x64_access((const unsigned char *)pc,15,pc,regs,&a)||a.address!=(uintptr_t)info->si_addr)return 0;
    uint64_t value=a.has_immediate?a.immediate:(regs[a.reg]>>a.shift);
    if(a.bytes<8)value&=(UINT64_C(1)<<(a.bytes*8))-1;
    if(!tsfp_mmio_device(a.address,a.bytes,a.load,&value))return 0;
    if(a.load)uc->uc_mcontext.gregs[index[a.reg]]=(greg_t)tsfp_x64_load_result(regs[a.reg],value,&a);
    uc->uc_mcontext.gregs[REG_RIP]+=(greg_t)a.length;
    return 1;
}
