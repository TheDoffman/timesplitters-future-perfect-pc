#include "native/crash_posix.h"
#include "pc_breadcrumbs.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void guest(uint32_t r[8]){for(unsigned i=0;i<8;i++)r[i]=0x12340000+i;}
__attribute__((noinline)) void tsfp_native_crash_test_site(void){raise(SIGABRT);}
int main(int argc,char **argv){
    int result=tsfp_native_crash_helper(argc,argv);if(result>=0)return result;
    if(argc!=2 || !tsfp_native_crash_init(guest))return 2;
    pc_capture_gameplay(4,4,0,1,2,0);pc_capture_model(0x811ee970u,15,123);pc_capture_allocation(1,0x10000,4096);pc_capture_allocation(257,0x10000,4096);
    struct sigaction action={0};action.sa_sigaction=tsfp_native_crash_signal;action.sa_flags=SA_SIGINFO|SA_ONSTACK;sigemptyset(&action.sa_mask);sigaction(SIGABRT,&action,NULL);
    if(!strcmp(argv[1],"crash")){tsfp_native_crash_test_site();return 3;}
    if(!strcmp(argv[1],"unarmed")){usleep(1800000);return 0;}
    tsfp_native_crash_armed(1);
    if(!strcmp(argv[1],"startup")){usleep(800000);tsfp_native_crash_frame();return 0;}
    tsfp_native_crash_frame();
    if(!strcmp(argv[1],"background"))tsfp_native_crash_focus(0);
    if(!strcmp(argv[1],"modal"))tsfp_native_crash_inhibit(1);
    if(!strcmp(argv[1],"responsive")){for(int i=0;i<20;i++){tsfp_native_crash_frame();usleep(100000);}return 0;}
    usleep(2200000);
    if(!strcmp(argv[1],"twice")){tsfp_native_crash_frame();usleep(2200000);}
    if(!strcmp(argv[1],"then-crash"))tsfp_native_crash_test_site();
    return 0;
}
