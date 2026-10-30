#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#ifdef PATH_POSIX_TEST
#undef _WIN32
#endif
#ifdef _WIN32
#include <windows.h>
typedef WCHAR xbox_host_char;
#define GAME "C:/game"
#define SAVE "C:/save"
#define PROFILE "C:\\profile"
#define SEPARATOR '\\'
static WCHAR s_game_dir[MAX_PATH]=L"C:\\game",s_save_dir[MAX_PATH]=L"C:\\save";
#else
typedef int BOOL;
typedef uint32_t DWORD;
typedef char xbox_host_char;
#define MAX_PATH 4096
#define TRUE 1
#define FALSE 0
#define GAME "/game"
#define SAVE "/save"
#define PROFILE "/profile"
#define SEPARATOR '/'
static char s_game_dir[MAX_PATH]="/game",s_save_dir[MAX_PATH]="/save";
#endif
#define CHECK(x) do{if(!(x)){fprintf(stderr,"path host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static int s_initialized,initializations,dirs,watches;
static char active[4096],noted[4096];
static void xbox_path_init(const char *a,const char *b) {CHECK(!a && !b && !s_initialized);s_initialized=1;++initializations;}
static int profile_root_copy(char *out,size_t capacity) {size_t n=strlen(active);if(!n || n>=capacity)return 0;memcpy(out,active,n+1);return 1;}
static uint32_t recomp_config_title_id(void) {return 0x45410066;}
static void xbox_watch_note_path(const char *p) {CHECK(strlen(p)<sizeof(noted));memcpy(noted,p,strlen(p)+1);++watches;}
#ifdef _WIN32
static int make_dir(void *a,const WCHAR *p,void *b) {CHECK(!a && !b && p[0]);++dirs;return 0;}
#define SHCreateDirectoryExW make_dir
#else
static void mkdir_p(const char *p) {CHECK(p[0]);++dirs;}
#endif
#include "guest_kernel_paths.inc"
static void expect(const char *input,const char *base,const char *relative) {
    char wanted[4096];xbox_host_char out[MAX_PATH];size_t i;
    if(!base[0])snprintf(wanted,sizeof(wanted),"%s",relative);
    else if(!relative[0])snprintf(wanted,sizeof(wanted),"%s",base);
    else snprintf(wanted,sizeof(wanted),"%s/%s",base,relative);
    for(i=0;wanted[i];++i)if(wanted[i]=='/' || wanted[i]=='\\')wanted[i]=SEPARATOR;
    CHECK(xbox_translate_path(input,out,MAX_PATH));
#ifdef _WIN32
    {WCHAR wide[MAX_PATH];CHECK(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,wanted,-1,wide,MAX_PATH));if(wcscmp(out,wide))fprintf(stderr,"%s -> %ls wanted %ls\n",input,out,wide);CHECK(!wcscmp(out,wide));CHECK(!wcscmp(out,xbox_LastHostPath()));}
#else
    CHECK(!strcmp(out,wanted));
#endif
}
int main(void) {
    xbox_host_char out[MAX_PATH],before[MAX_PATH];char long_path[4097];int old_dirs,old_watches;
    const char key[]="\\??\\P:",target[]="\\Device\\Harddisk0\\Partition1\\UDATA";
    expect("D:\\pak\\x",GAME,"pak/x");CHECK(initializations==1 && dirs==0 && watches==1 && !strcmp(noted,"D:\\pak\\x"));
    expect("U:\\a",SAVE,"UserData/a");expect("T:\\a",SAVE,"TitleData/a");expect("Z:\\a",SAVE,"Cache/45410066/a");CHECK(dirs==3);
    memcpy(active,PROFILE,sizeof(PROFILE));expect("U:\\a",PROFILE,"UserData/a");expect("T:\\a",SAVE,"TitleData/a");expect("\\Device\\Harddisk0\\Partition1\\UDATA\\a",PROFILE,"UserData/a");
    expect("\\Device\\Harddisk0\\Partition0",SAVE,"Partition0.img");expect("\\Device\\CdRom0",GAME,"");
    expect("unknown\\a","","unknown/a");
    CHECK(tsfp_kernel_symlink_create(key,6,target,sizeof(target)-1)==TSFP_SYMLINK_OK);
    expect("P:\\a",PROFILE,"UserData/a");CHECK(tsfp_kernel_symlink_remove(key,6)==TSFP_SYMLINK_OK);
    memset(out,0x55,sizeof(out));memcpy(before,out,sizeof(out));old_dirs=dirs;old_watches=watches;
    CHECK(!xbox_translate_path("U:\\a",out,2) && !memcmp(out,before,sizeof(out)) && dirs==old_dirs && watches==old_watches);
    memset(long_path,'x',4096);long_path[4096]=0;CHECK(!xbox_translate_path(long_path,out,MAX_PATH) && !memcmp(out,before,sizeof(out)));
    CHECK(!xbox_translate_path(NULL,out,MAX_PATH) && !xbox_translate_path("D:\\x",NULL,MAX_PATH) && !xbox_translate_path("D:\\x",out,0));
#ifdef _WIN32
    memcpy(active,"C:/profile/\xc3\xa9",sizeof("C:/profile/\xc3\xa9"));CHECK(xbox_translate_path("U:\\a",out,MAX_PATH));CHECK(!wcscmp(out,L"C:/profile/\u00e9\\UserData\\a"));
    memcpy(s_game_dir,L"C:\\",sizeof(L"C:\\"));CHECK(xbox_translate_path("\\Device\\CdRom0",out,MAX_PATH) && !wcscmp(out,L"C:\\"));
#endif
    puts("PASS: original host adapter, selected profiles, actual link snapshots, directory/watch boundaries, bounded unchanged output and host encoding");return 0;
}
