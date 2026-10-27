/* Reproduce a timer reading settings while the renderer caches fullscreen. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
static HANDLE parsing,release_parse,reader_entered;
static char *blocked_read(char *line,int count,FILE *file)
{
    char *result=fgets(line,count,file);
    if(result && strstr(line,"fullscreen")) {
        SetEvent(parsing);
        assert(WaitForSingleObject(release_parse,5000)==WAIT_OBJECT_0);
    }
    return result;
}
#define fgets blocked_read
#include "config/recomp_config.c"
#undef fgets
static int target,fullscreen;
static DWORD WINAPI timer_read(void *unused) {target=recomp_config_int(NULL,"anisotropy",0);return 0;}
static DWORD WINAPI renderer_read(void *unused) {
    SetEvent(reader_entered);fullscreen=recomp_config_bool(NULL,"fullscreen",0);return 0;
}
int main(int argc,char **argv)
{
    assert(argc==2);FILE *file=fopen(argv[1],"w");assert(file);
    fputs("anisotropy = 8\nfullscreen = 1\nframe_cap = 60\n",file);fclose(file);
    assert(_putenv_s("RECOMP_DISPLAY_CONFIG",argv[1])==0);
    parsing=CreateEventW(NULL,TRUE,FALSE,NULL);release_parse=CreateEventW(NULL,TRUE,FALSE,NULL);
    reader_entered=CreateEventW(NULL,TRUE,FALSE,NULL);
    HANDLE first=CreateThread(NULL,0,timer_read,NULL,0,NULL);
    assert(WaitForSingleObject(parsing,5000)==WAIT_OBJECT_0);
    HANDLE second=CreateThread(NULL,0,renderer_read,NULL,0,NULL);
    assert(WaitForSingleObject(reader_entered,5000)==WAIT_OBJECT_0);
    assert(WaitForSingleObject(second,50)==WAIT_TIMEOUT); /* Cannot see a half-parsed table. */
    SetEvent(release_parse);
    assert(WaitForSingleObject(first,5000)==WAIT_OBJECT_0 && WaitForSingleObject(second,5000)==WAIT_OBJECT_0);
    assert(target==8 && fullscreen==1);
    CloseHandle(first);CloseHandle(second);CloseHandle(parsing);CloseHandle(release_parse);CloseHandle(reader_entered);
    puts("PASS: concurrent timer/renderer lookups wait for complete configuration");
    return 0;
}
