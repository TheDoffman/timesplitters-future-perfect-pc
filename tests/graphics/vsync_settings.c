#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
static int fixture_initial,reads;
uint32_t recomp_config_title_id(void){return 0x45410066;}
int recomp_settings_path(uint32_t title,char *out,size_t n){(void)title;return snprintf(out,n,"runtime.conf")>0;}
int recomp_config_bool(const char *env,const char *key,int fallback){assert(!strcmp(env,"TSFP_VSYNC") && !strcmp(key,"vsync") && !fallback);reads++;return fixture_initial;}
#define PC_VSYNC_SETTINGS_IMPLEMENTATION
#include "graphics/pc_vsync_settings.h"
static void contents(char *out,size_t n){FILE *f=fopen("runtime.conf","rb");assert(f);size_t got=fread(out,1,n-1,f);out[got]=0;assert(!ferror(f));fclose(f);}
int main(int argc,char **argv){
    assert(argc==2);fixture_initial=atoi(argv[1]);
    assert(pc_vsync_enabled()==fixture_initial && pc_vsync_enabled()==fixture_initial && reads==1);
    FILE *f=fopen("runtime.conf","wb");assert(f);
    fputs("# user settings\r\nvrr = 1\r\nframe_cap = 60\r\nnative_fps = 240\r\nfuture = retain\r\nvsync = 0\r\nvsync = 0\r\n",f);fclose(f);
    for(int enabled=0;enabled<=1;enabled++){
        assert(pc_vsync_set_enabled(enabled));assert(pc_vsync_enabled()==enabled);
        assert(pc_vsync_present_interval(0)==(unsigned)enabled);
        assert(pc_vsync_present_interval(1)==1);
        char text[512];contents(text,sizeof text);
        assert(strstr(text,"vrr = 1\r\n") && strstr(text,"native_fps = 240\r\n") && strstr(text,"future = retain\r\n"));
        assert(strstr(text,enabled?"vsync = 1\r\nvsync = 1":"vsync = 0\r\nvsync = 0"));
    }
    assert(!pc_vsync_set_enabled(2) && pc_vsync_enabled());
#ifdef _WIN32
    assert(SetFileAttributesA("runtime.conf",FILE_ATTRIBUTE_READONLY));
#else
    assert(!chmod("runtime.conf",0444));
#endif
    assert(!pc_vsync_set_enabled(0) && pc_vsync_enabled());
#ifdef _WIN32
    assert(SetFileAttributesA("runtime.conf",FILE_ATTRIBUTE_NORMAL));
#else
    assert(!chmod("runtime.conf",0644));
#endif
    f=fopen("runtime.conf","wb");assert(f);fwrite("bad\0data",1,8,f);fclose(f);
    assert(!pc_vsync_set_enabled(0) && pc_vsync_enabled());
    assert(!remove("runtime.conf"));assert(pc_vsync_set_enabled(0));
    char text[512];contents(text,sizeof text);assert(!strcmp(text,"vsync = 0\n"));
    assert(reads==1);puts("PASS: saved/live V-sync, failed writes and presentation intervals");return 0;
}
