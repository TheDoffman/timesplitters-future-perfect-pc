#include "kernel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if(!(c)) {fprintf(stderr,"boot line%d: %s\n",__LINE__,#c);exit(1);} } while(0)
#undef EnterCriticalSection
#undef LeaveCriticalSection
static LONG enters,leaves;
void boot_enter(CRITICAL_SECTION *lock) {
    EnterCriticalSection(lock);InterlockedIncrement(&enters);
}
void boot_leave(CRITICAL_SECTION *lock) {
    InterlockedIncrement(&leaves);LeaveCriticalSection(lock);
}
static unsigned order;
void xbox_GuestLockInit(void) {CHECK(order==0);order=1;}
void xbox_GuestLockEnter(void) {CHECK(order==1);order=2;}
void xbox_GuestLiftedEnter(void) {CHECK(order==2);order=3;}
void xbox_hal_init_timers(void) {CHECK(order==3);order=4;}
static DWORD WINAPI worker(void *context) {
    unsigned i,id=(unsigned)(uintptr_t)context;
    for(i=0;i<100;++i)xbox_log(XBOX_LOG_INFO,"WORK","ROW:%u:%u:END",id,i);
    return 0;
}
int main(int argc,char **argv) {
    FILE *f;char line[512];unsigned count=0,i,j,seen[4][100]={{0}};HANDLE threads[4];
    CHECK(argc==2);xbox_kernel_shutdown();xbox_kernel_init();CHECK(order==4);
    xbox_kernel_init();CHECK(order==4);
    if(!strcmp(argv[1],"threads")) {
        for(i=0;i<4;++i){threads[i]=CreateThread(NULL,0,worker,(void *)(uintptr_t)i,0,NULL);CHECK(threads[i]);}
        CHECK(WaitForMultipleObjects(4,threads,TRUE,10000)==WAIT_OBJECT_0);
        for(i=0;i<4;++i)CHECK(CloseHandle(threads[i]));
    }
    xbox_log(XBOX_LOG_ERROR,"CHECK","ERROR_MARK");
    xbox_log(XBOX_LOG_DEBUG,"CHECK","DEBUG_MARK");
    xbox_log(XBOX_LOG_TRACE,"CHECK","TRACE_MARK");
    xbox_log(XBOX_LOG_INFO,NULL,NULL);
    xbox_kernel_shutdown();xbox_kernel_shutdown();
    if(strcmp(argv[1],"threads"))return 0;
    CHECK(enters==403 && leaves==403);
    f=fopen("xbox_kernel.log","r");CHECK(f);
    while(fgets(line,sizeof(line),f)) {
        char *row=strstr(line,"ROW:");if(!row)continue;
        CHECK(sscanf(row,"ROW:%u:%u:END",&i,&j)==2 && i<4 && j<100);
        CHECK(strstr(row,":END\n") && !seen[i][j]);seen[i][j]=1;++count;
    }
    CHECK(!ferror(f));fclose(f);CHECK(count==400);
    puts("PASS: boot ordering, repeated lifecycle and concurrent logging");return 0;
}
